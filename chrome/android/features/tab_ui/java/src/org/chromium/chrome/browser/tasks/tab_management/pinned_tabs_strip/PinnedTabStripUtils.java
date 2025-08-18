// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/** Utility class for the pinned tabs strip. */
@NullMarked
public class PinnedTabStripUtils {

    /**
     * Helper method to quickly check if two lists of tabs are the same by comparing their sizes and
     * the IDs of the tabs they contain.
     *
     * @param listA The first list of tabs.
     * @param listB The second list of tabs.
     * @return True if the lists are the same, false otherwise.
     */
    public static boolean areListsOfTabsSame(List<ListItem> listA, ModelList listB) {
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
}
