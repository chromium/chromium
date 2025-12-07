// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.KeyEvent.KEYCODE_PAGE_DOWN;
import static android.view.KeyEvent.KEYCODE_PAGE_UP;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;

/** Handler for {@link TabKeyEventData} related actions. */
@NullMarked
/*package*/ class TabKeyEventHandler {
    private TabKeyEventHandler() {}

    /**
     * Handles a {@link KEYCODE_PAGE_UP} or {@link KEYCODE_PAGE_DOWN} event by moving the tab
     * specified in the event data forward or backward in the {@link TabModel} by one index.
     *
     * @param eventData The data for the input event.
     * @param filter The {@link TabGroupModelFilter} to apply changes to.
     * @param moveSingleTab If true moves just a single tab rather than the tab's tab group.
     */
    /* package */ static void onPageKeyEvent(
            TabKeyEventData eventData, TabGroupModelFilter filter, boolean moveSingleTab) {
        @TabId int tabId = eventData.tabId;
        TabModel tabModel = filter.getTabModel();
        Tab tab = tabModel.getTabById(tabId);
        if (tab == null) return;

        int keyCode = eventData.keyCode;
        boolean moveForward = keyCode == KEYCODE_PAGE_UP;
        assert moveForward || keyCode == KEYCODE_PAGE_DOWN;

        if (moveSingleTab) {
            int index = tabModel.indexOf(tab);

            // Skip the operation if the move would result in moving the tab outside of its tab
            // group.
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null) {
                int adjacentIndex = moveForward ? index - 1 : index + 1;
                Tab adjacentTab = tabModel.getTabAt(adjacentIndex);
                if (adjacentTab != null && !tabGroupId.equals(adjacentTab.getTabGroupId())) return;
            }

            tabModel.moveTab(tabId, moveForward ? index - 1 : index + 1);
            return;
        }

        // Tab group case: find the adjacent group and then get the index before or after the
        // adjacent group and move there. Note in this context an adjacent group might just be a
        // single tab.
        List<Tab> currentGroup = filter.getRelatedTabList(tabId);
        int adjacentIndex;
        if (moveForward) {
            adjacentIndex = TabGroupUtils.getFirstTabModelIndexForList(tabModel, currentGroup) - 1;
        } else {
            adjacentIndex = TabGroupUtils.getLastTabModelIndexForList(tabModel, currentGroup) + 1;
        }
        Tab adjacentTab = tabModel.getTabAt(adjacentIndex);
        if (adjacentTab == null) return;

        List<Tab> adjacentGroup = filter.getRelatedTabList(adjacentTab.getId());
        int newIndex;
        if (moveForward) {
            newIndex = TabGroupUtils.getFirstTabModelIndexForList(tabModel, adjacentGroup);
        } else {
            newIndex = TabGroupUtils.getLastTabModelIndexForList(tabModel, adjacentGroup);
        }

        filter.moveRelatedTabs(tabId, newIndex);
    }
}
