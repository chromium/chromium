// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;
import java.util.Set;

/** Interface for getting tab groups for the tabs in the {@link TabModel}. */
public interface TabGroupModelFilter extends TabList {
    /**
     * Adds a {@link TabModelObserver} to be notified on {@link TabGroupModelFilter} changes.
     *
     * @param observer The {@link TabModelObserver} to add.
     */
    void addObserver(TabModelObserver observer);

    /**
     * Removes a {@link TabModelObserver}.
     *
     * @param observer The {@link TabModelObserver} to remove.
     */
    void removeObserver(TabModelObserver observer);

    /**
     * This method adds a {@link TabGroupModelFilterObserver} to be notified on {@link
     * TabGroupModelFilter} changes.
     *
     * @param observer The {@link TabGroupModelFilterObserver} to add.
     */
    public void addTabGroupObserver(TabGroupModelFilterObserver observer);

    /**
     * This method removes a {@link TabGroupModelFilterObserver}.
     *
     * @param observer The {@link TabGroupModelFilterObserver} to remove.
     */
    public void removeTabGroupObserver(TabGroupModelFilterObserver observer);

    /** Whether this is filter for the currently active {@link TabModel}. */
    boolean isCurrentlySelectedFilter();

    /** Returns the {@link TabModel} that the filter is acting on. */
    @NonNull
    TabModel getTabModel();

    /**
     * A wrapper around {@link TabModel#closeTabs} that sets hiding state for tab groups correctly.
     *
     * @param tabClosureParams The params to use when closing tabs.
     */
    public boolean closeTabs(TabClosureParams tabClosureParams);

    /** Returns the total tab count in the underlying {@link TabModel}. */
    int getTotalTabCount();

    /** Returns the number of tab groups. */
    public int getTabGroupCount();

    /**
     * This method returns the number of tabs in a tab group with reference to {@code tabRootId} as
     * root id.
     *
     * @param tabRootId The tab root id that is used to find the related group.
     * @return The number of related tabs.
     */
    public int getRelatedTabCountForRootId(int tabRootId);

    /**
     * @param rootId The root identifier of the tab group.
     * @return Whether the given rootId has any tab group associated with it.
     */
    public boolean tabGroupExistsForRootId(int rootId);

    /**
     * Given a tab group's stable ID, finds out the root ID, or {@link Tab.INVALID_TAB_ID} if the
     * tab group doesn't exist in the model.
     *
     * @param stableId The stable ID of the tab group.
     * @return The root ID of the tab group or {@link Tab.INVALID_TAB_ID} if the group isn't found
     *     in the tab model.
     */
    public int getRootIdFromStableId(@NonNull Token stableId);

    /**
     * Given a tab group's root ID, finds out the stable ID, or null if the tab group doesn't exist
     * in the model.
     *
     * @param rootId The root ID of the tab group.
     * @return The stable ID of the tab group or null if the group isn't found in the tab model.
     */
    public @Nullable Token getStableIdFromRootId(int rootId);

    /**
     * Any of the concrete class can override and define a relationship that links a {@link Tab} to
     * a list of related {@link Tab}s. By default, this returns an unmodifiable list that only
     * contains the {@link Tab} with the given id. Note that the meaning of related can vary
     * depending on the filter being applied.
     *
     * @param tabId Id of the {@link Tab} try to relate with.
     * @return An unmodifiable list of {@link Tab} that relate with the given tab id.
     */
    @NonNull
    List<Tab> getRelatedTabList(int tabId);

    /**
     * Any of the concrete class can override and define a relationship that links a {@link Tab} to
     * a list of related {@link Tab}s. By default, this returns an unmodifiable list that only
     * contains the given id. Note that the meaning of related can vary depending on the filter
     * being applied.
     *
     * @param tabId Id of the {@link Tab} try to relate with.
     * @return An unmodifiable list of id that relate with the given tab id.
     */
    @NonNull
    List<Integer> getRelatedTabIds(int tabId);

