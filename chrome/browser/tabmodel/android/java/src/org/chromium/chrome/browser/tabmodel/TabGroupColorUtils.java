// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Helper class to handle tab group color related utilities. */
@NullMarked
public class TabGroupColorUtils {
    public static final int INVALID_COLOR_ID = -1;

    /**
     * This method assigns a color to all tab groups which do not have an assigned tab color at
     * startup. If a migration for all existing tabs has already been performed, skip this logic.
     *
     * @param tabGroupModelFilter The {@TabGroupModelFilter} that governs the current tab groups.
     */
    public static void assignTabGroupColorsIfApplicable(TabGroupModelFilter tabGroupModelFilter) {
        // TODO(b/41490324): Consider removing the migration logic when tab group colors are
        // launched. There may be an argument to keep this around in case the color info is somehow
        // lost between startups, in which case this will at least set some default colors up. In
        // theory, once the migrations have been applied to everyone there won't be a need for this.
        //
        // If the migration is already done, skip the below logic.
        if (TabGroupVisualDataStore.isColorInitialMigrationDone()) {
            return;
        }

        Map<Integer, Integer> currentColorCountMap = getCurrentColorCountMap(tabGroupModelFilter);
        Set<Token> tabGroupIds = tabGroupModelFilter.getAllTabGroupIds();

        // Assign a color to all tab groups that don't have a color.
        for (Token tabGroupId : tabGroupIds) {
            int colorId = tabGroupModelFilter.getTabGroupColor(tabGroupId);

            // Retrieve the next suggested colorId if the current tab group does not have a color.
            if (colorId == INVALID_COLOR_ID) {
                int suggestedColorId = getNextSuggestedColorId(currentColorCountMap);
                tabGroupModelFilter.setTabGroupColor(tabGroupId, suggestedColorId);
                currentColorCountMap.put(
                        suggestedColorId,
                        assumeNonNull(currentColorCountMap.get(suggestedColorId)) + 1);
            }
        }

        // Mark that the initial migration of tab colors is complete.
        TabGroupVisualDataStore.setColorInitialMigrationDone();
    }

    /**
     * This method returns the next suggested colorId to be assigned to a tab group if that tab
     * group has no color assigned to it. This algorithm uses a key-value map to store all usage
     * counts of the current list of colors in other tab groups. It will select the least used color
     * that appears first in the color list. The suggested color value should be a color id of type
     * {@link TabGroupColorId}.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} that governs all tab groups.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static int getNextSuggestedColorId(TabGroupModelFilter tabGroupModelFilter) {
        // Generate the currentColorCountMap.
        Map<Integer, Integer> currentColorCountMap = getCurrentColorCountMap(tabGroupModelFilter);
        return getNextSuggestedColorId(currentColorCountMap);
    }

    /**
     * This method returns the color id list attributed to tab groups specifically.
     *
     * @return An array list of ids from 0 to n representing all colors in the palette
     */
    public static List<Integer> getTabGroupColorIdList() {
        // The color ids used here can be found in {@link TabGroupColorId}. Note that it is assumed
        // the id list is contiguous from 0 to size-1.
        List<Integer> colors = new ArrayList<>(TabGroupColorId.NUM_ENTRIES);
        for (int i = 0; i < TabGroupColorId.NUM_ENTRIES; i++) {
            colors.add(i);
        }
        return colors;
    }

    /** Get a map that indicates the current usage count of each tab group color. */
    private static Map<Integer, Integer> getCurrentColorCountMap(
            TabGroupModelFilter tabGroupModelFilter) {
        List<Integer> colorList = getTabGroupColorIdList();
        Map<Integer, Integer> colorCountMap = new LinkedHashMap<>(colorList.size());
        for (Integer colorId : colorList) {
            colorCountMap.put(colorId, 0);
        }

        Set<Token> tabGroupIds = tabGroupModelFilter.getAllTabGroupIds();

        // Filter all tab groups for ones that already have a color assigned.
        for (Token tabGroupId : tabGroupIds) {
            int colorId = tabGroupModelFilter.getTabGroupColor(tabGroupId);

            // If the tab group has a color stored on shared prefs, increment the colorId map count.
            if (colorId != INVALID_COLOR_ID) {
                colorCountMap.put(colorId, assumeNonNull(colorCountMap.get(colorId)) + 1);
            }
        }

        return colorCountMap;
    }

    /** Impl of getNextSuggestedColorId which assumes a currentColorCountMap has been created. */
    private static int getNextSuggestedColorId(Map<Integer, Integer> currentColorCountMap) {
        int colorId = Integer.MAX_VALUE;
        int colorCount = Integer.MAX_VALUE;

        for (Map.Entry<Integer, Integer> entry : currentColorCountMap.entrySet()) {
            if (entry.getValue() < colorCount) {
                colorCount = entry.getValue();
                colorId = entry.getKey();
            }
        }

        // Assert that the current color count map exists and sets a valid colorId on loop
        // iteration, otherwise default to an invalid colorId.
        assert colorId != Integer.MAX_VALUE;
        return colorId != Integer.MAX_VALUE ? colorId : INVALID_COLOR_ID;
    }
}
