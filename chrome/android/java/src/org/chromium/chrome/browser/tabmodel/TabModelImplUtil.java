// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Set;

/**
 * Utility class for {@link TabCollectionTabModelImpl}. Permits easier testing of some static
 * helpers.
 */
@NullMarked
class TabModelImplUtil {
    /**
     * Returns the next tab to select after closing the given tabs.
     *
     * @param model The {@link TabModel} to act on.
     * @param modelDelegate The {@link TabModelDelegate} to get the current tab from.
     * @param currentTabSupplier The {@link MonotonicObservableSupplier} that supplies the current tab.
     * @param nextTabPolicySupplier The {@link NextTabPolicySupplier} to get the next tab policy.
     * @param closingTabs The list of tabs that are closing.
     * @param uponExit Whether the app is closing as a result of this tab closure.
     * @param tabCloseType The type of tab closure.
     * @return The next tab to select after closing the given tabs or null if no tab could be found.
     */
    /* package */ static @Nullable Tab getNextTabIfClosed(
            TabModel model,
            TabModelDelegate modelDelegate,
            NullableObservableSupplier<Tab> currentTabSupplier,
            NextTabPolicySupplier nextTabPolicySupplier,
            List<Tab> closingTabs,
            boolean uponExit,
            @TabCloseType int tabCloseType) {
        // If closing a tab in the non-active model, select the current tab in the active model.
        if (!model.isActiveModel()) {
            Tab otherModelTab = TabModelUtils.getCurrentTab(modelDelegate.getCurrentModel());
            return otherModelTab != null && !otherModelTab.isClosing() ? otherModelTab : null;
        }

        // If the current tab is not closing, return it.
        Tab currentTab = currentTabSupplier.get();
        if (validNextTab(currentTab) && !closingTabs.contains(currentTab)) {
            return currentTab;
        }

        // If uponExit, select the next most recent tab.
        if (uponExit) {
            Tab nextMostRecentTab = TabModelUtils.getMostRecentTab(model, closingTabs);
            if (validNextTab(nextMostRecentTab)) {
                return nextMostRecentTab;
            }
        }

        // If not in overview mode, select the parent tab if it exists.
        if (closingTabs.size() == 1 && NextTabPolicy.HIERARCHICAL == nextTabPolicySupplier.get()) {
            Tab parentTab =
                    findTabInAllTabModels(
                            modelDelegate,
                            model.isIncognitoBranded(),
                            closingTabs.get(0).getParentId());
            if (validNextTab(parentTab)) {
                return parentTab;
            }
        }

        // Select a nearby tab if one exists.
        if (tabCloseType != TabCloseType.ALL) {
            int anchorIndex = -1;

            // Search for the first closing tab that is not a new tab.
            for (Tab tab : closingTabs) {
                if (isNotNewTab(tab)) {
                    anchorIndex = model.indexOf(tab);
                    break;
                }
            }

            // Fallback to the active tab if all closing tabs were blank new tabs.
            if (anchorIndex == -1 && currentTab != null) {
                anchorIndex = model.indexOf(currentTab);
            }

            // Ultimate fallback to the first closing tab if all else fails.
            if (anchorIndex == -1) {
                anchorIndex = model.indexOf(closingTabs.get(0));
            }

            Tab nearbyTab = findNearbyNotClosingTab(model, anchorIndex, closingTabs);
            if (validNextTab(nearbyTab)) {
                return nearbyTab;
            }
        }

        // If closing the last incognito tab, select the current normal tab.
        if (model.isIncognitoBranded()) {
            Tab regularCurrentTab = TabModelUtils.getCurrentTab(modelDelegate.getModel(false));
            if (validNextTab(regularCurrentTab)) {
                return regularCurrentTab;
            }
        }

        return null;
    }

    private static boolean validNextTab(@Nullable Tab tab) {
        return tab != null && !tab.isClosing();
    }

    private static boolean isNotNewTab(@Nullable Tab tab) {
        if (tab == null || tab.isClosing()) return false;
        GURL url = tab.getUrl();
        if (url == null) return false;

        return !UrlUtilities.isNtpUrl(url);
    }

    /**
     * Returns the tab with the given ID checking both incognito and normal tab models.
     *
     * @param modelDelegate The {@link TabModelDelegate} to get the tab from.
     * @param isIncognito Whether to start with checking the incognito tab model.
     * @param tabId The ID of the tab to get.
     * @return The tab with the given ID or null if no tab could be found.
     */
    private static @Nullable Tab findTabInAllTabModels(
            TabModelDelegate modelDelegate, boolean isIncognito, int tabId) {
        Tab tab = modelDelegate.getModel(isIncognito).getTabById(tabId);
        if (tab != null) return tab;
        return modelDelegate.getModel(!isIncognito).getTabById(tabId);
    }

    /**
     * Returns the tab that is closest to the given index, if any.
     *
     * @param tabIterable The iterable to act on.
     * @param closingIndex The index of the tab that is closing.
     * @param closingTabs The list of tabs that are closing. This is used to avoid returning a tab
     *     that is closing.
     * @return The closest tab or null if no tab could be found.
     */
    /* package */ static @Nullable Tab findNearbyNotClosingTab(
            Iterable<Tab> tabIterable, int closingIndex, List<Tab> closingTabs) {
        // This is implemented in desktop here: chrome/browser/ui/tabs/tab_strip_model.cc
        Tab leftCandidate = null;
        int currentIndex = 0;

        for (Tab tab : tabIterable) {
            if (currentIndex < closingIndex) {
                if (validNextTab(tab, closingTabs)) leftCandidate = tab;
            } else if (currentIndex > closingIndex) {
                if (validNextTab(tab, closingTabs)) return tab;
            }
            currentIndex++;
        }

        return leftCandidate;
    }

    private static boolean validNextTab(@Nullable Tab tab, List<Tab> closingTabs) {
        return tab != null && !tab.isClosing() && !closingTabs.contains(tab);
    }

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
