// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.jni_zero.JniPtr;

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
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabStripCollection;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
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

    /**
     * Returns whether the given {@link TabModelType} is dormant ({@link TabModelType#ARCHIVED} or
     * {@link TabModelType#HEADLESS}). Tabs in dormant models never hold a {@code WebContents}, and
     * {@link TabModelType#HEADLESS} is intended to become strictly read-only in the future.
     */
    static boolean isDormantTabModel(@TabModelType int tabModelType) {
        return tabModelType == TabModelType.ARCHIVED || tabModelType == TabModelType.HEADLESS;
    }

    /** Returns the profile associated with the current model. */
    @Nullable Profile getProfile();

    /**
     * Associates this tab model with a native {@code AndroidBrowserWindow}. This should be called
     * shortly after startup. Note that this is model-level window association, distinct from
     * per-tab window attachment ({@link Tab#isDetachedFromActivity()}) and model attachment ({@link
     * Tab#hasParentCollection()}).
     *
     * @param nativeAndroidBrowserWindow The native AndroidBrowserWindow pointer.
     */
    void associateWithBrowserWindow(long nativeAndroidBrowserWindow);

    /**
     * Dissociates this tab model from a native {@code AndroidBrowserWindow}. This should be called
     * before the browser window is destroyed. Note that this is model-level window association,
     * distinct from per-tab window attachment ({@link Tab#isDetachedFromActivity()}) and model
     * attachment ({@link Tab#hasParentCollection()}).
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
     * Returns which tab would become the current (active) tab if the specified tab {@code id} were
     * closed.
     *
     * @param id The ID of tab which would be closed.
     * @param uponExit If the next tab is being selected upon exit or backgrounding of the app.
     * @return The next {@link Tab} that would become active, or {@code null} if none.
     */
    @Nullable Tab getNextTabIfClosed(@TabId int id, boolean uponExit);

    /**
     * Finds the next tab to activate hierarchically (child -> sibling -> parent) when closing the
     * given tab.
     *
     * @param closingTab The {@link Tab} that is closing.
     * @param closingTabs The collection of all tabs that are closing.
     * @return The next {@link Tab} to activate, or {@code null} if no candidate is found.
     */
    default @Nullable Tab getHierarchicalNextTab(Tab closingTab, List<Tab> closingTabs) {
        return null;
    }

    boolean supportsPendingClosures();

    /**
     * Returns whether the tab with {@code tabId} has a pending closure.
     *
     * @param tabId The id of the {@link Tab} that might have a pending closure.
     */
    boolean isClosurePending(@TabId int tabId);

    /**
     * Commits all pending closures, closing all tabs that had a chance to be undone, and notifies
     * observers to dismiss any active undo affordances.
     */
    void commitAllTabClosures();

    /**
     * Commits a pending closure specified by {@code tabId}.
     *
     * @param tabId The id of the {@link Tab} to commit the pending closure.
     */
    void commitTabClosure(@TabId int tabId);

    /**
     * Cancels a pending {@link Tab} closure, bringing the tab back into this model. Note that this
     * will activate the rewound {@link Tab} if no tab is currently active in the model.
     *
     * @param tabId The id of the {@link Tab} to undo.
     */
    void cancelTabClosure(@TabId int tabId);

    /**
     * Restores the closed tabs captured by {@code snapshot} when the user triggers undo.
     *
     * @param snapshot The closure snapshot captured at close time.
     * @param reselect Whether to reselect a tab that was selected prior to closure.
     * @return Restored tab IDs using the same indexing as {@code snapshot.tabs}, i.e. element
     *     {@code i} is the restored ID of {@code snapshot.tabs.get(i)}, or {@link
     *     Tab#INVALID_TAB_ID} if that tab could not be restored.
     */
    default int[] restoreClosedTabsForUndo(UndoClosureSnapshot snapshot, boolean reselect) {
        int[] result = new int[snapshot.tabs.size()];
        Arrays.fill(result, Tab.INVALID_TAB_ID);
        return result;
    }

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
     * Returns the complete (rewound) {@link TabList} this {@link TabModel} represents. Note that
     * this may be different than this actual {@link TabModel} if it supports pending closures
     * {@link #supportsPendingClosures()}, as this will include all tabs with pending undoable
     * closures from {@code PendingTabClosureManager} (where {@link Tab#isClosing()} is true).
     */
    TabList getComprehensiveModel();

    /**
     * Returns a supplier of the current (active) {@link Tab}. The tab may be null if no tab is
     * present in the model or no tab is active. The contained tab should always match the result of
     * {@code getTabAt(index())}.
     */
    NullableObservableSupplier<Tab> getCurrentTabSupplier();

    /** Returns the supplier of the next tab policy. */
    NextTabPolicySupplier getNextTabPolicySupplier();

    /**
     * Activates a tab by its index (setting it as the current tab).
     *
     * @param i The index of the tab to activate.
     * @param type The type of selection.
     */
    void setIndex(int i, @TabSelectionType int type);

    /**
     * Returns whether this tab model is currently the active model in the corresponding {@link
     * TabModelSelector}.
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
    void pinTab(int tabId, boolean showUngroupDialog);

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
     * Checks if a tab is part of the multi-selection set. Multi-selection is distinct from
     * single-tab activation, though {@link TabModel}'s multi-selection set implicitly includes the
     * current (active) tab when a tab is active (and not in an {@link TabModelType#ARCHIVED} model,
     * where {@link #index()} is {@link TabList#INVALID_TAB_INDEX}) in addition to any tabs
     * explicitly added to the multi-selection group.
     *
     * @param tabId The ID of the tab to check.
     * @return true if the tab is part of the multi-selection set, false otherwise.
     */
    boolean isTabMultiSelected(int tabId);

    /**
     * Gets the total number of tabs in the multi-selection set. Multi-selection is distinct from
     * single-tab activation, though {@link TabModel}'s multi-selection set implicitly includes the
     * current (active) tab plus any other tabs explicitly added to the multi-selection group. If no
     * other tabs are multi-selected, this will return 1 when an active tab is present ({@code
     * getCurrentTabSupplier().get() != null}), or 0 if no tab is currently active (e.g. when the
     * model is empty or during background restore before initialization completes).
     *
     * @return The total count of tabs in the multi-selection set.
     */
    int getMultiSelectedTabsCount();

    /**
     * Gets the list of multi-selected tab ids in the order they were selected.
     *
     * @return The list of ordered selected tab ids.
     */
    List<Integer> getOrderedMultiSelectedTabIds();

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

    /** Returns the number of pinned tabs in this model. */
    int getPinnedTabsCount();

    /** Returns the native {@code SessionID} as returned by {@code tab_model.h:GetSessionId()}. */
    @Nullable Integer getNativeSessionId();

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
    @Nullable JniPtr<TabStripCollection> getTabStripCollection();

    /** Returns {@link ActivityType} of the this tab model. */
    default int getActivityTypeForTesting() {
        // Return an invalid type for the implementation to fail tests by default.
        return -1;
    }

    /** Duplicates the given tab. */
    @Nullable Tab duplicateTab(Tab tab);

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
     * This method adds a {@link TabGroupObserver} to be notified on tab group changes.
     *
     * @param observer The {@link TabGroupObserver} to add.
     */
    void addTabGroupObserver(TabGroupObserver observer);

    /**
     * This method removes a {@link TabGroupObserver}.
     *
     * @param observer The {@link TabGroupObserver} to remove.
     */
    void removeTabGroupObserver(TabGroupObserver observer);

    /**
     * Returns a list of {@link Tab} objects with a tab group being represented by one tab from its
     * tab group.
     *
     * @deprecated Do not use representative tabs to model tab groups. Iterate over {@link TabModel}
     *     directly and group tabs by {@link Tab#getTabGroupId()}, or query {@link
     *     #getAllTabGroupIds()} and {@link #getTabsInGroup(Token)}.
     */
    // TODO(crbug.com/517544602): Delete when removing the flag.
    @Deprecated
    List<Tab> getRepresentativeTabList();

    /**
     * Returns the number of individual tabs plus tab groups in the filter.
     *
     * @deprecated Do not rely on collapsed representative counts in {@link TabModel}. Query {@link
     *     #getCount()} or {@link #getTabGroupCount()} directly.
     */
    // TODO(crbug.com/517544602): Delete when removing the flag.
    @Deprecated
    int getIndividualTabAndGroupCount();

    /**
     * Returns the representative tab for an index or null if one does not exist. For an individual
     * tab this is the tab itself. For a tab group this is the most recently shown/activated tab in
     * the group.
     *
     * @deprecated Do not index into collapsed representative tab lists. Use {@link #getTabAt(int)}
     *     for flat tab model indices or {@link #getTabsInGroup(Token)} for tab group members.
     */
    // TODO(crbug.com/517544602): Delete when removing the flag.
    @Deprecated
    @Nullable Tab getRepresentativeTabAt(int index);

    /**
     * Returns the index of the individual tab or the tab group it belongs to.
     *
     * @deprecated Do not use collapsed representative indices. Use {@link #indexOf(Tab)} for model
     *     indices or {@link #getIndexOfTabInGroup(Tab)} for within-group indices.
     */
    // TODO(crbug.com/517544602): Delete when removing the flag.
    @Deprecated
    int representativeIndexOf(@Nullable Tab tab);

    /** Returns the number of tab groups. */
    int getTabGroupCount();

    /**
     * Returns the number of tabs in the tab group with {@code tabGroupId} or 0 if the tab group
     * does not exist.
     */
    int getTabCountForGroup(@Nullable Token tabGroupId);

    /** Returns whether a tab group exists with {@code tabGroupId}. */
    boolean containsTabGroup(@Nullable Token tabGroupId);

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

    /**
     * Returns the ID of the most recently shown/activated tab in the tab group with {@code
     * tabGroupId}.
     *
     * @deprecated Do not rely on mutable last-shown representative tab state for tab groups.
     *     Operate on {@link Token} {@code tabGroupId} and {@link #getTabsInGroup(Token)} directly,
     *     or use {@link TabGroupUtils#getSelectedOrFirstTabInGroup(TabModel, Token)} or {@link
     *     TabGroupUtils#getFirstTabIdInGroup(TabModel, Token)}.
     */
    // TODO(crbug.com/517544602): Delete once sAndroidTabUiRefactor launches.
    @Deprecated
    @TabId
    int getGroupLastShownTabId(@Nullable Token tabGroupId);

    /**
     * Moves the tab group with {@code tabGroupId} to the position with {@code newIndex} in
     * TabModel.
     *
     * @param tabGroupId The {@link Token} ID of the tab group being moved.
     * @param newIndex The new index in TabModel that the group is being moved to.
     */
    void moveGroupToIndex(Token tabGroupId, int newIndex);

    /**
     * Creates a tab group containing a single tab.
     *
     * @param tab The tab to create the group for.
     */
    void createSingleTabGroup(Tab tab);

    /**
     * Creates a tab group with a preallocated {@link Token} for the TabGroupId.
     *
     * <p>This should only be used by the tab group sync service and related code. Ideally, this
     * would be locked down using a mechanism like {@code friend class} or some sort of access
     * token. However, for now this disclaimer will suffice.
     *
     * @param tabs The list of tabs to make a tab group from. An empty list will no-op.
     * @param tabGroupId An externally minted tab group id token.
     */
    void createTabGroupForTabGroupSync(List<Tab> tabs, Token tabGroupId);

    /**
    /**
     * Appends a list of {@link Tab}s to the destination group that contains the {@code
     * destinationTab}. The {@link TabModel} ordering of the tabs in the given list is not
     * preserved. After calling this method, the {@link TabModel} ordering of these tabs would
     * become the ordering of {@code tabs}.
     *
     * @param tabs List of {@link Tab}s to be appended.
     * @param destinationTab The destination {@link Tab} to be append to.
     * @param notify Whether or not to notify observers about the merging events.
     */
    default void mergeListOfTabsToGroup(
            List<Tab> tabs, Tab destinationTab, @TabGroupMergeNotificationType int notify) {
        mergeListOfTabsToGroup(tabs, destinationTab, /* indexInGroup= */ null, notify);
    }

    /**
     * Merges a list of {@link Tab}s into the destination group that contains the {@code
     * destinationTab}.
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

    /**
     * Merges a list of {@link Tab}s into the existing destination group identified by {@code
     * destinationGroupId}. No-ops if the group does not exist in the model.
     *
     * @param tabs List of {@link Tab}s to be merged. The ordering of this list is preserved.
     * @param destinationGroupId The destination tab group ID {@link Token}.
     * @param indexInGroup The index within the destination group to insert the tabs (clamped to the
     *     valid range [0, group_size]), or {@code null} to append at the end.
     * @param notify Whether or not to notify observers about the merging events.
     */
    void mergeListOfTabsToExistingGroup(
            List<Tab> tabs,
            Token destinationGroupId,
            @Nullable Integer indexInGroup,
            @TabGroupMergeNotificationType int notify);

    /** Returns a utility interface to help with that ungrouping tabs from a tab group. */
    TabUngrouper getTabUngrouper();

    // TODO(crbug.com/372068933): This method should probably have more restricted access.
    /**
     * Undoes a group operation.
     *
     * @param undoGroupMetadata Metadata to undo the operation provided by {@link
     *     TabGroupObserver#showUndoGroupSnackbar}.
     */
    void performUndoGroupOperation(UndoGroupMetadata undoGroupMetadata);

    /**
     * Notifies that the undo window for a group operation has expired.
     *
     * @param undoGroupMetadata Metadata to undo the operation provided by {@link
     *     TabGroupObserver#showUndoGroupSnackbar}.
     */
    void undoGroupOperationExpired(UndoGroupMetadata undoGroupMetadata);

    /** Get all tab group IDs that are associated with tab groups. */
    Set<Token> getAllTabGroupIds();

    /** Returns whether the tab state is initialized. */
    boolean isTabStateInitialized();

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
     * This method fetches tab group colors for the specified tab group. If the color does not
     * exist, then GREY will be returned. This method is intended to be used by UI surfaces that
     * want to show a color, and they need the color returned to be valid.
     *
     * @param tabGroupId The tab group ID whose tab group color will be fetched if found.
     * @return The color that should be used for this group.
     */
    @TabGroupColorId
    int getTabGroupColorWithFallback(Token tabGroupId);

    /** Stores the given color for the tab group. */
    void setTabGroupColor(Token tabGroupId, @TabGroupColorId int color);

    /** Returns whether the tab group is expanded or collapsed. */
    boolean getTabGroupCollapsed(Token tabGroupId);

    /** Sets whether the tab group is expanded or collapsed. */
    default void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed) {
        setTabGroupCollapsed(tabGroupId, isCollapsed, /* animate= */ false);
    }

    /** Sets whether the tab group is expanded or collapsed, with optional animation. */
    void setTabGroupCollapsed(Token tabGroupId, boolean isCollapsed, boolean animate);
}