    /**
     * This method returns all tabs in a tab group with reference to {@code tabRootId} as root id.
     *
     * @param tabRootId The tab root id that is used to find the related group.
     * @return An unmodifiable list of {@link Tab} that relate with the given tab root id.
     */
    public List<Tab> getRelatedTabListForRootId(int tabRootId);

    /**
     * @param tab A {@link Tab} to check group membership of.
     * @return Whether the given {@link Tab} is part of a tab group.
     */
    boolean isTabInTabGroup(Tab tab);

    /** Returns the position of the given {@link Tab} in its group. */
    public int getIndexOfTabInGroup(Tab tab);

    /**
     * @param rootId The rootId of the group to lookup.
     * @return the last shown tab in that group or Tab.INVALID_TAB_ID otherwise.
     */
    public int getGroupLastShownTabId(int rootId);

    /**
     * @param rootId The rootId of the group to lookup.
     * @return the last shown tab in that group or null otherwise.
     */
    public @Nullable Tab getGroupLastShownTab(int rootId);

    /**
     * This method moves the tab group which contains the tab with tab {@code id} to {@code
     * newIndex} in TabModel.
     *
     * @param id The id of the tab whose related tabs are being moved.
     * @param newIndex The new index in TabModel that these tabs are being moved to.
     */
    public void moveRelatedTabs(int id, int newIndex);

    /**
     * This method checks if an impending group merge action will result in a new group creation.
     *
     * @param tabsToMerge The list of tabs to be merged including all source and destination tabs.
     */
    public boolean willMergingCreateNewGroup(List<Tab> tabsToMerge);

    /**
     * Creates a tab group containing a single tab.
     *
     * @param tabId The tab id of the tab to create the group for.
     * @param notify Whether to notify observers to create an undo snackbar.
     */
    public void createSingleTabGroup(int tabId, boolean notify);

    /** Same as {@link #createSingleTabGroup(int, boolean)}, but with a {@link Tab} object. */
    public void createSingleTabGroup(Tab tab, boolean notify);

    /**
     * This method merges the source group that contains the {@code sourceTabId} to the destination
     * group that contains the {@code destinationTabId}. This method only operates if two groups are
     * in the same {@code TabModel}.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param destinationTabId The id of a {@link Tab} to get the destination group.
     */
    public void mergeTabsToGroup(int sourceTabId, int destinationTabId);

    /**
     * This method merges the source group that contains the {@code sourceTabId} to the destination
     * group that contains the {@code destinationTabId}. This method only operates if two groups are
     * in the same {@code TabModel}.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param destinationTabId The id of a {@link Tab} to get the destination group.
     * @param skipUpdateTabModel True if updating the tab model will be handled elsewhere (e.g. by
     *     the tab strip).
     */
    public void mergeTabsToGroup(int sourceTabId, int destinationTabId, boolean skipUpdateTabModel);

    /**
     * This method appends a list of {@link Tab}s to the destination group that contains the {@code}
     * destinationTab. The {@link TabModel} ordering of the tabs in the given list is not preserved.
     * After calling this method, the {@link TabModel} ordering of these tabs would become the
     * ordering of {@code tabs}.
     *
     * @param tabs List of {@link Tab}s to be appended.
     * @param destinationTab The destination {@link Tab} to be append to.
     * @param notify Whether or not to notify observers about the merging events.
     */
    public void mergeListOfTabsToGroup(List<Tab> tabs, Tab destinationTab, boolean notify);

    /**
     * This method moves Tab with id as {@code sourceTabId} out of the group it belongs to in the
     * specified direction.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param trailing True if the tab should be placed after the tab group when removed. False if
     *     it should be placed before.
     */
    public void moveTabOutOfGroupInDirection(int sourceTabId, boolean trailing);

