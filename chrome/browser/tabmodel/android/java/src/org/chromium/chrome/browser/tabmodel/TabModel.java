// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.tabs.TabStripCollection;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * TabModel organizes all the open tabs and allows you to create new ones. Regular and Incognito
 * tabs are kept in different TabModels.
 */
@NullMarked
public interface TabModel extends SupportsTabModelObserver, TabList {
    Map<Integer, Long> sTabPinTimestampMap = new HashMap<>();

    /** Returns the profile associated with the current model. */
    @Nullable Profile getProfile();

    /**
     * Associates this tab model with a browser window. This should be called shortly after startup
     * to associate the tab model with a AndroidBrowserWindow.
     *
     * @param nativeAndroidBrowserWindow The native AndroidBrowserWindow pointer.
     */
    void associateWithBrowserWindow(long nativeAndroidBrowserWindow);

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
    ObservableSupplier<@Nullable Tab> getCurrentTabSupplier();

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
    ObservableSupplier<Integer> getTabCountSupplier();

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
}
