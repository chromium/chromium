// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;
import androidx.collection.ArrayMap;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Map;
import java.util.Set;

/** Helper class to handle tab group color related utilities. */
public class TabGroupColorUtils {
    public static final int INVALID_COLOR_ID = -1;
    private static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";
    private static final String MIGRATION_CHECK = "migration_check";
    private static final int MIGRATION_NOT_DONE = 0;
    private static final int MIGRATION_DONE = 1;

    /**
     * This method stores tab group colors with reference to {@code tabRootId}. Package protected as
     * all access should route through the {@link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID which is used as a reference to store group colors.
     * @param color The tab group color {@link TabGroupColorId} to store.
     */
    static void storeTabGroupColor(int tabRootId, int color) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().putInt(String.valueOf(tabRootId), color).apply();
    }

    /**
     * This method deletes a specific stored tab group color with reference to {@code tabRootId}.
     * While currently public, the intent is to make this package protected and force all access to
     * go through the {@Link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group color will be deleted.
     */
    static void deleteTabGroupColor(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * This method fetches tab group colors for the related tab group root ID. While currently
     * public, the intent is to make thisUndo package protected and force all access to go through
     * the {@Link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group color will be fetched.
     * @return The stored color of the target tab group, default value is -1 (INVALID_COLOR_ID).
     */
    static int getTabGroupColor(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        return getSharedPreferences().getInt(String.valueOf(tabRootId), INVALID_COLOR_ID);
    }

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
        if (getSharedPreferences().getInt(MIGRATION_CHECK, MIGRATION_NOT_DONE) == MIGRATION_DONE) {
            return;
        }

        Map<Integer, Integer> currentColorCountMap = getCurrentColorCountMap(tabGroupModelFilter);
        Set<Integer> rootIds = tabGroupModelFilter.getAllTabGroupRootIds();

        // Assign a color to all tab groups that don't have a color.
        for (Integer rootId : rootIds) {
            int colorId = getTabGroupColor(rootId);

            // Retrieve the next suggested colorId if the current tab group does not have a color.
            if (colorId == INVALID_COLOR_ID) {
                int suggestedColorId = getNextSuggestedColorId(currentColorCountMap);
                storeTabGroupColor(rootId, suggestedColorId);
                currentColorCountMap.put(
                        suggestedColorId, currentColorCountMap.get(suggestedColorId) + 1);
            }
        }

        // Mark that the initial migration of tab colors is complete.
        getSharedPreferences().edit().putInt(MIGRATION_CHECK, MIGRATION_DONE).apply();
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
     * This method removes the shared preference file. TODO(b/41490324): Consider removing this when
     * the feature is launched.
     */
    public static void clearTabGroupColorInfo() {
        ContextUtils.getApplicationContext().deleteSharedPreferences(TAB_GROUP_COLORS_FILE_NAME);
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
    }

    /** Get a map that indicates the current usage count of each tab group color. */
    private static Map<Integer, Integer> getCurrentColorCountMap(
            TabGroupModelFilter tabGroupModelFilter) {
        int colorListSize = TabGroupColorId.NUM_ENTRIES;
        Map<Integer, Integer> colorCountMap = new ArrayMap<>(colorListSize);
        for (int i = 0; i < colorListSize; i++) {
            colorCountMap.put(i, 0);
        }

        Set<Integer> rootIds = tabGroupModelFilter.getAllTabGroupRootIds();

        // Filter all tab groups for ones that already have a color assigned.
        for (Integer rootId : rootIds) {
            int colorId = getTabGroupColor(rootId);

            // If the tab group has a color stored on shared prefs, increment the colorId map count.
            if (colorId != INVALID_COLOR_ID) {
                colorCountMap.put(colorId, colorCountMap.get(colorId) + 1);
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
            } else if (entry.getValue() == colorCount) {
                if (entry.getKey() < colorId) {
                    colorId = entry.getKey();
                }
            }
        }

        // Assert that the current color count map exists and sets a valid colorId on loop
        // iteration, otherwise default to an invalid colorId.
        assert colorId != Integer.MAX_VALUE;
        return colorId != Integer.MAX_VALUE ? colorId : INVALID_COLOR_ID;
    }
}
