// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.ICON;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.TITLE;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.TITLE_CONDENSED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.IMMERIVE_MODE_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.ZOOM_PERCENT_TEXT;

import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the zoom menu item. */
@NullMarked
public class PageZoomMenuItemViewBinder {
    /** Handles binding the view and models changes. */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == MENU_ITEM_ID) {
            int id = model.get(MENU_ITEM_ID);
            assert id == R.id.page_zoom_id;
            view.setId(id);
        } else if (key == TITLE) {
            TextViewWithCompoundDrawables titleView = view.findViewById(R.id.title);
            titleView.setText(model.get(TITLE));
        } else if (key == TITLE_CONDENSED) {
            CharSequence titleCondensed = model.get(TITLE_CONDENSED);
            view.findViewById(R.id.title).setContentDescription(titleCondensed);
        } else if (key == ICON) {
            TextViewWithCompoundDrawables titleView = view.findViewById(R.id.title);
            titleView.setCompoundDrawablesRelative(
                    model.get(ICON), /* top= */ null, /* end= */ null, /* bottom= */ null);
        } else if (key == INCREASE_ZOOM_CALLBACK) {
            View zoomInButton = view.findViewById(R.id.zoom_in_button);
            zoomInButton.setOnClickListener(v -> model.get(INCREASE_ZOOM_CALLBACK).run());
        } else if (key == DECREASE_ZOOM_CALLBACK) {
            View zoomOutButton = view.findViewById(R.id.zoom_out_button);
            zoomOutButton.setOnClickListener(v -> model.get(DECREASE_ZOOM_CALLBACK).run());
        } else if (key == INCREASE_ZOOM_ENABLED) {
            ImageButton zoomInButton = view.findViewById(R.id.zoom_in_button);
            zoomInButton.setEnabled(model.get(INCREASE_ZOOM_ENABLED));
            zoomInButton.setFocusable(model.get(INCREASE_ZOOM_ENABLED));
        } else if (key == DECREASE_ZOOM_ENABLED) {
            ImageButton zoomOutButton = view.findViewById(R.id.zoom_out_button);
            zoomOutButton.setEnabled(model.get(DECREASE_ZOOM_ENABLED));
            zoomOutButton.setFocusable(model.get(DECREASE_ZOOM_ENABLED));
        } else if (key == ZOOM_PERCENT_TEXT) {
            ((TextView) view.findViewById(R.id.zoom_percentage))
                    .setText(model.get(ZOOM_PERCENT_TEXT));
        } else if (key == IMMERIVE_MODE_CALLBACK) {
            View immersiveModeButton = view.findViewById(R.id.fullscreen_button);
            immersiveModeButton.setOnClickListener(v -> model.get(IMMERIVE_MODE_CALLBACK).run());
        }
    }
}
