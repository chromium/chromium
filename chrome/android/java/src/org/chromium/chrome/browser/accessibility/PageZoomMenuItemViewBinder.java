// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.HOVER_LISTENER;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.ICON;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.TITLE;
import static org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties.TITLE_CONDENSED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.DECREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.IMMERIVE_MODE_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.IMMERIVE_MODE_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_CALLBACK;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.INCREASE_ZOOM_ENABLED;
import static org.chromium.components.browser_ui.accessibility.PageZoomProperties.ZOOM_PERCENT_TEXT;

import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the zoom menu item. */
@NullMarked
public class PageZoomMenuItemViewBinder {
    private PageZoomMenuItemViewBinder() {}

    /** Handles binding the view and models changes. */
    public static void bind(PropertyModel model, PageZoomMenuItemView view, PropertyKey key) {
        if (key == MENU_ITEM_ID) {
            int id = model.get(MENU_ITEM_ID);
            assert id == R.id.page_zoom_id;
            view.setId(id);
        } else if (key == TITLE || key == TITLE_CONDENSED) {
            CharSequence title = model.get(TITLE);
            if (TextUtils.isEmpty(title)) {
                title = model.get(TITLE_CONDENSED);
            }
            view.setTitle(title, model.get(TITLE_CONDENSED));
        } else if (key == ICON) {
            view.setIcon(model.get(ICON));
        } else if (key == INCREASE_ZOOM_CALLBACK) {
            view.setZoomInCallback(model.get(INCREASE_ZOOM_CALLBACK));
        } else if (key == DECREASE_ZOOM_CALLBACK) {
            view.setZoomOutCallback(model.get(DECREASE_ZOOM_CALLBACK));
        } else if (key == INCREASE_ZOOM_ENABLED) {
            view.setZoomInEnabled(model.get(INCREASE_ZOOM_ENABLED));
        } else if (key == DECREASE_ZOOM_ENABLED) {
            view.setZoomOutEnabled(model.get(DECREASE_ZOOM_ENABLED));
        } else if (key == ZOOM_PERCENT_TEXT) {
            view.setZoomPercentText(model.get(ZOOM_PERCENT_TEXT));
        } else if (key == IMMERIVE_MODE_ENABLED) {
            view.setFullscreenEnabled(model.get(IMMERIVE_MODE_ENABLED));
        } else if (key == IMMERIVE_MODE_CALLBACK) {
            view.setFullscreenCallback(model.get(IMMERIVE_MODE_CALLBACK));
        } else if (key == HOVER_LISTENER) {
            view.setOnHoverListener(model.get(HOVER_LISTENER));
        }
    }
}
