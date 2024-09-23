// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.content.res.TypedArray;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuUtil;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageView;

/** A custom binder used to bind the incognito menu item. */
class IncognitoMenuItemViewBinder implements CustomViewBinder {
    private static final int INCOGNITO_ITEM_VIEW_TYPE = 0;

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return id == R.id.new_incognito_tab_menu_id
                ? INCOGNITO_ITEM_VIEW_TYPE
                : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public int getLayoutId(int viewType) {
        if (viewType == INCOGNITO_ITEM_VIEW_TYPE) {
            return R.layout.custom_view_menu_item;
        }
        return CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public void bind(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            assert id == R.id.new_incognito_tab_menu_id;
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

    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    @Override
    public int getPixelHeight(Context context) {
        TypedArray a =
                context.obtainStyledAttributes(
                        new int[] {android.R.attr.listPreferredItemHeightSmall});
        return a.getDimensionPixelSize(0, 0);
    }
}
