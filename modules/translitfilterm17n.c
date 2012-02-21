/*
 * Copyright (C) 2012 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libtranslit/translit.h>
#include <m17n.h>
#include <gio/gio.h>

#define TRANSLIT_TYPE_FILTER_M17N (translit_filter_m17n_get_type())
#define TRANSLIT_FILTER_M17N(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRANSLIT_TYPE_FILTER_M17N, TranslitFilterM17n))
#define TRANSLIT_FILTER_M17N_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TRANSLIT_TYPE_FILTER_M17N, TranslitFilterM17nClass))
#define TRANSLIT_IS_FILTER_M17N(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRANSLIT_TYPE_FILTER_M17N))
#define TRANSLIT_IS_FILTER_M17N_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRANSLIT_TYPE_FILTER_M17N))
#define TRANSLIT_FILTER_M17N_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TRANSLIT_TYPE_FILTER_M17N, TranslitFilterM17nClass))

struct _TranslitFilterM17n
{
  TranslitFilter parent;
  MInputMethod *im;
  MInputContext *ic;
  char *language;
  char *name;
  MSymbol symbol;
};

struct _TranslitFilterM17nClass
{
  TranslitFilterClass parent_class;
};

typedef struct _TranslitFilterM17n TranslitFilterM17n;
typedef struct _TranslitFilterM17nClass TranslitFilterM17nClass;

enum
  {
    PROP_0,
    PROP_LANGUAGE,
    PROP_NAME
  };

static void initable_iface_init (GInitableIface *initable_iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (TranslitFilterM17n,
				translit_filter_m17n,
				TRANSLIT_TYPE_FILTER,
				0,
				G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						       initable_iface_init));

static MConverter *utf8_converter;

static char *
mtext_to_utf8 (MText *mt)
{
  char *buf;
  size_t cap;

  mconv_reset_converter (utf8_converter);
  cap = (mtext_len (mt) + 1) * 6;
  buf = (char *) malloc (cap);
  mconv_rebind_buffer (utf8_converter, buf, cap);
  mconv_encode (utf8_converter, mt);

  buf[utf8_converter->nbytes] = '\0';

  return buf;
}

static gboolean
translit_filter_m17n_real_filter (TranslitFilter *self,
				  gchar ascii,
				  TranslitModifierType modifiers)
{
  TranslitFilterM17n *m17n = TRANSLIT_FILTER_M17N (self);
  gchar *name = g_strdup_printf ("%c", ascii); /* FIXME: handle modifiers */
  gint retval;

  m17n->symbol = msymbol (name);
  retval = minput_filter (m17n->ic, m17n->symbol, NULL);
  return retval != 0;
}

static gchar *
translit_filter_m17n_real_poll_output (TranslitFilter *self)
{
  TranslitFilterM17n *m17n = TRANSLIT_FILTER_M17N (self);
  MText *mt;
  gchar *output;
  gint retval;

  g_return_val_if_fail (m17n->symbol, NULL);

  mt = mtext ();
  retval = minput_lookup (m17n->ic, m17n->symbol, NULL, mt);
  if (retval == 0)
    {
      output = mtext_to_utf8 (mt);
      m17n_object_unref (mt);
      return output;
    }
  m17n_object_unref (mt);
  return NULL;
}

static void
translit_filter_m17n_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  TranslitFilterM17n *m17n = TRANSLIT_FILTER_M17N (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      g_free (m17n->language);
      m17n->language = g_value_dup_string (value);
      break;
    case PROP_NAME:
      g_free (m17n->name);
      m17n->name = g_value_dup_string (value);
      break;
    default:
      g_object_set_property (object,
			     g_param_spec_get_name (pspec),
			     value);
      break;
    }
}

static void
translit_filter_m17n_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  TranslitFilterM17n *m17n = TRANSLIT_FILTER_M17N (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      g_value_set_string (value, m17n->language);
      break;
    case PROP_NAME:
      g_value_set_string (value, m17n->name);
      break;
    default:
      g_object_get_property (object,
			     g_param_spec_get_name (pspec),
			     value);
      break;
    }
}

static void
translit_filter_m17n_finalize (GObject *object)
{
  TranslitFilterM17n *m17n = TRANSLIT_FILTER_M17N (object);

  minput_destroy_ic (m17n->ic);
  minput_close_im (m17n->im);
  g_free (m17n->language);
  g_free (m17n->name);

  G_OBJECT_CLASS (translit_filter_m17n_parent_class)->finalize (object);
}

static void
translit_filter_m17n_class_init (TranslitFilterM17nClass *klass)
{
  TranslitFilterClass *filter_class = TRANSLIT_FILTER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  filter_class->filter = translit_filter_m17n_real_filter;
  filter_class->poll_output = translit_filter_m17n_real_poll_output;

  gobject_class->set_property = translit_filter_m17n_set_property;
  gobject_class->get_property = translit_filter_m17n_get_property;
  gobject_class->finalize = translit_filter_m17n_finalize;

  pspec = g_param_spec_string ("language",
			       "language",
			       "Language",
			       NULL,
			       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_LANGUAGE, pspec);

  pspec = g_param_spec_string ("name",
			       "name",
			       "Name",
			       NULL,
			       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (gobject_class, PROP_NAME, pspec);

  M17N_INIT ();
  utf8_converter = mconv_buffer_converter (Mcoding_utf_8, NULL, 0);
}

static void
translit_filter_m17n_class_finalize (TranslitFilterM17nClass *klass)
{
  m17n_object_unref (utf8_converter);
  M17N_FINI ();
}

static void
translit_filter_m17n_init (TranslitFilterM17n *self)
{
  g_object_get (G_OBJECT (self),
		"language", &self->language,
		"name", &self->name,
		NULL);
}

static gboolean
initable_init (GInitable *initable,
	       GCancellable *cancellable,
	       GError **error)
{
  TranslitFilterM17n *m17n = TRANSLIT_FILTER_M17N (initable);

  m17n->im = minput_open_im (msymbol (m17n->language),
			     msymbol (m17n->name),
			     NULL);
  if (m17n->im)
    {
      m17n->ic = minput_create_ic (m17n->im, NULL);
      return TRUE;
    }
  return FALSE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

void
translit_filter_m17n_register (GTypeModule *module)
{
  translit_filter_m17n_register_type (module);
  translit_filter_implement ("m17n", translit_filter_m17n_get_type ());
}
