// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.KeyEvent;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.HashSet;
import java.util.Set;
import java.util.function.Supplier;

/**
 * Helper class for coordinating tab multi-selection logic across the Horizontal Tab Strip and
 * Vertical Tabs.
 *
 * <p>This centralizes the algorithms for modifier click handling (Ctrl/Meta+Click toggle,
 * Shift+Click range selection, active tab handover, and anchor management), ensuring identical
 * selection semantics across all desktop and tablet tab strip implementations.
 */
@NullMarked
public class TabMultiSelectHelper {
    private final Supplier<@Nullable TabModel> mTabModelSupplier;
    private final Callback<Integer> mSelectTabCallback;
    private int mAnchorTabId = Tab.INVALID_TAB_ID;

    /**
     * Constructs a new {@link TabMultiSelectHelper}.
     *
     * @param tabModelSupplier Supplier providing the active {@link TabModel}.
     * @param selectTabCallback Callback to invoke when a tab should be made active.
     */
    public TabMultiSelectHelper(
            Supplier<@Nullable TabModel> tabModelSupplier, Callback<Integer> selectTabCallback) {
        mTabModelSupplier = tabModelSupplier;
        mSelectTabCallback = selectTabCallback;
    }

    /**
     * Returns whether the tab model currently has multiple tabs selected.
     *
     * @param tabModel The {@link TabModel} to inspect.
     * @return true if more than one tab is currently selected in the model, false otherwise.
     */
    public static boolean hasMultipleTabsSelected(@Nullable TabModel tabModel) {
        return tabModel != null && tabModel.getMultiSelectedTabsCount() > 1;
    }

    /**
     * Handles a tab click event with modifier keys.
     *
     * @param clickedTabId The ID of the tab that was clicked.
     * @param modifiers The active keyboard modifiers from the {@link
     *     android.view.MotionEvent#getMetaState()}.
     * @return true if the click was handled as a multi-selection modifier click (Shift or Ctrl), or
     *     false if it was a normal click (meaning selection was cleared and caller should proceed
     *     with standard single-tab selection).
     */
    public boolean handleTabClick(int clickedTabId, int modifiers) {
        // Force flags are required for testing on an emulator, as key presses don't seem to be
        // propagated to the app.
        boolean isShiftPressed = (modifiers & KeyEvent.META_SHIFT_ON) != 0;
        boolean isCtrlPressed = (modifiers & (KeyEvent.META_CTRL_ON | KeyEvent.META_META_ON)) != 0;

        if (isShiftPressed && isCtrlPressed) {
            handleShiftClick(clickedTabId, /* isDestructive= */ false);
            return true;
        } else if (isShiftPressed) {
            handleShiftClick(clickedTabId, /* isDestructive= */ true);
            return true;
        } else if (isCtrlPressed) {
            handleCtrlClick(clickedTabId);
            return true;
        } else {
            // Clear multi-selection and anchor tab.
            clearMultiSelection(/* clearAnchor= */ true, /* notifyObservers= */ true);
            return false;
        }
    }

    /**
     * Handles a Ctrl+Click event, which toggles the selection state of a single tab. If the tab is
     * already in the multi-selection set, it is removed; otherwise, it is added. If the tab is
     * added to the selection, it is also set as the active tab.
     *
     * @param clickedTabId The ID of the tab that was clicked.
     */
    private void handleCtrlClick(int clickedTabId) {
        TabModel tabModel = mTabModelSupplier.get();
        if (tabModel == null
                || clickedTabId == Tab.INVALID_TAB_ID
                || tabModel.getTabById(clickedTabId) == null) {
            return;
        }

        // If the tab is already multi-selected, ctrl click should unselect it.
        if (tabModel.isTabMultiSelected(clickedTabId)) {
            if (clickedTabId == TabModelUtils.getCurrentTabId(tabModel)) {
                handleSelectedTabCtrlClicked(clickedTabId);
                return;
            }
            tabModel.setTabsMultiSelected(Set.of(clickedTabId), /* isSelected= */ false);
        } else {
            int oldSelectedTabId = TabModelUtils.getCurrentTabId(tabModel);
            // Select clicked tab.
            mSelectTabCallback.onResult(clickedTabId);
            // When Ctrl clicked, even the previous tab gets selected.
            if (oldSelectedTabId != Tab.INVALID_TAB_ID && oldSelectedTabId != clickedTabId) {
                tabModel.setTabsMultiSelected(
                        Set.of(clickedTabId, oldSelectedTabId), /* isSelected= */ true);
            } else {
                tabModel.setTabsMultiSelected(Set.of(clickedTabId), /* isSelected= */ true);
            }
            // Clear anchor tab.
            mAnchorTabId = Tab.INVALID_TAB_ID;
        }
    }

