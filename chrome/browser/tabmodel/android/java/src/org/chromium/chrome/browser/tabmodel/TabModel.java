// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabStripCollection;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * TabModel organizes all the open tabs and allows you to create new ones. Regular and Incognito
 * tabs are kept in different TabModels.
 */
@NullMarked
public interface TabModel extends TabList {
    static final long INVALID_TIMESTAMP = -1L;
    Map<Integer, Long> sTabPinTimestampMap = new HashMap<>();

    @IntDef({
        RecentlyClosedEntryType.NONE,
        RecentlyClosedEntryType.TAB,
        RecentlyClosedEntryType.TABS,
        RecentlyClosedEntryType.GROUP
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface RecentlyClosedEntryType {
        int NONE = 0;
        int TAB = 1;
        int TABS = 2;
        int GROUP = 3;
    }

    /** Returns the {@link TabModelType} of this tab model. */
    @TabModelType
    int getTabModelType();

    /** Returns the profile associated with the current model. */
    @Nullable Profile getProfile();

    /**
     * Associates this tab model with a browser window. This should be called shortly after startup
     * to associate the tab model with a AndroidBrowserWindow.
     *
     * @param nativeAndroidBrowserWindow The native AndroidBrowserWindow pointer.
     */
    void associateWithBrowserWindow(long nativeAndroidBrowserWindow);

    /**
     * Dissociates this tab model from a browser window. This should be called before the browser
     * window is destroyed.
     */
    void dissociateWithBrowserWindow();

    /** Returns the matching tab that has the given id, or null if there is none. */
    @Nullable Tab getTabById(@TabId int tabId);

    /** Returns the matching tab that has the given id, or null if there is none. */
    default Tab getTabByIdChecked(@TabId int tabId) {
        Tab t = getTabById(tabId);
        assert t != null;
        return t;
    }

    /** Returns the tab remover for this tab model. */
    TabRemover getTabRemover();

    /**
     * Returns which tab would be selected if the specified tab {@code id} were closed.
     *
     * @param id The ID of tab which would be closed.
     * @param uponExit If the next tab is being selected upon exit or backgrounding of the app.
     * @return The id of the next tab that would be visible.
     */
    @Nullable Tab getNextTabIfClosed(@TabId int id, boolean uponExit);

    /**
     * @return Whether or not this model supports pending closures.
     */
    boolean supportsPendingClosures();

    /**
     * @param tabId The id of the {@link Tab} that might have a pending closure.
     * @return Whether or not the {@link Tab} specified by {@code tabId} has a pending closure.
     */
    boolean isClosurePending(@TabId int tabId);

    /** Commits all pending closures, closing all tabs that had a chance to be undone. */
    void commitAllTabClosures();

    /**
     * Commits a pending closure specified by {@code tabId}.
     *
     * @param tabId The id of the {@link Tab} to commit the pending closure.
     */
    void commitTabClosure(@TabId int tabId);

    /**
     * Cancels a pending {@link Tab} closure, bringing the tab back into this model. Note that this
     * will select the rewound {@link Tab}.
     *
     * @param tabId The id of the {@link Tab} to undo.
     */
    void cancelTabClosure(@TabId int tabId);

    /**
     * Restores the most recent closure, bringing the tab(s) back into their original tab model or
     * this model if the original model no longer exists.
     */
    void openMostRecentlyClosedEntry();

    /** Returns the type of the most recently closed entry. */
    @RecentlyClosedEntryType
    int getMostRecentlyClosedEntryType();

    /**
     * Gets the timestamp of the most recent tab closure event. If a valid, non-zero timestamp is
     * not available, this should return {@link TabModel#INVALID_TIMESTAMP}.
     *
     * @return The closure timestamp, in millis.
     */
    long getMostRecentClosureTime();

    /**
     * @return The complete {@link TabList} this {@link TabModel} represents. Note that this may be
     *     different than this actual {@link TabModel} if it supports pending closures {@link
     *     #supportsPendingClosures()}, as this will include all pending closure tabs.
     */
    TabList getComprehensiveModel();

    /**
     * Returns a supplier of the current {@link Tab}. The tab may be null if no tab is present in
     * the model or selected. The contained tab should always match the result of {@code
     * getTabAt(index())}.
     */
    NullableObservableSupplier<Tab> getCurrentTabSupplier();

    /**
     * Selects a tab by its index.
     *
     * @param i The index of the tab to select.
     * @param type The type of selection.
     */
    void setIndex(int i, @TabSelectionType int type);

    /**
     * @return Whether this tab model is currently selected in the correspond {@link
     *     TabModelSelector}.
     */
    boolean isActiveModel();

    /**
     * Moves a tab to a new index.
     *
     * @param id The id of the tab to move.
     * @param newIndex The new place to put the tab.
     */
    void moveTab(@TabId int id, int newIndex);

    /**
     * Pins a tab to the model.
     *
     * @param tabId The id of the tab to pin.
     * @param showUngroupDialog Whether to possibly show a dialog to the user when pinning the last
     *     tab in a group.
     */
    default void pinTab(int tabId, boolean showUngroupDialog) {
        pinTab(tabId, showUngroupDialog, /* tabModelActionListener= */ null);
    }

    /**
     * Pins a tab to the model.
     *
     * @param tabId The id of the tab to pin.
     * @param showUngroupDialog Whether to possibly show a dialog to the user when pinning the last
     *     tab in a group.
     * @param tabModelActionListener A listener that is notified in response to the user actions
     *     taken in the ungroup dialog (if shown).
     */
    void pinTab(
            int tabId,
            boolean showUngroupDialog,
            @Nullable TabModelActionListener tabModelActionListener);

    /**
     * Unpins a tab from the model.
     *
     * @param tabId The id of the tab to unpin.
     */
    void unpinTab(int tabId);

    /**
     * Returns a supplier for the number of tabs in this tab model. This does not count tabs that
     * are pending closure.
     */
    NonNullObservableSupplier<Integer> getTabCountSupplier();

    /** Returns the tab creator for this tab model. */
    TabCreator getTabCreator();

    /**
     * Adds a newly created tab to this model.
     *
     * @param tab The tab to be added.
     * @param index The index where the tab should be inserted. The model may override the index.
     * @param type How the tab was opened.
     * @param creationState How the tab was created.
     */
    void addTab(Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState);

    /** Broadcast a native-side notification that all tabs are now loaded from storage. */
    void broadcastSessionRestoreComplete();

    /**
     * Sets the multi-selected state for a collection of tabs in a single batch operation.
     *
     * @param tabIds A Set of tab IDs to either add to or remove from the multi-selection.
     * @param isSelected If true, the tab IDs will be added to the selection; if false, they will be
     *     removed.
     */
    void setTabsMultiSelected(Set<Integer> tabIds, boolean isSelected);

    /**
     * Clears the entire multi-selection set.
     *
     * @param notifyObservers If true, observers will be notified of the change. This can be set to
     *     false to avoid redundant notifications when this clear is part of a larger operation.
     */
    void clearMultiSelection(boolean notifyObservers);

    /**
     * Checks if a tab is part of the current selection. A tab is considered selected if it is
     * either the currently active tab or has been explicitly added to the multi-selection group.
     *
     * @param tabId The ID of the tab to check.
     * @return true if the tab is selected, false otherwise.
     */
    boolean isTabMultiSelected(int tabId);

    /**
     * Gets the total number of selected tabs. This includes the currently active tab plus any other
     * tabs explicitly added to the multi-selection group. If no tabs are multi-selected, this will
     * return 1 (for the active tab). If there are no tabs in the model, this will return 0.
     *
     * @return The total count of selected tabs.
     */
    int getMultiSelectedTabsCount();

    /**
     * Gets the list of multi-selected tabs in the order they were selected.
     *
     * @return The list of ordered selected tabs.
     */
    List<Tab> getOrderedMultiSelectedTabs();

    /**
     * Returns the index of the first non-pinned tab in the model.
     *
     * @return The index of the first non-pinned tab, or the model count if all tabs are pinned.
     */
    int findFirstNonPinnedTabIndex();

    /**
     * @return The number of pinned tabs in this model.
     */
    int getPinnedTabsCount();

    /** Returns the native {@code SessionID} as returned by {@code tab_model.h:GetSessionId()}. */
    @Nullable Integer getNativeSessionIdForTesting();

    /**
     * Sets the mute setting for the sites of the provided tabs.
     *
     * @param tabs The list of {@link Tab}s whose sites will have their sound setting changed.
     * @param mute If true, it will block sound (muted); if false, it will allow sound (unmuted).
     */
    void setMuteSetting(List<Tab> tabs, boolean mute);

    /**
     * Returns whether a tab is muted. This is determined by the audio state of the WebContents if
     * it's available, otherwise it falls back to the sound content setting for the tab's URL.
     *
     * @param tab The {@link Tab} to check.
     */
    boolean isMuted(Tab tab);

    private static long getCurrentTimeMillis() {
        return System.currentTimeMillis();
    }

    /**
     * Records the timestamp when a tab is pinned.
     *
     * @param tab The tab that was pinned.
     */
    default void recordPinTimestamp(Tab tab) {
        sTabPinTimestampMap.put(tab.getId(), getCurrentTimeMillis());
    }

    /**
     * Records the duration for which a tab was pinned. This is called when a tab is unpinned. If a
     * timestamp for the tab's pinning exists, it calculates the duration and records it to a
     * histogram.
     *
     * @param tab The tab that was unpinned.
     */
    default void recordPinnedDuration(Tab tab) {
        if (sTabPinTimestampMap.containsKey(tab.getId())) {
            long pinTimestamp = sTabPinTimestampMap.get(tab.getId());
            long duration = getCurrentTimeMillis() - pinTimestamp;
            RecordHistogram.recordLongTimesHistogram100("Tab.PinnedDuration", duration);
            sTabPinTimestampMap.remove(tab.getId());
        }
    }

    /**
     * Returns the {@link TabStripCollection} associated with this {@link TabModel} if tab
     * collections are enabled. Otherwise, returns null.
     */
    @Nullable TabStripCollection getTabStripCollection();

    /** Returns {@link ActivityType} of the this tab model. */
    default int getActivityTypeForTesting() {
        // Return an invalid type for the implementation to fail tests by default.
        return -1;
    }

    /** Duplicates the given tab. */
    @Nullable Tab duplicateTab(Tab tab);

    /** Whether the model is currently in the process of closing all of its tabs. */
    boolean isClosingAllTabs();

    // ---------------------------------------------------------------------------------------------
    // Tab group methods.
    // ---------------------------------------------------------------------------------------------

    /**
     * Subscribes a {@link TabModelObserver} to be notified about changes to a {@link TabModel}.
     *
     * @param observer The observer to be subscribed.
     */
    void addObserver(TabModelObserver observer);

    /**
     * Unsubscribes a previously subscribed {@link TabModelObserver}.
     *
     * @param observer The observer to be unsubscribed.
     */
    void removeObserver(TabModelObserver observer);

    /**
     * This method adds a {@link TabGroupModelFilterObserver} to be notified on tab group changes.
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
        createSingleTabGroup(getTabByIdChecked(tabId));
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
            List<Tab> tabs, Tab destinationTab, @TabGroupMergeNotificationType int notify) {
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
            @TabGroupMergeNotificationType int notify);

    /** Returns a utility interface to help with that ungrouping tabs from a tab group. */
    TabUngrouper getTabUngrouper();

    // TODO(crbug.com/372068933): This method should probably have more restricted access.
    /**
     * Undoes a group operation.
     *
     * @param undoGroupMetadata Metadata to undo the operation provided by {@link
     *     TabGroupModelFilterObserver#showUndoGroupSnackbar}.
     */
    void performUndoGroupOperation(UndoGroupMetadata undoGroupMetadata);

    /**
     * Notifies that the undo window for a group operation has expired.
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
     * Returns the current title of the tab group, or {@link
     * TabGroupTitleUtils#UNSET_TAB_GROUP_TITLE} if a title is not set. Prefer {@link
     * TabGroupTitleUtils#getDisplayableTitle} in most cases.
     */
    String getTabGroupTitle(Token tabGroupId);

    /**
     * {@see #getTabGroupTitle(Token)}. This looks up the tab group via {@code groupedTab}. This is
     * primarily to be used if the tab group has already been closed. Prefer the {@link
     * TabGroupTitleUtils#getDisplayableTitle} or {@link #getTabGroupTitle(Token)} method in most
     * cases.
     */
    String getTabGroupTitle(Tab groupedTab);

    /** Stores the given title for the tab group. */
    void setTabGroupTitle(Token tabGroupId, String title);

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
