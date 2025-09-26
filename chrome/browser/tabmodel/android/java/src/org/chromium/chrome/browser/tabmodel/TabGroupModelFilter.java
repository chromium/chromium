// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.List;
import java.util.Set;

/** Interface for getting tab groups for the tabs in the {@link TabModel}. */
@NullMarked
public interface TabGroupModelFilter extends SupportsTabModelObserver {

    @IntDef({
        MergeNotificationType.DONT_NOTIFY,
        MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP,
        MergeNotificationType.NOTIFY_ALWAYS
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface MergeNotificationType {
        int DONT_NOTIFY = 0;
        int NOTIFY_IF_NOT_NEW_GROUP = 1;
        int NOTIFY_ALWAYS = 2;
    }

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
    TabModel getTabModel();

    /**
     * Returns a list of {@link Tab} objects with a tab group being represented by one tab from its
     * tab group.
     */
    List<Tab> getRepresentativeTabList();

    /** Returns the number of individual tabs plus tab groups in the filter. */
    int getIndividualTabAndGroupCount();

    /**
     * Returns the current representative tab's index or {@link TabModel.INVALID_TAB_INDEX} if none
     * is selected.
     */
    int getCurrentRepresentativeTabIndex();

    /** Returns the current representative tab or null if none is selected. */
    @Nullable Tab getCurrentRepresentativeTab();

    /**
     * Returns the representative tab for an index or null if one does not exist. For an individual
     * tab this is the tab itself. For a tab group this is the most recently selected tab in the
     * group.
     */
    @Nullable Tab getRepresentativeTabAt(int index);

    /** Returns the index of the individual tab or the tab group it belongs to. */
    int representativeIndexOf(@Nullable Tab tab);

    /** Returns the number of tab groups. */
    int getTabGroupCount();

    /**
     * Returns the number of tabs in the tab group with {@code tabGroupId} or 0 if the tab group
     * does not exist.
     */
    int getTabCountForGroup(@Nullable Token tabGroupId);

    /** Returns whether a tab group exists with {@code tabGroupId}. */
    boolean tabGroupExists(@Nullable Token tabGroupId);

    /**
     * Returns the list of {@link Tab}s that are grouped with the given {@code tabId}.
     *
     * @param tabId The id of a {@link Tab} in the group.
     * @return An unmodifiable list of {@link Tab}s that are grouped, or a list containing only the
     *     given tab if the tab is not in a group.
     */
    List<Tab> getRelatedTabList(@TabId int tabId);

    /** Returns the list of tabs in a tab group or an empty list if the group does not exist. */
    List<Tab> getTabsInGroup(@Nullable Token tabGroupId);

    /** Returns whether the given {@link Tab} is in a tab group. */
    boolean isTabInTabGroup(Tab tab);

    /** Returns the position of the given {@link Tab} in its group. */
    int getIndexOfTabInGroup(Tab tab);

    /** Returns the last shown tab id in the tab group with {@code tabGroupId}. */
    @TabId
    int getGroupLastShownTabId(@Nullable Token tabGroupId);

    /**
     * This method moves the tab group which contains the tab with tab {@code id} to {@code
     * newIndex} in TabModel.
     *
     * @param id The id of the tab whose related tabs are being moved.
     * @param newIndex The new index in TabModel that these tabs are being moved to.
     */
    void moveRelatedTabs(@TabId int id, int newIndex);

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
    default void createSingleTabGroup(@TabId int tabId) {
        createSingleTabGroup(getTabModel().getTabByIdChecked(tabId));
    }

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
    void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId);

    /**
     * This method merges the source group that contains the {@code sourceTabId} to the destination
     * group that contains the {@code destinationTabId}. This method only operates if two groups are
     * in the same {@code TabModel}.
     *
     * @param sourceTabId The id of the {@link Tab} to get the source group.
     * @param destinationTabId The id of a {@link Tab} to get the destination group.
     */
    default void mergeTabsToGroup(@TabId int sourceTabId, @TabId int destinationTabId) {
        mergeTabsToGroup(sourceTabId, destinationTabId, /* skipUpdateTabModel= */ false);
    }

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
    void mergeTabsToGroup(
            @TabId int sourceTabId, @TabId int destinationTabId, boolean skipUpdateTabModel);

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
    default void mergeListOfTabsToGroup(
            List<Tab> tabs, Tab destinationTab, @MergeNotificationType int notify) {
        mergeListOfTabsToGroup(tabs, destinationTab, /* index=InGroup */ null, notify);
    }

    /**
     * This method merges a list of {@link Tab}s into the destination group that contains the
     * {@code} destinationTab.
     *
     * @param tabs List of {@link Tab}s to be merged. The ordering of this list is preserved.
     * @param destinationTab The destination {@link Tab}. If not in a group, a new group will be
     *     created.
     * @param indexInGroup The index within the destination group to insert the tabs.
     *     <ul>
     *       <li>0 - inserts at the front.
     *       <li>`null` or an index >= group size - inserts at the back.
     *       <li>Any other value is clamped to the valid range [0, group_size].
     *     </ul>
     *
     * @param notify Whether or not to notify observers about the merging events.
     */
    void mergeListOfTabsToGroup(
            List<Tab> tabs,
            Tab destinationTab,
            @Nullable Integer indexInGroup,
            @MergeNotificationType int notify);

    /** Returns a utility interface to help with that ungrouping tabs from a tab group. */
    TabUngrouper getTabUngrouper();

    // TODO(crbug.com/372068933): This method should probably have more restricted access.
    /**
     * Undoes a group operation performed by this TabGroupModelFilter.
     *
     * @param undoGroupMetadata Metadata to undo the operation provided by {@link
     *     TabGroupModelFilterObserver#showUndoGroupSnackbar}.
     */
    void performUndoGroupOperation(UndoGroupMetadata undoGroupMetadata);

    /**
     * Notifies that the undo window for a group operation performed by this TabGroupModelFilter has
     * expired.
     *
     * @param undoGroupMetadata Metadata to undo the operation provided by {@link
     *     TabGroupModelFilterObserver#showUndoGroupSnackbar}.
     */
    void undoGroupOperationExpired(UndoGroupMetadata undoGroupMetadata);

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
     * Returns the current title of the tab group or null if a title is not set. Prefer {@link
     * TabGroupTitleUtils#getDisplayableTitle} in most cases.
     */
    @Nullable String getTabGroupTitle(Token tabGroupId);

    /**
     * {@see #getTabGroupTitle(Token)}. This looks up the tab group via {@code groupedTab}. This is
     * primarily to be used if the tab group has already been closed. Prefer the {@link
     * TabGroupTitleUtils#getDisplayableTitle} or {@link #getTabGroupTitle(Token)} method in most
     * cases.
     */
    @Nullable String getTabGroupTitle(Tab groupedTab);

    /** Stores the given title for the tab group. */
    void setTabGroupTitle(Token tabGroupId, @Nullable String title);

    /** Deletes the stored title for the tab group, defaulting it back to "N tabs." */
    void deleteTabGroupTitle(Token tabGroupId);

    /**
     * This method fetches tab group colors id for the specified tab group. It will be a {@link
     * TabGroupColorId} if found, otherwise a {@link TabGroupColorUtils.INVALID_COLOR_ID} if there
     * is no color entry for the group.
     */
    int getTabGroupColor(Token tabGroupId);

    /**
     * This method fetches tab group colors for the related tab group root ID. If the color does not
     * exist, then GREY will be returned. This method is intended to be used by UI surfaces that
     * want to show a color, and they need the color returned to be valid.
     *
     * @param tabGroupId The tab group ID whose related tab group color will be fetched if found.
     * @return The color that should be used for this group.
     */
    @TabGroupColorId
    int getTabGroupColorWithFallback(Token tabGroupId);

    /**
     * {@see #getTabGroupColorWithFallback(Token)}. This looks up the tab group via {@code
     * groupedTab}. This is primarily to be used if the tab group has already been closed. Prefer
     * the {@link #getTabGroupColorWithFallback(Token)} method in most cases.
     */
    @TabGroupColorId
    int getTabGroupColorWithFallback(Tab groupedTab);

    /** Stores the given color for the tab group. */
    void setTabGroupColor(Token tabGroupId, @TabGroupColorId int color);

    /** Deletes the color that was recorded for the group. */
    void deleteTabGroupColor(Token tabGroupId);

    /** Returns whether the tab group is expanded or collapsed. */
    boolean getTabGroupCollapsed(Token tabGroupId);

    /** Sets whether the tab group is expanded or collapsed. */
    default void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed) {
        setTabGroupCollapsed(tabGroupId, isCollapsed, /* animate= */ false);
    }

    /** Sets whether the tab group is expanded or collapsed, with optional animation. */
    void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate);

    /** Deletes the record that the group is collapsed, setting it to expanded. */
    void deleteTabGroupCollapsed(Token tabGroupId);
}
