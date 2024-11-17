// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab strip. */
class TabStripViewBinder {
    private static final int FAVICON_BACKGROUND_DEFAULT_ALPHA = 255;
    private static final int FAVICON_BACKGROUND_SELECTED_ALPHA = 0;

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
            int selectedDrawableId =
                    model.get(TabProperties.IS_INCOGNITO)
                            ? R.drawable.tab_strip_selected_ring_incognito
                            : R.drawable.tab_strip_selected_ring;
            view.setForeground(
                    model.get(TabProperties.IS_SELECTED)
                            ? ResourcesCompat.getDrawable(
                                    view.getResources(),
                                    selectedDrawableId,
                                    view.getContext().getTheme())
                            : null);

            if (model.get(TabProperties.IS_SELECTED)) {
                button.setOnClickListener(
                        v -> {
                            TabActionButtonData data =
                                    model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
                            assert data.type != TabActionButtonData.TabActionButtonType.OVERFLOW;
                            data.tabActionListener.run(v, model.get(TabProperties.TAB_ID));
                        });
                button.getBackground().setAlpha(FAVICON_BACKGROUND_SELECTED_ALPHA);
            } else {
                button.setOnClickListener(
                        v -> {
                            model.get(TabProperties.TAB_CLICK_LISTENER)
                                    .run(v, model.get(TabProperties.TAB_ID));
                        });
                button.getBackground().setAlpha(FAVICON_BACKGROUND_DEFAULT_ALPHA);
            }
            setContentDescription(view, model);
        } else if (TabProperties.FAVICON_FETCHER == propertyKey) {
            model.set(TabProperties.FAVICON_FETCHED, false);
            TabListFaviconProvider.TabFaviconFetcher fetcher =
                    model.get(TabProperties.FAVICON_FETCHER);
            if (fetcher == null) {
                setFavicon(view, model, null);
                model.set(TabProperties.FAVICON_FETCHED, true);
                return;
            }
            fetcher.fetch(
                    tabFavicon -> {
                        if (fetcher != model.get(TabProperties.FAVICON_FETCHER)) return;

                        setFavicon(view, model, tabFavicon.getDefaultDrawable());
                        model.set(TabProperties.FAVICON_FETCHED, true);
                    });
        } else if (TabProperties.HAS_NOTIFICATION_BUBBLE == propertyKey) {
            ImageView notificationView =
                    (ImageView) view.fastFindViewById(R.id.tab_strip_notification_bubble);

            if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
                int visibility =
                        model.get(TabProperties.HAS_NOTIFICATION_BUBBLE) ? View.VISIBLE : View.GONE;
                notificationView.setVisibility(visibility);
            } else {
                notificationView.setVisibility(View.GONE);
            }
            setContentDescription(view, model);
        }
    }

    /**
     * Handles any cleanup for recycled views that might be expensive to keep around in the pool.
     *
     * @param model The property model to possibly cleanup.
     * @param view The view to possibly cleanup.
     */
    public static void onViewRecycled(PropertyModel model, View view) {
        if (view instanceof ViewLookupCachingFrameLayout frameLayout) {
            // There is a possibility the frameLayout is not for a tab strip item as the strip items
            // don't have a specialized view type. setFavicon will check if an expected child view
            // exists so that we know it is safe to update the model. Otherwise it is possible the
            // PropertyKeys we try to modify might not be applicable to the model that was supplied.
            if (setFavicon(frameLayout, model, /* faviconDrawable= */ null)) {
                model.set(TabProperties.FAVICON_FETCHED, false);
            }
        }
    }

    private static void onBindViewHolder(ViewGroup view, PropertyModel item) {
        for (PropertyKey propertyKey : TabProperties.ALL_KEYS_TAB_STRIP) {
            bind(item, view, propertyKey);
        }
    }

    /** Returns true if the favicon was successfully set. */
    private static boolean setFavicon(
            ViewLookupCachingFrameLayout view, PropertyModel model, Drawable faviconDrawable) {
        @Nullable
        ImageButton button = (ImageButton) view.fastFindViewById(R.id.tab_strip_item_button);
        if (button == null) return false;

        button.setBackgroundResource(
                org.chromium.chrome.browser.tab_ui.R.drawable.tabstrip_favicon_background);

        ViewCompat.setBackgroundTintList(
                button,
                AppCompatResources.getColorStateList(
                        view.getContext(),
                        model.get(TabProperties.IS_INCOGNITO)
                                ? R.color.favicon_background_color_incognito
                                : R.color.favicon_background_color));
        if (!model.get(TabProperties.IS_SELECTED)) {
            button.getBackground().setAlpha(FAVICON_BACKGROUND_DEFAULT_ALPHA);
        } else {
            button.getBackground().setAlpha(FAVICON_BACKGROUND_SELECTED_ALPHA);
        }
        button.setImageDrawable(faviconDrawable);
        return true;
    }

    private static void setContentDescription(
            ViewLookupCachingFrameLayout view, PropertyModel model) {
        Context context = view.getContext();
        ImageButton button = (ImageButton) view.fastFindViewById(R.id.tab_strip_item_button);
        String title = model.get(TabProperties.TITLE);
        @StringRes int contentDescRes;

        if (model.get(TabProperties.IS_SELECTED)) {
            contentDescRes = R.string.accessibility_tabstrip_btn_close_tab;
        } else {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                    && model.get(TabProperties.HAS_NOTIFICATION_BUBBLE)) {
                contentDescRes = R.string.accessibility_tabstrip_tab_notification;
            } else {
                contentDescRes = R.string.accessibility_tabstrip_tab;
            }
        }
        button.setContentDescription(context.getString(contentDescRes, title));
    }
}
