/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <glade/glade.h>
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include <pk-debug.h>
#include <pk-client.h>
#include <pk-enum-list.h>
#include "pk-common.h"

#define PK_CONF_NOTIFY_COMPLETED	"/apps/gnome-packagekit/notify_complete"
#define PK_CONF_NOTIFY_AVAILABLE	"/apps/gnome-packagekit/notify_available"
#define PK_CONF_FIND_AS_TYPE		"/apps/gnome-packagekit/find_as_you_type"
#define PK_CONF_UPDATE_TIMEOUT		"/apps/gnome-packagekit/update_timeout"
#define PK_CONF_UPDATE_CHECK		"/apps/gnome-packagekit/update_check"
#define PK_CONF_AUTO_UPDATE		"/apps/gnome-packagekit/auto_update"

#define PK_FREQ_HOURLY_TEXT		_("Hourly")
#define PK_FREQ_DAILY_TEXT		_("Daily")
#define PK_FREQ_WEEKLY_TEXT		_("Weekly")
#define PK_FREQ_NEVER_TEXT		_("Never")

#define PK_UPDATE_ALL_TEXT		_("All updates")
#define PK_UPDATE_SECURITY_TEXT		_("Only security updates")
#define PK_UPDATE_NONE_TEXT		_("Nothing")

static GladeXML *glade_xml = NULL;

/**
 * pk_button_help_cb:
 **/
static void
pk_button_help_cb (GtkWidget *widget,
		   gboolean  data)
{
	pk_debug ("emitting action-help");
}

/**
 * pk_button_close_cb:
 **/
static void
pk_button_close_cb (GtkWidget *widget, gboolean data)
{
	GMainLoop *loop = (GMainLoop *) data;
	g_main_loop_quit (loop);
	pk_debug ("emitting action-close");
}

/**
 * pk_button_checkbutton_clicked_cb:
 **/
static void
pk_button_checkbutton_clicked_cb (GtkWidget *widget, gpointer data)
{
	gboolean checked;
	GConfClient *client;
	const gchar *gconf_key;

	client = gconf_client_get_default ();
	checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	gconf_key = (const char *) g_object_get_data (G_OBJECT (widget), "gconf_key");
	pk_debug ("Changing %s to %i", gconf_key, checked);
	gconf_client_set_bool (client, gconf_key, checked, NULL);

	g_object_unref (client);
}

/**
 * pk_window_delete_event_cb:
 * @event: The event type, unused.
 **/
static gboolean
pk_window_delete_event_cb (GtkWidget	*widget,
			    GdkEvent	*event,
			    gboolean	 data)
{
	GMainLoop *loop = (GMainLoop *) data;
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * pk_prefs_freq_combo_changed:
 **/
static void
pk_prefs_freq_combo_changed (GtkWidget *widget, gpointer data)
{
	gchar *value;
	const gchar *action;
	PkFreqEnum freq;
	GConfClient *client;

	client = gconf_client_get_default ();
	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (strcmp (value, PK_FREQ_HOURLY_TEXT) == 0) {
		freq = PK_FREQ_ENUM_HOURLY;
	} else if (strcmp (value, PK_FREQ_DAILY_TEXT) == 0) {
		freq = PK_FREQ_ENUM_DAILY;
	} else if (strcmp (value, PK_FREQ_WEEKLY_TEXT) == 0) {
		freq = PK_FREQ_ENUM_WEEKLY;
	} else if (strcmp (value, PK_FREQ_NEVER_TEXT) == 0) {
		freq = PK_FREQ_ENUM_NEVER;
	} else {
		g_assert (FALSE);
	}

	action = pk_freq_enum_to_text (freq);
	pk_debug ("Changing %s to %s", PK_CONF_UPDATE_CHECK, action);
	gconf_client_set_string (client, PK_CONF_UPDATE_CHECK, action, NULL);
	g_free (value);
	g_object_unref (client);
}

/**
 * pk_prefs_update_combo_changed:
 **/
static void
pk_prefs_update_combo_changed (GtkWidget *widget, gpointer data)
{
	gchar *value;
	const gchar *action;
	PkUpdateEnum update;
	GConfClient *client;

	client = gconf_client_get_default ();
	value = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
	if (strcmp (value, PK_UPDATE_ALL_TEXT) == 0) {
		update = PK_UPDATE_ENUM_ALL;
	} else if (strcmp (value, PK_UPDATE_SECURITY_TEXT) == 0) {
		update = PK_UPDATE_ENUM_SECURITY;
	} else if (strcmp (value, PK_UPDATE_NONE_TEXT) == 0) {
		update = PK_UPDATE_ENUM_NONE;
	} else {
		g_assert (FALSE);
	}

	action = pk_update_enum_to_text (update);
	pk_debug ("Changing %s to %s", PK_CONF_AUTO_UPDATE, action);
	gconf_client_set_string (client, PK_CONF_AUTO_UPDATE, action, NULL);
	g_free (value);
	g_object_unref (client);
}

/**
 * pk_prefs_freq_combo_setup:
 **/
static void
pk_prefs_freq_combo_setup (void)
{
	gchar *value;
	gboolean is_writable;
	GtkWidget *widget;
	PkFreqEnum freq;
	GConfClient *client;

	client = gconf_client_get_default ();
	widget = glade_xml_get_widget (glade_xml, "combobox_check");
	is_writable = gconf_client_key_is_writable (client, PK_CONF_UPDATE_CHECK, NULL);
	value = gconf_client_get_string (client, PK_CONF_UPDATE_CHECK, NULL);
	if (value == NULL) {
		pk_error ("invalid schema, please re-install");
	}
	pk_debug ("value from gconf %s", value);
	freq = pk_freq_enum_from_text (value);
	g_free (value);
	g_object_unref (client);

	gtk_widget_set_sensitive (widget, is_writable);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (pk_prefs_freq_combo_changed), NULL);

	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), PK_FREQ_HOURLY_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), PK_FREQ_DAILY_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), PK_FREQ_WEEKLY_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), PK_FREQ_NEVER_TEXT);
	/* we can do this as it's the same order */
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), freq);
}

