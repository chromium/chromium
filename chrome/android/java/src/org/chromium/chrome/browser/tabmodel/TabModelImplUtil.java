// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;

import java.util.Set;

/**
 * Utility class for {@link TabCollectionTabModelImpl}. Permits easier testing of some static
 * helpers.
 */
@NullMarked
class TabModelImplUtil {

    /**
     * Sets the multi-selected state for a collection of tabs in a single batch operation.
     *
     * @param tabIds A Set of tab IDs to either add to or remove from the multi-selection.
     * @param isSelected If true, the tab IDs will be added; if false, they will be removed.
     * @param multiSelectedTabs The Set of selected tab IDs to modify.
     * @param observers The observer list to notify of the change.
     */
    /* package */ static void setTabsMultiSelected(
            Set<Integer> tabIds,
            boolean isSelected,
            Set<Integer> multiSelectedTabs,
            ObserverList<TabModelObserver> observers) {
        if (isSelected) {
            multiSelectedTabs.addAll(tabIds);
        } else {
            multiSelectedTabs.removeAll(tabIds);
        }
        for (TabModelObserver obs : observers) {
            obs.onTabsSelectionChanged();
        }
    }

    /**
     * Clears the entire multi-selection set.
     *
     * @param notifyObservers If true, observers will be notified of the change.
     * @param multiSelectedTabs The Set of selected tab IDs to clear.
     * @param observers The observer list to notify of the change.
     */
    /* package */ static void clearMultiSelection(
            boolean notifyObservers,
            Set<Integer> multiSelectedTabs,
            ObserverList<TabModelObserver> observers) {
        if (multiSelectedTabs.isEmpty()) return;
        multiSelectedTabs.clear();
        if (notifyObservers) {
            for (TabModelObserver obs : observers) {
                obs.onTabsSelectionChanged();
            }
        }
    }

    /**
     * Checks if a tab is part of the current selection. A tab is considered selected if it is
     * either the currently active tab or has been explicitly added to the multi-selection group.
     *
     * @param tabId The ID of the tab to check.
     * @param multiSelectedTabs The Set containing the IDs of multi-selected tabs.
     * @param model The TabModel, used to get the currently active tab.
     * @return true if the tab is selected, false otherwise.
     */
    /* package */ static boolean isTabMultiSelected(
            int tabId, Set<Integer> multiSelectedTabs, TabModel model) {
        return multiSelectedTabs.contains(tabId) || tabId == TabModelUtils.getCurrentTabId(model);
    }
}
