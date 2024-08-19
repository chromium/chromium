// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.VectorDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.DrawableCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

public class TabGroupColorFaviconProvider {
    static final String TAG = "TabGroupColorFaviconProvider";
    static final int TAB_GROUP_FAVICON_COLOR_LEVEL = 1;
    static final int TAB_GROUP_FAVICON_SHARE_ICON_LEVEL = 2;
    static final int FAVICON_BACKGROUND_DEFAULT_ALPHA = 255;
    static final int FAVICON_BACKGROUND_SELECTED_ALPHA = 0;
    private final Context mContext;

    private TabGroupSyncService mTabGroupSyncService;

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

        @VisibleForTesting
        public TabGroupColorFavicon(
                @NonNull Drawable defaultDrawable,
                @NonNull Drawable selectedDrawable,
                @org.chromium.components.tab_groups.TabGroupColorId int colorId) {
            this(defaultDrawable, selectedDrawable, false, colorId);
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
     * Asynchronously get a full color favicon for tab group color icon display. A share group icon
     * based on the user's dynamic color scheme is shown beside the full color favicon if tab group
     * sharing is in use for the tab group in question.
     *
     * @param colorId The color id associated with the chosen color to be displayed.
     */
    public TabFaviconFetcher getFaviconFromTabGroupColorFetcher(
            @NonNull @org.chromium.components.tab_groups.TabGroupColorId int colorId,
            TabModel tabModel,
            Tab tab) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> faviconCallback) {
                if (colorId != TabGroupColorUtils.INVALID_COLOR_ID) {
                    boolean isTabGroupShareActive = isTabGroupShareActive(tabModel, tab);
                    final @ColorInt int color =
                            ColorPickerUtils.getTabGroupColorPickerItemColor(
                                    mContext, colorId, tabModel.isIncognitoBranded());
                    final @DrawableRes int iconRes =
                            isTabGroupShareActive && !tabModel.isIncognitoBranded()
                                    ? org.chromium.chrome.tab_ui.R.drawable
                                            .tab_group_share_with_color_icon
                                    : org.chromium.chrome.tab_ui.R.drawable.tab_group_color_icon;

                    LayerDrawable tabGroupColorIcon =
                            (LayerDrawable)
                                    ResourcesCompat.getDrawable(
                                            mContext.getResources(), iconRes, mContext.getTheme());
                    ((GradientDrawable)
                                    tabGroupColorIcon.getDrawable(TAB_GROUP_FAVICON_COLOR_LEVEL))
                            .setColor(color);

                    if (isTabGroupShareActive && !tabModel.isIncognitoBranded()) {
                        LayerDrawable tabGroupColorIconSelected =
                                (LayerDrawable)
                                        tabGroupColorIcon.getConstantState().newDrawable().mutate();
                        VectorDrawable shareGroupDrawable =
                                (VectorDrawable)
                                        tabGroupColorIconSelected.getDrawable(
                                                TAB_GROUP_FAVICON_SHARE_ICON_LEVEL);
                        DrawableCompat.setTintList(
                                shareGroupDrawable,
                                ColorStateList.valueOf(
                                        MaterialColors.getColor(
                                                mContext, R.attr.colorPrimary, TAG)));

                        faviconCallback.onResult(
                                new TabGroupColorFavicon(
                                        tabGroupColorIcon, tabGroupColorIconSelected, colorId));
                        return;
                    }

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

    private boolean isTabGroupShareActive(TabModel tabModel, Tab tab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
            return false;
        }

        if (TabGroupSyncFeatures.isTabGroupSyncEnabled(tab.getProfile())) {
            mTabGroupSyncService =
                    TabGroupSyncServiceFactory.getForProfile(tab.getProfile().getOriginalProfile());
        }

        if (tabModel == null || mTabGroupSyncService == null) {
            return false;
        }

        final @Nullable String collaborationId =
                TabShareUtils.getCollaborationIdOrNull(tab.getId(), tabModel, mTabGroupSyncService);
        return TabShareUtils.isCollaborationIdValid(collaborationId);
    }
}
