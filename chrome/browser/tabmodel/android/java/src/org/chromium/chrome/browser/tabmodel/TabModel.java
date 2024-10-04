// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * TabModel organizes all the open tabs and allows you to create new ones. Regular and Incognito
 * tabs are kept in different TabModels.
 */
public interface TabModel extends TabList {
    /** Returns the profile associated with the current model. */
    Profile getProfile();

    /** Returns the matching tab that has the given id, or null if there is none. */
    @Nullable
    Tab getTabById(int tabId);

    /**
     * Closes tabs based on the provided parameters. Refer to {@link TabClosureParams} for different
     * ways to close tabs.
     *
     * @param tabClosureParams The parameters to follow when closing tabs.
     * @return Whether the tab closure succeeded (only possibly false for single tab closure).
     */
    boolean closeTabs(TabClosureParams tabClosureParams);

    /**
     * Returns which tab would be selected if the specified tab {@code id} were closed.
     *
     * @param id The ID of tab which would be closed.
     * @param uponExit If the next tab is being selected upon exit or backgrounding of the app.
     * @return The id of the next tab that would be visible.
     */
    Tab getNextTabIfClosed(int id, boolean uponExit);

    /**
     * @return Whether or not this model supports pending closures.
     */
    boolean supportsPendingClosures();

    /**
     * @param tabId The id of the {@link Tab} that might have a pending closure.
     * @return Whether or not the {@link Tab} specified by {@code tabId} has a pending closure.
     */
    boolean isClosurePending(int tabId);

    /** Commits all pending closures, closing all tabs that had a chance to be undone. */
    void commitAllTabClosures();

    /**
     * Commits a pending closure specified by {@code tabId}.
     *
     * @param tabId The id of the {@link Tab} to commit the pending closure.
     */
    void commitTabClosure(int tabId);

    /**
     * Cancels a pending {@link Tab} closure, bringing the tab back into this model. Note that this
     * will select the rewound {@link Tab}.
     *
     * @param tabId The id of the {@link Tab} to undo.
     */
    void cancelTabClosure(int tabId);

    /**
     * Notifies observers that all tabs closure action has been completed and tabs have been
     * restored.
     */
    void notifyAllTabsClosureUndone();

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
    @NonNull
    ObservableSupplier<Tab> getCurrentTabSupplier();

    /**
     * Selects a tab by its index.
     *
     * @param i The index of the tab to select.
     * @param type The type of selection.
     */
    void setIndex(int i, final @TabSelectionType int type);

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
    void moveTab(int id, int newIndex);

    /**
     * Returns a supplier for the number of tabs in this tab model. This does not count tabs that
     * are pending closure.
     */
    @NonNull
    ObservableSupplier<Integer> getTabCountSupplier();

    /**
     * Adds a newly created tab to this model.
     *
     * @param tab The tab to be added.
     * @param index The index where the tab should be inserted. The model may override the index.
     * @param type How the tab was opened.
     * @param creationState How the tab was created.
     */
    void addTab(Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState);

    /**
     * Removes the given tab from the model without destroying it. The tab should be inserted into
     * another model to avoid leaking as after this the link to the old Activity will be broken.
     * @param tab The tab to remove.
     */
    void removeTab(Tab tab);

    /**
     * Subscribes a {@link TabModelObserver} to be notified about changes to this model.
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
     * Returns the count of non-custom tabs that have a {@link
     * Tab#getLastNavigationCommittedTimestampMillis()} within the time range [beginTimeMs,
     * endTimeMs).
     */
    int getTabCountNavigatedInTimeWindow(long beginTimeMs, long endTimeMs);

    /**
     * Closes non-custom tabs that have a {@link Tab#getLastNavigationCommittedTimestampMillis()}
     * within the time range [beginTimeMs, endTimeMs).
     */
    void closeTabsNavigatedInTimeWindow(long beginTimeMs, long endTimeMs);
}
