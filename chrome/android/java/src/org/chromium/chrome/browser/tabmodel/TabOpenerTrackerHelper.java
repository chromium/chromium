// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.ArraySet;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Tracks opener/parent and sibling relationships among tabs in a {@link TabModel} using a set of
 * related tab IDs.
 *
 * <p>Observes {@link TabModel} to track newly opened tabs in the current relationship cluster and
 * resets the tracked set when the user switches to an unrelated tab. This mimics the logic in
 * chrome/browser/ui/tabs/tab_strip_model.cc.
 *
 * <p>TODO(crbug.com/545250525): Investigate if it is safe to directly mutate the Tabs' parent IDs
 * instead of tracking the Set of related IDs in this class.
 */
@NullMarked
/* package */ final class TabOpenerTrackerHelper implements TabModelObserver {
    private final Set<Integer> mRelatedTabIds = new ArraySet<>();

    /**
     * Returns a new {@link TabOpenerTrackerHelper} if the TabOpenerTracking feature is enabled, or
     * {@code null} otherwise.
     */
    /* package */ static @Nullable TabOpenerTrackerHelper create() {
        if (!ChromeFeatureList.sTabOpenerTracking.isEnabled()) return null;
        return new TabOpenerTrackerHelper();
    }

    private TabOpenerTrackerHelper() {}

    // ============================================================================================
    // TabModelObserver Implementation
    // ============================================================================================

    @Override
    public void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        // If the added tab was opened by one of the related tabs, add its ID to the tracked set.
        int parentId = tab.getParentId();
        if (parentId != Tab.INVALID_TAB_ID && mRelatedTabIds.contains(parentId)) {
            mRelatedTabIds.add(tab.getId());
        }
    }

    @Override
    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
        if (lastId == Tab.INVALID_TAB_ID || tab.getId() == lastId) return;

        // If an unrelated tab is selected, reset the tracked set.
        boolean isRelated = mRelatedTabIds.contains(tab.getId());
        if (!isRelated) {
            mRelatedTabIds.clear();
            mRelatedTabIds.add(tab.getId());
        }
    }

    @Override
    public void tabRemoved(Tab tab) {
        mRelatedTabIds.remove(tab.getId());
    }

    // ============================================================================================
    // API
    // ============================================================================================

    /**
     * Finds the next tab to select hierarchically (child -> sibling -> parent) among the currently
     * related tabs.
     *
     * @param tabModel The {@link TabModel} to search within.
     * @param closingTab The {@link Tab} that is closing.
     * @param closingTabs The collection of all tabs that are closing.
     * @return The next {@link Tab} to select, or {@code null} if no candidate is found.
     */
    /* package */ @Nullable Tab findHierarchicalNextTab(
            TabModel tabModel, Tab closingTab, List<Tab> closingTabs) {
        if (!mRelatedTabIds.contains(closingTab.getId())) return null;

        Tab childTab = findChildTab(tabModel, closingTab, closingTabs);
        if (childTab != null) return childTab;

        Tab siblingTab = findSiblingTab(tabModel, closingTab, closingTabs);
        if (siblingTab != null) return siblingTab;

        return findParentTab(tabModel, closingTab, closingTabs);
    }

    // ============================================================================================
    // Private helpers
    // ============================================================================================

    /**
     * Finds a child tab opened by {@code closingTab}, searching outward from the closing tab index.
     *
     * @param tabModel The {@link TabModel} to search within.
     * @param closingTab The {@link Tab} being closed.
     * @param closingTabs Collection of tabs being closed.
     * @return A child {@link Tab}, or {@code null} if none found.
     */
    private @Nullable Tab findChildTab(
            TabModel tabModel, Tab closingTab, Collection<Tab> closingTabs) {
        return findTabWithParentId(tabModel, closingTab.getId(), closingTab, closingTabs);
    }

    /**
     * Finds a sibling tab opened by the same opener as {@code closingTab}, searching outward.
     *
     * @param tabModel The {@link TabModel} to search within.
     * @param closingTab The {@link Tab} being closed.
     * @param closingTabs Collection of tabs being closed.
     * @return A sibling {@link Tab}, or {@code null} if none found.
     */
    private @Nullable Tab findSiblingTab(
            TabModel tabModel, Tab closingTab, Collection<Tab> closingTabs) {
        int parentId = closingTab.getParentId();
        if (!mRelatedTabIds.contains(parentId)) return null;

        return findTabWithParentId(tabModel, parentId, closingTab, closingTabs);
    }

    /**
     * Finds a tab with the specified {@code parentId}, searching outward (right then left) from
     * {@code startTab}. This only considers "related" tabs tracked in {@link #mRelatedTabIds}.
     *
     * @param tabModel The {@link TabModel} to search within.
     * @param parentId The parent ID to search for.
     * @param startTab The {@link Tab} to search outward from.
     * @param excludedTabs A Collection of {@link Tab}s to exclude from the search.
     */
    private @Nullable Tab findTabWithParentId(
            TabModel tabModel, int parentId, Tab startTab, Collection<Tab> excludedTabs) {
        if (parentId == Tab.INVALID_TAB_ID) return null;

        int startIndex = tabModel.indexOf(startTab);
        if (startIndex == TabList.INVALID_TAB_INDEX) return null;
        int count = tabModel.getCount();

        // Scan right
        for (int i = startIndex + 1; i < count; i++) {
            Tab tab = tabModel.getTabAtChecked(i);
            if (tab.getParentId() == parentId && validCandidate(tabModel, tab, excludedTabs)) {
                return tab;
            }
        }
        // Scan left
        for (int i = startIndex - 1; i >= 0; i--) {
            Tab tab = tabModel.getTabAtChecked(i);
            if (tab.getParentId() == parentId && validCandidate(tabModel, tab, excludedTabs)) {
                return tab;
            }
        }
        return null;
    }

    /**
     * Finds the parent tab of {@code closingTab} within the same model. This only considers
     * "related" tabs tracked in {@link #mRelatedTabIds}.
     *
     * @param tabModel The {@link TabModel} to search within.
     * @param closingTab The {@link Tab} being closed.
     * @param closingTabs Collection of tabs being closed.
     * @return The parent {@link Tab}, or {@code null} if none found.
     */
    private @Nullable Tab findParentTab(
            TabModel tabModel, Tab closingTab, Collection<Tab> closingTabs) {
        Tab parentTab = tabModel.getTabById(closingTab.getParentId());
        if (validCandidate(tabModel, parentTab, closingTabs)) return parentTab;
        return null;
    }

    private boolean validCandidate(
            TabModel tabModel, @Nullable Tab tab, Collection<Tab> closingTabs) {
        return tab != null
                && mRelatedTabIds.contains(tab.getId())
                && !tab.isClosing()
                && !closingTabs.contains(tab)
                && !isTabGroupCollapsed(tabModel, tab);
    }

    private boolean isTabGroupCollapsed(TabModel tabModel, Tab tab) {
        Token groupId = tab.getTabGroupId();
        return groupId != null && tabModel.getTabGroupCollapsed(groupId);
    }

    // ============================================================================================
    // Testing Methods
    // ============================================================================================

    /* package */ Set<Integer> getRelatedTabIdsForTesting() {
        return Collections.unmodifiableSet(mRelatedTabIds);
    }

    /* package */ void addRelatedTabForTesting(int tabId) {
        mRelatedTabIds.add(tabId);
    }
}
