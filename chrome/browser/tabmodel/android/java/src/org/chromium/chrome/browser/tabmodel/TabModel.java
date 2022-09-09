// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.List;

/**
 * TabModel organizes all the open tabs and allows you to create new ones.  Regular and Incognito
 * tabs are kept in different TabModels.
 */
public interface TabModel extends TabList {
    /**
     * @return The profile associated with the current model.
     */
    public Profile getProfile();

    /**
     * Unregisters and destroys the specified tab, and then switches to the previous tab.
     * @param tab The non-null tab to close
     * @return true if the tab was found
     */
    public boolean closeTab(Tab tab);

    /**
     * Unregisters and destroys the specified tab, and then switches to the previous tab.
     *
     * @param tab The non-null tab to close
     * @param animate true iff the closing animation should be displayed
     * @param uponExit true iff the tab is being closed upon application exit (after user presses
     *                 the system back button)
     * @param canUndo Whether or not this action can be undone. If this is {@code true} and
     *                {@link #supportsPendingClosures()} is {@code true}, this {@link Tab}
     *                will not actually be closed until {@link #commitTabClosure(int)} or
     *                {@link #commitAllTabClosures()} is called, but it will be effectively removed
     *                from this list. To get a comprehensive list of all tabs, including ones that
     *                have been partially closed, use the {@link TabList} from
     *                {@link #getComprehensiveModel()}.
     * @return true if the tab was found
     */
    public boolean closeTab(Tab tab, boolean animate, boolean uponExit, boolean canUndo);

    /**
     * Unregisters and destroys the specified tab, and then switches to {@code recommendedNextTab}
     * if it is not null, otherwise switches to the previous tab.
     *
     * @param tab The non-null tab to close.
     * @param recommendedNextTab The tab to switch to if not null.
     * @param animate true iff the closing animation should be displayed.
     * @param uponExit true iff the tab is being closed upon application exit (after user presses
     *                 the system back button).
     * @param canUndo Whether or not this action can be undone. If this is {@code true} and
     *                {@link #supportsPendingClosures()} is {@code true}, this {@link Tab}
     *                will not actually be closed until {@link #commitTabClosure(int)} or
     *                {@link #commitAllTabClosures()} is called, but it will be effectively removed
     *                from this list. To get a comprehensive list of all tabs, including ones that
     *                have been partially closed, use the {@link TabList} from
     *                {@link #getComprehensiveModel()}.
     *
     * @return true if the tab was found.
     */
    public boolean closeTab(Tab tab, @Nullable Tab recommendedNextTab, boolean animate,
            boolean uponExit, boolean canUndo);

    /**
     * Returns which tab would be selected if the specified tab {@code id} were closed.
     * @param id The ID of tab which would be closed.
     * @param uponExit If the next tab is being selected upon exit or backgrounding of the app.
     * @return The id of the next tab that would be visible.
     */
    public Tab getNextTabIfClosed(int id, boolean uponExit);

    /**
     * Close multiple tabs on this model.
     * @param tabs The tabs to be closed.
     * @param canUndo Whether or not this action can be undone. If this is {@code true} and
     *                {@link #supportsPendingClosures()} is {@code true}, this {@link Tab}
     *                will not actually be closed until {@link #commitTabClosure(int)} is called for
     *                every {@link Tab} in {@code tabs} or {@link #commitAllTabClosures()} is
     *                called. However, it will be effectively removed from this list. To get a
     *                comprehensive list of all tabs, including ones that have been partially
     *                closed, use the {@link TabList} from {@link #getComprehensiveModel()}.
     */
    public void closeMultipleTabs(List<Tab> tabs, boolean canUndo);

    /**
     * Close all the tabs on this model. Same as closeAllTabs(false).
     * @deprecated in favor of the clearer {@link #closeAllTabs(boolean)}.
     */
    @Deprecated
    public void closeAllTabs();

    /**
     * Close all tabs on this model. Note this inherently supports the {@code canUndo} behavior of
     * {@link #closeMultipleTabs(List<Tab>, boolean)}.
     * @param uponExit true iff the tabs are being closed upon application exit (after user presses
     *                 the system back button).
     */
    public void closeAllTabs(boolean uponExit);

    /**
     * @return Whether or not this model supports pending closures.
     */
    public boolean supportsPendingClosures();

    /**
     * @param tabId The id of the {@link Tab} that might have a pending closure.
     * @return Whether or not the {@link Tab} specified by {@code tabId} has a pending
     *         closure.
     */
    boolean isClosurePending(int tabId);

    /**
     * Commits all pending closures, closing all tabs that had a chance to be undone.
     */
    public void commitAllTabClosures();

    /**
     * Commits a pending closure specified by {@code tabId}.
     * @param tabId The id of the {@link Tab} to commit the pending closure.
     */
    public void commitTabClosure(int tabId);

    /**
     * Cancels a pending {@link Tab} closure, bringing the tab back into this model.  Note that this
     * will select the rewound {@link Tab}.
     * @param tabId The id of the {@link Tab} to undo.
     */
    public void cancelTabClosure(int tabId);

    /**
     * Notifies observers that all tabs closure action has been completed and tabs have been
     * restored.
     */
    public void notifyAllTabsClosureUndone();

    /**
     * Restores the most recent closure, bringing the tab(s) back into their original tab model or
     * this model if the original model no longer exists.
     */
    public void openMostRecentlyClosedEntry();

    /**
     * @return The complete {@link TabList} this {@link TabModel} represents.  Note that this may
     *         be different than this actual {@link TabModel} if it supports pending closures
     *         {@link #supportsPendingClosures()}, as this will include all pending closure tabs.
     */
    public TabList getComprehensiveModel();

    /**
     * Selects a tab by its index.
     * @param i    The index of the tab to select.
     * @param type The type of selection.
     * @param skipLoadingTab Whether to skip loading the Tab.
     */
    public void setIndex(int i, final @TabSelectionType int type, boolean skipLoadingTab);

    /**
     * @return Whether this tab model is currently selected in the correspond
     *         {@link TabModelSelector}.
     */
    boolean isActiveModel();

    /**
     * Moves a tab to a new index.
     * @param id       The id of the tab to move.
     * @param newIndex The new place to put the tab.
     */
    public void moveTab(int id, int newIndex);

    /**
     * To be called when this model should be destroyed.  The model should no longer be used after
     * this.
     *
     * <p>
     * As a result of this call, all {@link Tab}s owned by this model should be destroyed.
     */
    public void destroy();

    /**
     * Adds a newly created tab to this model.
     * @param tab   The tab to be added.
     * @param index The index where the tab should be inserted. The model may override the index.
     * @param type  How the tab was opened.
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
     * @param observer The observer to be unsubscribed.
     */
    void removeObserver(TabModelObserver observer);

    /**
     * Set when tab model become active and inactive.
     * @param active Whether the tab model is active.
     *
     * TODO(crbug.com/1140125): This function is only called by TabModelSelectorBase class, so we
     * should create a package private TabModelInternal interface which inherits from TabModel.
     * TabModelInternal interface should have this method and change TabModelSelectorBase#mTabModels
     * to hold the impls.
     */
    void setActive(boolean active);
}
