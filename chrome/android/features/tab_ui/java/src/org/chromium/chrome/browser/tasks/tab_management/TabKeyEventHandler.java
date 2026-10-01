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
     * @param toPrevious If true, moves earlier in the list (up / previous); if false, moves later.
     * @param moveSingleTab If true, moves just a single tab rather than the tab's tab group.
     */
    public static void reorderTab(
            TabModel tabModel, @TabId int tabId, boolean toPrevious, boolean moveSingleTab) {
        Tab tab = tabModel.getTabById(tabId);
        if (tab == null) return;

        Token tabGroupId = tab.getTabGroupId();
        if (moveSingleTab) {
            int index = tabModel.indexOf(tab);
            int adjacentIndex = toPrevious ? index - 1 : index + 1;

            // Skip the operation if the move would result in moving the tab outside of its tab
            // group.
            if (tabGroupId != null) {
                Tab adjacentTab = tabModel.getTabAt(adjacentIndex);
                if (adjacentTab != null && !tabGroupId.equals(adjacentTab.getTabGroupId())) return;
            }

            tabModel.moveTab(tabId, adjacentIndex);
            return;
        }

        if (tabGroupId != null) {
            TabUiUtils.reorderTabGroup(tabModel, tabGroupId, toPrevious);
            return;
        }

        int newIndex =
                TabUiUtils.getIndexPastAdjacentTabOrGroup(tabModel, List.of(tab), toPrevious);
        if (newIndex != TabModel.INVALID_TAB_INDEX) {
            tabModel.moveTab(tabId, newIndex);
        }
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
        boolean toPrevious = keyCode == KEYCODE_PAGE_UP;
        assert toPrevious || keyCode == KEYCODE_PAGE_DOWN;
        reorderTab(tabModel, eventData.tabId, toPrevious, moveSingleTab);
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
