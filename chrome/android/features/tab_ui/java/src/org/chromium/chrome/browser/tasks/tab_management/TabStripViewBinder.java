// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;
import android.support.v4.content.res.ResourcesCompat;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.annotation.Nullable;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab strip.
 */
class TabStripViewBinder {
    /**
     * Partially or fully update the given ViewHolder based on the given model over propertyKey.
     * @param model The model to use.
     * @param group The view group to bind to.
     * @param propertyKey If present, to be used as the key to partially update. If null, a full
     *                    bind is done.
     */
    public static void bind(
            PropertyModel model, ViewGroup group, @Nullable PropertyKey propertyKey) {
        assert group instanceof ViewLookupCachingFrameLayout;
        ViewLookupCachingFrameLayout view = (ViewLookupCachingFrameLayout) group;
        if (propertyKey == null) {
            onBindViewHolder(view, model);
            return;
        }
        if (TabProperties.IS_SELECTED == propertyKey) {
            ImageButton button = (ImageButton) view.fastFindViewById(R.id.tab_strip_item_button);
            view.setForeground(model.get(TabProperties.IS_SELECTED)
                            ? ResourcesCompat.getDrawable(view.getResources(),
                                    R.drawable.tabstrip_selected, view.getContext().getTheme())
                            : null);
            String title = model.get(TabProperties.TITLE);
            if (model.get(TabProperties.IS_SELECTED)) {
                button.setOnClickListener(v -> {
                    model.get(TabProperties.TAB_CLOSED_LISTENER)
                            .run(model.get(TabProperties.TAB_ID));
                });
                button.setContentDescription(view.getContext().getString(
                        R.string.accessibility_tabstrip_btn_close_tab, title));
            } else {
                button.setOnClickListener(v -> {
                    model.get(TabProperties.TAB_SELECTED_LISTENER)
                            .run(model.get(TabProperties.TAB_ID));
                });
                button.setContentDescription(
                        view.getContext().getString(R.string.accessibility_tabstrip_tab, title));
            }
        } else if (TabProperties.FAVICON == propertyKey) {
            Drawable faviconDrawable = model.get(TabProperties.FAVICON);
            ImageButton button = (ImageButton) view.fastFindViewById(R.id.tab_strip_item_button);
            button.setBackgroundResource(R.drawable.tabstrip_favicon_background);
            ViewCompat.setBackgroundTintList(button,
                    AppCompatResources.getColorStateList(view.getContext(),
                            model.get(TabProperties.TABSTRIP_FAVICON_BACKGROUND_COLOR_ID)));
            if (faviconDrawable != null) {
                button.setImageDrawable(faviconDrawable);
            }
        }
    }

    private static void onBindViewHolder(ViewGroup view, PropertyModel item) {
        for (PropertyKey propertyKey : TabProperties.ALL_KEYS_TAB_STRIP) {
            bind(item, view, propertyKey);
        }
    }
}