/**
 * pk_prefs_update_combo_setup:
 **/
static void
pk_prefs_update_combo_setup (void)
{
	gchar *value;
	gboolean is_writable;
	GtkWidget *widget;
	PkUpdateEnum update;
	GConfClient *client;

	client = gconf_client_get_default ();
	widget = glade_xml_get_widget (glade_xml, "combobox_install");
	is_writable = gconf_client_key_is_writable (client, PK_CONF_UPDATE_CHECK, NULL);
	value = gconf_client_get_string (client, PK_CONF_UPDATE_CHECK, NULL);
	if (value == NULL) {
		pk_error ("invalid schema, please re-install");
	}
	pk_debug ("value from gconf %s", value);
	update = pk_update_enum_from_text (value);
	g_free (value);
	g_object_unref (client);

	gtk_widget_set_sensitive (widget, is_writable);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (pk_prefs_update_combo_changed), NULL);

	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), PK_UPDATE_ALL_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), PK_UPDATE_SECURITY_TEXT);
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), PK_UPDATE_NONE_TEXT);
	/* we can do this as it's the same order */
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), update);
}

/**
 * pk_prefs_notify_checkbutton_setup:
 **/
static void
pk_prefs_notify_checkbutton_setup (GtkWidget *widget, const gchar *gconf_key)
{
	GConfClient *client;
	gboolean value;

	client = gconf_client_get_default ();
	value = gconf_client_get_bool (client, gconf_key, NULL);
	pk_debug ("value from gconf %i for %s", value, gconf_key);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

	g_object_set_data (G_OBJECT (widget), "gconf_key", (gpointer) gconf_key);
	g_signal_connect (widget, "clicked", G_CALLBACK (pk_button_checkbutton_clicked_cb), NULL);
	g_object_unref (client);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GMainLoop *loop;
	gboolean verbose = FALSE;
	GOptionContext *context;
	GtkWidget *main_window;
	GtkWidget *widget;
	PkEnumList *role_list;
	PkClient *client;

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_NONE, &verbose,
		  "Show extra debugging information", NULL },
		{ NULL}
	};

	if (! g_thread_supported ()) {
		g_thread_init (NULL);
	}
	dbus_g_thread_init ();
	g_type_init ();

	context = g_option_context_new (_("Software Update Preferences"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);
	pk_debug_init (verbose);
	gtk_init (&argc, &argv);

	loop = g_main_loop_new (NULL, FALSE);

	client = pk_client_new ();

	/* get actions */
	role_list = pk_client_get_actions (client);

	glade_xml = glade_xml_new (PK_DATA "/pk-prefs.glade", NULL, NULL);
	main_window = glade_xml_get_widget (glade_xml, "window_prefs");

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_icon_name (GTK_WINDOW (main_window), "system-installer");

	/* Get the main window quit */
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (pk_window_delete_event_cb), loop);

	widget = glade_xml_get_widget (glade_xml, "checkbutton_notify_updates");
	pk_prefs_notify_checkbutton_setup (widget, PK_CONF_NOTIFY_AVAILABLE);

	widget = glade_xml_get_widget (glade_xml, "checkbutton_notify_completed");
	pk_prefs_notify_checkbutton_setup (widget, PK_CONF_NOTIFY_COMPLETED);

	widget = glade_xml_get_widget (glade_xml, "button_close");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (pk_button_close_cb), loop);
	widget = glade_xml_get_widget (glade_xml, "button_help");
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (pk_button_help_cb), NULL);

	/* update the combo boxes */
	pk_prefs_freq_combo_setup ();
	pk_prefs_update_combo_setup ();

	gtk_widget_show (main_window);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	g_object_unref (client);
	g_object_unref (role_list);

	return 0;
}
