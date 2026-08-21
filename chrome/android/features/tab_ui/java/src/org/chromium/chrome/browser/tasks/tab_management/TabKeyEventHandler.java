// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.KeyEvent.KEYCODE_PAGE_DOWN;
import static android.view.KeyEvent.KEYCODE_PAGE_UP;

import android.view.KeyEvent;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;

/** Handler for {@link TabKeyEventData} related actions. */
@NullMarked
public class TabKeyEventHandler {
    private TabKeyEventHandler() {}

    /**
     * Reorders the tab (or its tab group) forward (previous/up) or backward (next/down) in the
     * {@link TabModel}.
     *
     * @param tabModel The {@link TabModel} to apply changes to.
     * @param tabId The ID of the tab to move.
     * @param moveForward If true, moves earlier in the list (up / previous); if false, moves later.
     * @param moveSingleTab If true, moves just a single tab rather than the tab's tab group.
     */
    public static void reorderTab(
            TabModel tabModel, @TabId int tabId, boolean moveForward, boolean moveSingleTab) {
        Tab tab = tabModel.getTabById(tabId);
        if (tab == null) return;

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
        List<Tab> currentGroup = tabModel.getRelatedTabList(tabId);
        int adjacentIndex;
        if (moveForward) {
            adjacentIndex = TabGroupUtils.getFirstTabModelIndexForList(tabModel, currentGroup) - 1;
        } else {
            adjacentIndex = TabGroupUtils.getLastTabModelIndexForList(tabModel, currentGroup) + 1;
        }
        Tab adjacentTab = tabModel.getTabAt(adjacentIndex);
        if (adjacentTab == null) return;

        List<Tab> adjacentGroup = tabModel.getRelatedTabList(adjacentTab.getId());
        int newIndex;
        if (moveForward) {
            newIndex = TabGroupUtils.getFirstTabModelIndexForList(tabModel, adjacentGroup);
        } else {
            newIndex = TabGroupUtils.getLastTabModelIndexForList(tabModel, adjacentGroup);
        }

        tabModel.moveRelatedTabs(tabId, newIndex);
    }

    /**
     * Handles a {@link KeyEvent#KEYCODE_PAGE_UP} or {@link KeyEvent#KEYCODE_PAGE_DOWN} event by
     * moving the tab specified in the event data forward or backward in the {@link TabModel} by one
     * index.
     *
     * @param eventData The data for the input event.
     * @param tabModel The {@link TabModel} to apply changes to.
     * @param moveSingleTab If true, moves just a single tab rather than the tab's tab group.
     */
    public static void onPageKeyEvent(
            TabKeyEventData eventData, TabModel tabModel, boolean moveSingleTab) {
        int keyCode = eventData.keyCode;
        boolean moveForward = keyCode == KEYCODE_PAGE_UP;
        assert moveForward || keyCode == KEYCODE_PAGE_DOWN;
        reorderTab(tabModel, eventData.tabId, moveForward, moveSingleTab);
    }

    /** Returns whether the given {@link KeyEvent} is a Ctrl+Up or Ctrl+Down reorder event. */
    public static boolean isCtrlDpadReorderEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        return event.isCtrlPressed()
                && (keyCode == KeyEvent.KEYCODE_DPAD_UP || keyCode == KeyEvent.KEYCODE_DPAD_DOWN);
    }

    /** Returns whether the reorder key event moves to previous (up) vs next (down). */
    public static boolean isMovePrevious(KeyEvent event) {
        return event.getKeyCode() == KeyEvent.KEYCODE_DPAD_UP;
    }
}
