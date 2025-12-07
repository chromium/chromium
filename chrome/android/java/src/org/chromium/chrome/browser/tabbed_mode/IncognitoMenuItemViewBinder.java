// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageView;

/** A custom binder used to bind the incognito menu item. */
@NullMarked
class IncognitoMenuItemViewBinder {
    /** Handles binding the view and models changes. */
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            assert id == R.id.new_incognito_tab_menu_id || id == R.id.new_incognito_window_menu_id;
            view.setId(id);
        } else if (key == AppMenuItemProperties.MANAGED) {
            ChromeImageView image = view.findViewById(R.id.trailing_icon);
            if (model.get(AppMenuItemProperties.MANAGED)) {
                image.setImageResource(R.drawable.ic_business);
                image.setVisibility(View.VISIBLE);
            } else {
                image.setVisibility(View.GONE);
            }
        } else if (key == AppMenuItemProperties.TITLE) {
            ((TextViewWithCompoundDrawables) view.findViewById(R.id.title))
                    .setText(model.get(AppMenuItemProperties.TITLE));
        } else if (key == AppMenuItemProperties.TITLE_CONDENSED) {
            CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
            view.findViewById(R.id.title).setContentDescription(titleCondensed);
        } else if (key == AppMenuItemProperties.ICON) {
            ((TextViewWithCompoundDrawables) view.findViewById(R.id.title))
                    .setCompoundDrawablesRelative(
                            model.get(AppMenuItemProperties.ICON), null, null, null);
        } else if (key == AppMenuItemProperties.ENABLED) {
            boolean enabled = model.get(AppMenuItemProperties.ENABLED);
            TextViewWithCompoundDrawables title = view.findViewById(R.id.title);
            title.setEnabled(enabled);
            // Setting |title| to non-focusable will allow TalkBack highlighting the whole view
            // of the menu item, not just title text.
            title.setFocusable(false);
            view.setFocusable(enabled);
        } else if (key == AppMenuItemProperties.HIGHLIGHTED) {
            if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
                ViewHighlighter.turnOnHighlight(
                        view, new HighlightParams(HighlightShape.RECTANGLE));
            } else {
                ViewHighlighter.turnOffHighlight(view);
            }
        } else if (key == AppMenuItemProperties.CLICK_HANDLER) {
            view.setOnClickListener(
                    v -> model.get(AppMenuItemProperties.CLICK_HANDLER).onItemClick(model));
        }
    }
}
