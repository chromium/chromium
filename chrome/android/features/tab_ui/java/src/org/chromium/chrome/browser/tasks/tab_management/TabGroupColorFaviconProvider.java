// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;

public class TabGroupColorFaviconProvider {
    static final int TAB_GROUP_FAVICON_COLOR_LEVEL = 1;
    static final int FAVICON_BACKGROUND_DEFAULT_ALPHA = 255;
    static final int FAVICON_BACKGROUND_SELECTED_ALPHA = 0;
    private static final int INVALID_COLOR_ID = -1;
    private final Context mContext;

    public TabGroupColorFaviconProvider(Context context) {
        mContext = context;
    }

    /** A favicon represented by a full color circle when displaying tab group colors. */
    @VisibleForTesting
    public static class TabGroupColorFavicon extends TabFavicon {
        private final @org.chromium.components.tab_groups.TabGroupColorId int mColorId;

        private TabGroupColorFavicon(
                @NonNull Drawable defaultDrawable,
                @NonNull Drawable selectedDrawable,
                boolean allowRecolor,
                @org.chromium.components.tab_groups.TabGroupColorId int colorId) {
            super(defaultDrawable, selectedDrawable, allowRecolor);
            mColorId = colorId;
        }

        @VisibleForTesting
        public TabGroupColorFavicon(
                @NonNull Drawable defaultDrawable,
                @org.chromium.components.tab_groups.TabGroupColorId int colorId) {
            this(defaultDrawable, defaultDrawable, false, colorId);
        }

        @Override
        public int hashCode() {
            return Integer.hashCode(mColorId);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof TabGroupColorFavicon)) {
                return false;
            }
            return this.mColorId == ((TabGroupColorFavicon) other).mColorId;
        }
    }

    /**
     * Asynchronously get a full color favicon for tab group color icon display.
     *
     * @param colorId The color id associated with the chosen color to be displayed.
     */
    public TabFaviconFetcher getFaviconFromTabGroupColorFetcher(
            @NonNull @org.chromium.components.tab_groups.TabGroupColorId int colorId,
            boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                if (colorId != INVALID_COLOR_ID) {
                    final @ColorInt int color =
                            ColorPickerUtils.getTabGroupColorPickerItemColor(
                                    mContext, colorId, isIncognito);

                    LayerDrawable tabGroupColorIcon =
                            (LayerDrawable)
                                    ResourcesCompat.getDrawable(
                                            mContext.getResources(),
                                            org.chromium.chrome.tab_ui.R.drawable
                                                    .tab_group_color_icon,
                                            mContext.getTheme());
                    ((GradientDrawable)
                                    tabGroupColorIcon.getDrawable(TAB_GROUP_FAVICON_COLOR_LEVEL))
                            .setColor(color);

                    faviconCallback.onResult(new TabGroupColorFavicon(tabGroupColorIcon, colorId));
                } else {
                    // If the color is invalid, don't set a drawable.
                    faviconCallback.onResult(
                            new TabGroupColorFavicon(
                                    null, org.chromium.components.tab_groups.TabGroupColorId.GREY));
                }
            }
        };
    }
}
