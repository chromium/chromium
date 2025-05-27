// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * TabModel organizes all the open tabs and allows you to create new ones. Regular and Incognito
 * tabs are kept in different TabModels.
 */
@NullMarked
public interface TabModel extends SupportsTabModelObserver, TabList {
    /** Returns the profile associated with the current model. */
    @Nullable Profile getProfile();

    /** Returns the matching tab that has the given id, or null if there is none. */
    @Nullable Tab getTabById(int tabId);

    /** Returns the matching tab that has the given id, or null if there is none. */
    default Tab getTabByIdChecked(int tabId) {
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
    @Nullable Tab getNextTabIfClosed(int id, boolean uponExit);

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
}