    // TODO(crbug.com/372068933): This method should probably have more restricted access.
    /**
     * This method undo the given grouped {@link Tab}.
     *
     * @param tab undo this grouped {@link Tab}.
     * @param originalIndex The tab index before grouped.
     * @param originalRootId The rootId before grouped.
     * @param originalTabGroupId The tabGroupId before grouped.
     */
    public void undoGroupedTab(
            Tab tab, int originalIndex, int originalRootId, @Nullable Token originalTabGroupId);

    /** Get all tab group root ids that are associated with tab groups. */
    public Set<Integer> getAllTabGroupRootIds();

    /** Get all tab group IDs that are associated with tab groups. */
    public Set<Token> getAllTabGroupIds();

    /**
     * Returns a valid position to add or move a tab to this model in the context of any related
     * tabs.
     *
     * @param tab The tab to be added/moved.
     * @param proposedPosition The current or proposed position of the tab in the model.
     * @return a valid position close to proposedPosition that respects related tab ordering rules.
     */
    int getValidPosition(Tab tab, int proposedPosition);

    /** Returns whether the tab model is fully restored. */
    boolean isTabModelRestored();

    /** Returns whether the tab group is being hidden. */
    public boolean isTabGroupHiding(@Nullable Token tabGroupId);

    /**
     * Returns a lazy oneshot supplier that generates all the tab group IDs including those pending
     * closure except those requested to be excluded.
     *
     * @param tabsToExclude The list of tabs to exclude.
     * @return A lazy oneshot supplier containing all the tab group IDs including those pending
     *     closure.
     */
    public LazyOneshotSupplier<Set<Token>> getLazyAllTabGroupIdsInComprehensiveModel(
            List<Tab> tabsToExclude);

    /**
     * Returns a lazy oneshot supplier that generates all the root IDs including those pending
     * closure except those requested to be excluded.
     *
     * @param tabsToExclude The list of tabs to exclude.
     * @return A lazy oneshot supplier containing all the root IDs including those pending closure.
     */
    public LazyOneshotSupplier<Set<Integer>> getLazyAllRootIdsInComprehensiveModel(
            List<Tab> tabsToExclude);

    /** Returns the current title of the tab group. */
    public String getTabGroupTitle(int rootId);

    /** Stores the given title for the tab group. */
    public void setTabGroupTitle(int rootId, String title);

    /** Deletes the stored title for the tab group, defaulting it back to "N tabs." */
    public void deleteTabGroupTitle(int rootId);

    /**
     * This method fetches tab group colors id for the specified tab group. It will be a {@link
     * TabGroupColorId} if found, otherwise a {@link TabGroupTitleUtils.INVALID_COLOR_ID} if there
     * is no color entry for the group.
     */
    public int getTabGroupColor(int rootId);

    /**
     * This method fetches tab group colors for the related tab group root ID. If the color does not
     * exist, then GREY will be returned. This method is intended to be used by UI surfaces that
     * want to show a color, and they need the color returned to be valid.
     *
     * @param rootId The tab root ID whose related tab group color will be fetched if found.
     * @return The color that should be used for this group.
     */
    public @TabGroupColorId int getTabGroupColorWithFallback(int rootId);

    /** Stores the given color for the tab group. */
    public void setTabGroupColor(int rootId, @TabGroupColorId int color);

    /** Deletes the color that was recorded for the group. */
    public void deleteTabGroupColor(int rootId);

    /** Returns whether the tab group is expanded or collapsed. */
    public boolean getTabGroupCollapsed(int rootId);

    /** Sets whether the tab group is expanded or collapsed. */
    public void setTabGroupCollapsed(int rootId, boolean isCollapsed);

    /** Deletes the record that the group is collapsed, setting it to expanded. */
    public void deleteTabGroupCollapsed(int rootId);

    /** Delete the title, color and collapsed state of a tab group. */
    public void deleteTabGroupVisualData(int rootId);

    /** Returns the sync ID associated with the tab group. */
    public String getTabGroupSyncId(int rootId);

    /** Stores the sync ID associated with the tab group. */
    public void setTabGroupSyncId(int rootId, String syncId);

    //
}
