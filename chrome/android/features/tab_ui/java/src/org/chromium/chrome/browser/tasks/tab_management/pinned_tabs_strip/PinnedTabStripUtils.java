// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import android.content.res.Resources;

import androidx.annotation.DimenRes;
import androidx.annotation.Px;
import androidx.core.content.res.ResourcesCompat;
import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/** Utility class for the pinned tabs strip. */
@NullMarked
public class PinnedTabStripUtils {

    /** Returns whether the pinned tabs feature is enabled. */
    public static boolean isPinnedTabsEnabled() {
        return ChromeFeatureList.sAndroidPinnedTabs.isEnabled();
    }

    /** Returns whether the search box should move for pinned tabs. */
    public static boolean isSearchBoxMovementEnabledForPinnedTabs() {
        return isPinnedTabsEnabled()
                && ChromeFeatureList.sAndroidPinnedTabsSearchBoxMovement.getValue();
    }

    /**
     * Helper method to quickly check if two lists of tabs are the same by comparing their sizes and
     * the IDs of the tabs they contain.
     *
     * @param listA The first list of tabs.
     * @param listB The second list of tabs.
     * @return True if the lists are the same, false otherwise.
     */
    static boolean areListsOfTabsSame(List<ListItem> listA, ModelList listB) {
        if (listA.size() != listB.size()) {
            return false;
        }
        for (int i = 0; i < listA.size(); i++) {
            int idA = listA.get(i).model.get(TabProperties.TAB_ID);
            int idB = listB.get(i).model.get(TabProperties.TAB_ID);
            if (idA != idB) {
                return false;
            }
        }
        return true;
    }

    /**
     * Helper method to get the width percentage multiplier for the pinned tab card based on the
     * number of items in the grid layout. This is used to adjust the width of the tab card so that
     * the tab cards can fit within the available space.
     *
     * @param res Resources for accessing dimensions.
     * @param layoutManager The {@link GridLayoutManager} for the RecyclerView.
     * @param itemCount The number of items in the RecyclerView.
     * @return The width percentage multiplier.
     */
    static float getWidthPercentageMultiplier(
            Resources res, GridLayoutManager layoutManager, int itemCount) {
        int spanCount = layoutManager.getSpanCount();
        int rowCount = (int) Math.ceil((double) itemCount / spanCount);
        return getWidthMultiplierForRow(res, rowCount);
    }

    /**
     * Helper method to get the minimum allowed width for a pinned tab strip item in pixels.
     *
     * @param res The resources for accessing dimensions.
     */
    static @Px int getMinAllowedWidthForPinTabStripItemPx(Resources res) {
        return res.getDimensionPixelSize(R.dimen.pinned_tab_strip_item_min_width);
    }

    private static float getWidthMultiplierForRow(Resources res, int rowCount) {
        if (rowCount <= 1) {
            return 1.0f;
        }

        @DimenRes
        int dimenId =
                switch (rowCount) {
                    case 2 -> R.dimen.pinned_tab_strip_item_width_percentage_multiplier_2_rows;
                    case 3 -> R.dimen.pinned_tab_strip_item_width_percentage_multiplier_3_rows;
                    default -> R.dimen
                            .pinned_tab_strip_item_width_percentage_multiplier_more_than_3_rows;
                };
        return ResourcesCompat.getFloat(res, dimenId);
    }
}
