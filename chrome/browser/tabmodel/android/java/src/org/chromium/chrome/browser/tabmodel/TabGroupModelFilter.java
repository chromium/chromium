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
    void addTabGroupObserver(TabGroupModelFilterObserver observer);

    /**
     * This method removes a {@link TabGroupModelFilterObserver}.
     *
     * @param observer The {@link TabGroupModelFilterObserver} to remove.
     */
    void removeTabGroupObserver(TabGroupModelFilterObserver observer);

    /** Returns the {@link TabModel} that the filter is acting on. */
    @NonNull
    TabModel getTabModel();

    /** Returns the number of tab groups. */
    int getTabGroupCount();

    /**
     * This method returns the number of tabs in a tab group with reference to {@code tabRootId} as
     * root id.
     *
     * @param tabRootId The tab root id that is used to find the related group.
     * @return The number of related tabs.
     * @deprecated Use {@link #getTabCountForGroup(Token)}. This method returns 1 in the event the
     *     group was not found or a tab is not in a group which is confusing. Any existing usages of
     *     this method will be migrated and any reliance on this method returning 1 if the group
     *     doesn't exist will be fixed as part of the migration.
     */
    @Deprecated
    int getRelatedTabCountForRootId(int tabRootId);

    /**
     * Returns the number of tabs in the tab group with {@code tabGroupId} or 0 if the tab group
     * does not exist.
     */
    int getTabCountForGroup(@Nullable Token tabGroupId);

    /**
     * @param rootId The root identifier of the tab group.
     * @return Whether the given rootId is tracked in the {@link TabGroupModelFilter}.
     * @deprecated Use {@link #tabGroupExists(Token)}. This method is confusing; it checked if any
     *     {@link TabGroup} existed for the {@code rootId}. This is not the same as the tab group
     *     being a valid group since {@link TabGroup} objects exist for all tabs and only some of
     *     the tabs are valid tab groups. When migrating off this method make sure the new behavior
     *     is still applicable. The old implementation effectively leaked implementation details
     *     which shouldn't be relevant to any caller, but in the event it was relevant a workaround
     *     might be required.
     */
    @Deprecated
    boolean tabGroupExistsForRootId(int rootId);

    /** Returns whether a tab group exists with {@code tabGroupId}. */
    boolean tabGroupExists(@Nullable Token tabGroupId);

    /**
     * Given a tab group's stable ID, finds out the root ID, or {@link Tab.INVALID_TAB_ID} if the
     * tab group doesn't exist in the model.
     *
     * @param stableId The stable ID of the tab group.
     * @return The root ID of the tab group or {@link Tab.INVALID_TAB_ID} if the group isn't found
     *     in the tab model.
     */
    int getRootIdFromTabGroupId(@Nullable Token stableId);

    /**
     * Given a tab group's root ID, finds out the stable ID, or null if the tab group doesn't exist
     * in the model.
     *
     * @param rootId The root ID of the tab group.
     * @return The stable ID of the tab group or null if the group isn't found in the tab model.
     */
    @Nullable
    Token getTabGroupIdFromRootId(int rootId);

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
    List<Tab> getRelatedTabListForRootId(int tabRootId);

    /**
     * @param tab A {@link Tab} to check group membership of.
     * @return Whether the given {@link Tab} is part of a tab group.
     */
    boolean isTabInTabGroup(Tab tab);

    /** Returns the position of the given {@link Tab} in its group. */
    int getIndexOfTabInGroup(Tab tab);

    /**
     * @param tabGroupId The tab group id of the group to lookup.
     * @return the last shown tab in that group or Tab.INVALID_TAB_ID otherwise.
     */
    int getGroupLastShownTabId(@Nullable Token tabGroupId);

    /**
     * @param rootId The rootId of the group to lookup.
     * @return the last shown tab in that group or Tab.INVALID_TAB_ID otherwise.
     */
    int getGroupLastShownTabId(int rootId);

    /**
     * @param rootId The rootId of the group to lookup.
     * @return the last shown tab in that group or null otherwise.
     */
    @Nullable
    Tab getGroupLastShownTab(int rootId);

    /**
     * This method moves the tab group which contains the tab with tab {@code id} to {@code
     * newIndex} in TabModel.
     *
     * @param id The id of the tab whose related tabs are being moved.
     * @param newIndex The new index in TabModel that these tabs are being moved to.
     */
    void moveRelatedTabs(int id, int newIndex);

    /**
     * This method checks if an impending group merge action will result in a new group creation.
     *
     * @param tabsToMerge The list of tabs to be merged including all source and destination tabs.
     */
    boolean willMergingCreateNewGroup(List<Tab> tabsToMerge);

    /**
     * Creates a tab group containing a single tab.
     *
     * @param tabId The tab id of the tab to create the group for.
     */
    void createSingleTabGroup(int tabId);

    /** Same as {@link #createSingleTabGroup(int)}, but with a {@link Tab} object. */
    void createSingleTabGroup(Tab tab);

    /**
     * Creates a tab group with a preallocated {@link Token} for the TabGroupId.
     *
     * <p>This should only be used by the tab group sync service and related code. Ideally, this
     * would be locked down using a mechanism like {@code friend class} or some sort of access
     * token. However, for now this disclaimer will suffice.
     *
     * @param tabs The list of tabs to make a tab group from. The first tab in the list will be the
     *     root tab. An empty list will no-op.
     * @param tabGroupId An externally minted tab group id token.
     */
    void createTabGroupForTabGroupSync(@NonNull List<Tab> tabs, @NonNull Token tabGroupId);

    /**
     * This method merges the source group that contains the {@code sourceTabId} to the destination
     * group that contains the {@code destinationTabId}. This method only operates if two groups are
     * in the same {@code TabModel}.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param destinationTabId The id of a {@link Tab} to get the destination group.
     */
    void mergeTabsToGroup(int sourceTabId, int destinationTabId);

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
    void mergeTabsToGroup(int sourceTabId, int destinationTabId, boolean skipUpdateTabModel);

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
    void mergeListOfTabsToGroup(List<Tab> tabs, Tab destinationTab, boolean notify);

    /** Returns a utility interface to help with that ungrouping tabs from a tab group. */
    @NonNull
    TabUngrouper getTabUngrouper();

    // TODO(crbug.com/372068933): This method should probably have more restricted access.
    /**
     * This method undo the given grouped {@link Tab}.
     *
     * @param tab undo this grouped {@link Tab}.
     * @param originalIndex The tab index before grouped.
     * @param originalRootId The rootId before grouped.
     * @param originalTabGroupId The tabGroupId before grouped.
     */
    void undoGroupedTab(
            Tab tab, int originalIndex, int originalRootId, @Nullable Token originalTabGroupId);

    /** Get all tab group root ids that are associated with tab groups. */
    Set<Integer> getAllTabGroupRootIds();

    /** Get all tab group IDs that are associated with tab groups. */
    Set<Token> getAllTabGroupIds();

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
    boolean isTabGroupHiding(@Nullable Token tabGroupId);

    /**
     * Returns a lazy oneshot supplier that generates all the tab group IDs except those requested
     * to be excluded.
     *
     * @param tabsToExclude The list of tabs to exclude.
     * @param includePendingClosures Whether to include pending tab closures.
     * @return A lazy oneshot supplier containing all the tab group IDs.
     */
    LazyOneshotSupplier<Set<Token>> getLazyAllTabGroupIds(
            List<Tab> tabsToExclude, boolean includePendingClosures);

    /**
     * Returns a lazy oneshot supplier that generates all the root IDs except those requested to be
     * excluded.
     *
     * @param tabsToExclude The list of tabs to exclude.
     * @param includePendingClosures Whether to include pending tab closures.
     * @return A lazy oneshot supplier containing all the root IDs.
     */
    LazyOneshotSupplier<Set<Integer>> getLazyAllRootIds(
            List<Tab> tabsToExclude, boolean includePendingClosures);

    /** Returns the current title of the tab group. */
    String getTabGroupTitle(int rootId);

    /** Stores the given title for the tab group. */
    void setTabGroupTitle(int rootId, String title);

    /** Deletes the stored title for the tab group, defaulting it back to "N tabs." */
    void deleteTabGroupTitle(int rootId);

    /**
     * This method fetches tab group colors id for the specified tab group. It will be a {@link
     * TabGroupColorId} if found, otherwise a {@link TabGroupTitleUtils.INVALID_COLOR_ID} if there
     * is no color entry for the group.
     */
    int getTabGroupColor(int rootId);

    /**
     * This method fetches tab group colors for the related tab group root ID. If the color does not
     * exist, then GREY will be returned. This method is intended to be used by UI surfaces that
     * want to show a color, and they need the color returned to be valid.
     *
     * @param rootId The tab root ID whose related tab group color will be fetched if found.
     * @return The color that should be used for this group.
     */
    @TabGroupColorId
    int getTabGroupColorWithFallback(int rootId);

    /** Stores the given color for the tab group. */
    void setTabGroupColor(int rootId, @TabGroupColorId int color);

    /** Deletes the color that was recorded for the group. */
    void deleteTabGroupColor(int rootId);

    /** Returns whether the tab group is expanded or collapsed. */
    boolean getTabGroupCollapsed(int rootId);

    /** Sets whether the tab group is expanded or collapsed. */
    void setTabGroupCollapsed(int rootId, boolean isCollapsed);

    /** Deletes the record that the group is collapsed, setting it to expanded. */
    void deleteTabGroupCollapsed(int rootId);

    /** Delete the title, color and collapsed state of a tab group. */
    void deleteTabGroupVisualData(int rootId);

    /** Returns the sync ID associated with the tab group. */
    String getTabGroupSyncId(int rootId);

    /** Stores the sync ID associated with the tab group. */
    void setTabGroupSyncId(int rootId, String syncId);
}
