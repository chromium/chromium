// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.FAVICON_BACKGROUND_DEFAULT_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.FAVICON_BACKGROUND_SELECTED_ALPHA;

import android.graphics.drawable.Drawable;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.view.ViewCompat;

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
            int selectedDrawableId = model.get(TabProperties.IS_INCOGNITO)
                    ? R.drawable.tab_strip_selected_ring_incognito
                    : R.drawable.tab_strip_selected_ring;
            view.setForeground(model.get(TabProperties.IS_SELECTED)
                            ? ResourcesCompat.getDrawable(view.getResources(), selectedDrawableId,
                                    view.getContext().getTheme())
                            : null);

            String title = model.get(TabProperties.TITLE);
            if (model.get(TabProperties.IS_SELECTED)) {
                button.setOnClickListener(v -> {
                    model.get(TabProperties.TAB_CLOSED_LISTENER)
                            .run(model.get(TabProperties.TAB_ID));
                });
                button.setContentDescription(view.getContext().getString(
                        R.string.accessibility_tabstrip_btn_close_tab, title));
                button.getBackground().setAlpha(FAVICON_BACKGROUND_SELECTED_ALPHA);
            } else {
                button.setOnClickListener(v -> {
                    model.get(TabProperties.TAB_SELECTED_LISTENER)
                            .run(model.get(TabProperties.TAB_ID));
                });
                button.setContentDescription(
                        view.getContext().getString(R.string.accessibility_tabstrip_tab, title));
                button.getBackground().setAlpha(FAVICON_BACKGROUND_DEFAULT_ALPHA);
            }
        } else if (TabProperties.FAVICON == propertyKey) {
            if (TabUiFeatureUtilities.ENABLE_DEFERRED_FAVICON.getValue()) return;

            Drawable favicon = model.get(TabProperties.FAVICON).getDefaultDrawable();
            setFavicon(view, model, favicon);
        } else if (TabProperties.FAVICON_FETCHER == propertyKey) {
            if (!TabUiFeatureUtilities.ENABLE_DEFERRED_FAVICON.getValue()) return;

            model.set(TabProperties.FAVICON_FETCHED, false);
            TabListFaviconProvider.TabFaviconFetcher fetcher =
                    model.get(TabProperties.FAVICON_FETCHER);
            if (fetcher == null) {
                setFavicon(view, model, null);
                model.set(TabProperties.FAVICON_FETCHED, true);
                return;
            }
            fetcher.fetch(tabFavicon -> {
                if (fetcher != model.get(TabProperties.FAVICON_FETCHER)) return;

                setFavicon(view, model, tabFavicon.getDefaultDrawable());
                model.set(TabProperties.FAVICON_FETCHED, true);
            });
        }
    }

    private static void onBindViewHolder(ViewGroup view, PropertyModel item) {
        for (PropertyKey propertyKey : TabProperties.ALL_KEYS_TAB_STRIP) {
            bind(item, view, propertyKey);
        }
    }

    private static void setFavicon(
            ViewLookupCachingFrameLayout view, PropertyModel model, Drawable faviconDrawable) {
        ImageButton button = (ImageButton) view.fastFindViewById(R.id.tab_strip_item_button);
        button.setBackgroundResource(R.drawable.tabstrip_favicon_background);
        ViewCompat.setBackgroundTintList(button,
                AppCompatResources.getColorStateList(view.getContext(),
                        model.get(TabProperties.TABSTRIP_FAVICON_BACKGROUND_COLOR_ID)));
        if (!model.get(TabProperties.IS_SELECTED)) {
            button.getBackground().setAlpha(FAVICON_BACKGROUND_DEFAULT_ALPHA);
        } else {
            button.getBackground().setAlpha(FAVICON_BACKGROUND_SELECTED_ALPHA);
        }
        button.setImageDrawable(faviconDrawable);
    }
}