    /**
     * Handles a Shift+Click event, which selects a range of tabs from an anchor tab to the clicked
     * tab.
     *
     * @param clickedTabId The ID of the tab that was clicked, representing the endpoint of the
     *     range.
     * @param isDestructive If true, any existing multi-selection is cleared before the new range is
     *     selected. If false, the new range is added to the existing selection.
     */
    private void handleShiftClick(int clickedTabId, boolean isDestructive) {
        TabModel tabModel = mTabModelSupplier.get();
        if (tabModel == null
                || clickedTabId == Tab.INVALID_TAB_ID
                || tabModel.getTabById(clickedTabId) == null) {
            return;
        }

        if (isDestructive) {
            clearMultiSelection(/* clearAnchor= */ false, /* notifyObservers= */ false);
        }
        if (mAnchorTabId == Tab.INVALID_TAB_ID) {
            // If there's no anchor, treat the previously selected tab as anchor.
            mAnchorTabId = TabModelUtils.getCurrentTabId(tabModel);
        }

        int anchorIndex = tabModel.indexOf(tabModel.getTabById(mAnchorTabId));
        int clickedIndex = tabModel.indexOf(tabModel.getTabById(clickedTabId));

        int startIndex = Math.min(anchorIndex, clickedIndex);
        int endIndex = Math.max(anchorIndex, clickedIndex);

        Set<Integer> selectedTabIds = new HashSet<>();
        Set<Token> tabGroupIds = new HashSet<>();
        if (startIndex != -1 && endIndex != -1) {
            for (int i = startIndex; i <= endIndex; i++) {
                Tab tab = tabModel.getTabAt(i);
                if (tab == null) continue;
                int tabId = tab.getId();
                selectedTabIds.add(tabId);
                // If part of a tab group, expand the tab group.
                Token tabGroupId = tab.getTabGroupId();
                if (tabGroupId != null && !tabGroupIds.contains(tabGroupId)) {
                    tabModel.setTabGroupCollapsed(
                            tabGroupId, /* isCollapsed= */ false, /* animate= */ true);
                    tabGroupIds.add(tabGroupId);
                }
            }
        }
        mSelectTabCallback.onResult(clickedTabId);
        tabModel.setTabsMultiSelected(/* tabIds= */ selectedTabIds, /* isSelected= */ true);
    }

    /**
     * Clears the entire set of multi-selected tabs.
     *
     * @param clearAnchor If true, the anchor tab for Shift+Click range selection is also reset.
     * @param notifyObservers Whether to notify observers of the selection clear.
     */
    public void clearMultiSelection(boolean clearAnchor, boolean notifyObservers) {
        if (clearAnchor) {
            // Clear anchor tab.
            mAnchorTabId = Tab.INVALID_TAB_ID;
        }
        TabModel tabModel = mTabModelSupplier.get();
        if (tabModel == null) return;
        tabModel.clearMultiSelection(/* notifyObservers= */ notifyObservers);
    }

    /**
     * Toggles multiSelection on the keyboard focused tab.
     *
     * @param tabId The ID of the tab that currently has keyboard focus.
     */
    public void multiselectKeyboardFocusedItem(int tabId) {
        TabModel tabModel = mTabModelSupplier.get();
        if (tabModel == null || tabId == Tab.INVALID_TAB_ID || tabModel.getTabById(tabId) == null) {
            return;
        }

        // If the tab is already multi-selected, unselect it.
        if (tabModel.isTabMultiSelected(tabId)) {
            tabModel.setTabsMultiSelected(Set.of(tabId), /* isSelected= */ false);
        } else {
            int activeTabId = TabModelUtils.getCurrentTabId(tabModel);
            // When toggling multiselect, we need to add the active tab to the multi-selection set.
            // This is an additive operation, and does not reset the selection set.
            if (activeTabId != Tab.INVALID_TAB_ID && activeTabId != tabId) {
                tabModel.setTabsMultiSelected(Set.of(tabId, activeTabId), /* isSelected= */ true);
            } else {
                tabModel.setTabsMultiSelected(Set.of(tabId), /* isSelected= */ true);
            }
        }
    }

    /**
     * Handles the specific user action of Ctrl+clicking the currently active tab. This action
     * deselects the active tab and transfers the active status to another tab within the existing
     * multi-selection. The new active tab will be the earliest index tab in the current selection.
     * If the clicked tab is the only one selected, this method does nothing to prevent a state with
     * no active tab.
     *
     * @param tabId The ID of the currently active tab that was clicked.
     */
    private void handleSelectedTabCtrlClicked(int tabId) {
        TabModel tabModel = mTabModelSupplier.get();
        if (tabModel == null
                || tabModel.getMultiSelectedTabsCount() <= 1
                || tabModel.getCount() <= 1) {
            // Can't deselect the only tab.
            return;
        }

        // Find and select the new active tab, which will be the earliest index tab
        // in the selection that isn't the one being deselected.
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            if (tab != null && tab.getId() != tabId && tabModel.isTabMultiSelected(tab.getId())) {
                mSelectTabCallback.onResult(tab.getId());
                tabModel.setTabsMultiSelected(Set.of(tabId), /* isSelected= */ false);
                break;
            }
        }
    }

    /** Returns the current anchor tab ID for Shift+Click range selection. */
    public int getAnchorTabIdForTesting() {
        return mAnchorTabId;
    }

    /** Sets the anchor tab ID for testing Shift+Click range selection. */
    void setAnchorTabIdForTesting(int anchorTabId) {
        mAnchorTabId = anchorTabId;
    }
}
