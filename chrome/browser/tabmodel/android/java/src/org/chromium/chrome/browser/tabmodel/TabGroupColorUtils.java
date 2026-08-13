// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Helper class to handle tab group color related utilities. */
@NullMarked
public class TabGroupColorUtils {
    public static final int INVALID_COLOR_ID = -1;

    /**
     * This method returns the next suggested colorId to be assigned to a tab group if that tab
     * group has no color assigned to it. This algorithm uses a key-value map to store all usage
     * counts of the current list of colors in other tab groups. It will select the least used color
     * that appears first in the color list. The suggested color value should be a color id of type
     * {@link TabGroupColorId}.
     *
     * @param tabModel The {@link TabModel} that governs all tab groups.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static int getNextSuggestedColorId(TabModel tabModel) {
        // Generate the currentColorCountMap.
        Map<Integer, Integer> currentColorCountMap = getCurrentColorCountMap(tabModel);
        return getNextSuggestedColorId(currentColorCountMap);
    }

    /** Get a map that indicates the current usage count of each tab group color. */
    private static Map<Integer, Integer> getCurrentColorCountMap(TabModel tabModel) {
        List<Integer> colorList = TabGroupColorPickerUtils.getTabGroupColorIdList();
        Map<Integer, Integer> colorCountMap = new LinkedHashMap<>(colorList.size());
        for (Integer colorId : colorList) {
            colorCountMap.put(colorId, 0);
        }

        Set<Token> tabGroupIds = tabModel.getAllTabGroupIds();

        // Filter all tab groups for ones that already have a color assigned.
        for (Token tabGroupId : tabGroupIds) {
            int colorId = tabModel.getTabGroupColor(tabGroupId);

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
