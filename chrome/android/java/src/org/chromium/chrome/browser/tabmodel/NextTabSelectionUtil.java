// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Utility class providing static helper methods for next-tab selection heuristics when tabs are
 * closed or tab groups are collapsed.
 */
@NullMarked
public class NextTabSelectionUtil {

    private NextTabSelectionUtil() {}

    /**
     * Returns the next tab to select after closing the given tabs.
     *
     * @param model The {@link TabModel} to act on.
     * @param modelDelegate The {@link TabModelDelegate} to check other models, or null.
     * @param closingTabs The list of tabs that are closing.
     * @param uponExit Whether the app is closing as a result of this tab closure.
     * @return The next tab to select after closing the given tabs or null if no tab could be found.
     */
    public static @Nullable Tab getNextTabIfClosed(
            TabModel model,
            @Nullable TabModelDelegate modelDelegate,
            List<Tab> closingTabs,
            boolean uponExit) {
        return getNextTabIfClosed(
                model,
                modelDelegate,
                closingTabs,
                uponExit,
                closingTabs.size() == 1 ? TabCloseType.SINGLE : TabCloseType.MULTIPLE);
    }

    /**
     * Returns the next tab to select after closing the given tabs.
     *
     * <p>Selection order heuristics:
     *
     * <ol>
     *   <li>If closing a tab in a non-active model, return current tab of active model.
     *   <li>If the current tab is not closing, stay on the current tab.
     *   <li>If closing upon exit, return the most recent active tab.
     *   <li>If hierarchical next tab policy is active, prefer the parent tab (if expanded).
     *   <li>Fall back to finding the nearest expanded non-closing tab (or collapsed if no expanded
     *       tab exists).
     * </ol>
     *
     * @param model The {@link TabModel} to act on.
     * @param modelDelegate The {@link TabModelDelegate} to check other models, or null.
     * @param closingTabs The list of tabs that are closing.
     * @param uponExit Whether the app is closing as a result of this tab closure.
     * @param tabCloseType The type of tab closure.
     * @return The next tab to select after closing the given tabs or null if no tab could be found.
     */
    public static @Nullable Tab getNextTabIfClosed(
            TabModel model,
            @Nullable TabModelDelegate modelDelegate,
            List<Tab> closingTabs,
            boolean uponExit,
            @TabCloseType int tabCloseType) {
        // If closing a tab in the non-active model, select the current tab in the active model.
        if (!model.isActiveModel() && modelDelegate != null) {
            Tab otherModelTab = TabModelUtils.getCurrentTab(modelDelegate.getCurrentModel());
            return otherModelTab != null && !otherModelTab.isClosing() ? otherModelTab : null;
        }

        // If the current tab is not closing, return it.
        Tab currentTab = model.getCurrentTabSupplier().get();
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

        // Select the parent tab if it exists and is expanded.
        if (closingTabs.size() == 1
                && NextTabPolicy.HIERARCHICAL == model.getNextTabPolicySupplier().get()) {
            Tab parentTab =
                    findTabInAllTabModels(
                            model,
                            modelDelegate,
                            closingTabs.get(0).getParentId(),
                            model.getCount() <= 1);
            if (parentTab != null
                    && validNextTab(parentTab, closingTabs)
                    && !isTabGroupCollapsed(model, parentTab)) {
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
        if (modelDelegate != null && model.isIncognitoBranded()) {
            Tab regularCurrentTab = TabModelUtils.getCurrentTab(modelDelegate.getModel(false));
            if (validNextTab(regularCurrentTab)) {
                return regularCurrentTab;
            }
        }

        return null;
    }

    /**
     * Returns the tab that is closest to the given index, if any. Prioritizes expanded tabs over
     * collapsed tabs.
     *
     * @param tabIterable The iterable of tabs to act on.
     * @param closingIndex The index of the tab that is closing.
     * @param closingTabs The list of tabs that are closing. This is used to avoid returning a tab
     *     that is closing.
     * @return The closest tab or null if no tab could be found.
     */
    public static @Nullable Tab findNearbyNotClosingTab(
            Iterable<Tab> tabIterable, int closingIndex, List<Tab> closingTabs) {
        TabModel model = tabIterable instanceof TabModel ? (TabModel) tabIterable : null;

        Tab expandedLeftCandidate = null;
        Tab expandedRightCandidate = null;
        Tab collapsedLeftCandidate = null;
        Tab collapsedRightCandidate = null;

        Set<Tab> closingTabSet =
                closingTabs instanceof Set ? (Set<Tab>) closingTabs : new HashSet<>(closingTabs);

        int currentIndex = 0;

        for (Tab tab : tabIterable) {
            if (validNextTab(tab, closingTabSet)) {
                boolean isCollapsed = model != null && isTabGroupCollapsed(model, tab);
                if (currentIndex < closingIndex) {
                    if (!isCollapsed) {
                        expandedLeftCandidate = tab;
                    }
                    collapsedLeftCandidate = tab;
                } else if (currentIndex > closingIndex) {
                    if (!isCollapsed && expandedRightCandidate == null) {
                        expandedRightCandidate = tab;
                    }
                    if (collapsedRightCandidate == null) {
                        collapsedRightCandidate = tab;
                    }
                }
            }
            currentIndex++;
        }

        if (expandedRightCandidate != null) return expandedRightCandidate;
        if (expandedLeftCandidate != null) return expandedLeftCandidate;
        if (collapsedRightCandidate != null) return collapsedRightCandidate;
        return collapsedLeftCandidate;
    }

    private static boolean isTabGroupCollapsed(TabModel model, Tab tab) {
        Token groupId = tab.getTabGroupId();
        return groupId != null && model.getTabGroupCollapsed(groupId);
    }

    private static boolean validNextTab(@Nullable Tab tab, Collection<Tab> closingTabs) {
        return tab != null && !tab.isClosing() && !closingTabs.contains(tab);
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

    private static @Nullable Tab findTabInAllTabModels(
            TabModel model,
            @Nullable TabModelDelegate modelDelegate,
            int tabId,
            boolean includeOtherModels) {
        if (tabId == Tab.INVALID_TAB_ID) return null;
        if (modelDelegate != null) {
            boolean isIncognito = model.isIncognitoBranded();
            Tab tab = modelDelegate.getModel(isIncognito).getTabById(tabId);
            if (tab != null) return tab;
            if (includeOtherModels) {
                return modelDelegate.getModel(!isIncognito).getTabById(tabId);
            }
            return null;
        }
        return model.getTabById(tabId);
    }
}
