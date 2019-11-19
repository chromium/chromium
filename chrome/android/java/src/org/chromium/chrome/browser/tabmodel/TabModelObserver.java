// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * An interface to be notified about changes to a TabModel.
 */
public interface TabModelObserver {
    /**
     * Called when a tab is selected.
     *
     * @param tab The newly selected tab.
     * @param type The type of selection.
     * @param lastId The ID of the last selected tab, or {@link Tab#INVALID_TAB_ID} if no tab was
     *               selected.
     */
    void didSelectTab(Tab tab, @TabSelectionType int type, int lastId);

    /**
     * Called when a tab starts closing.
     *
     * @param tab The tab to close.
     * @param animate Whether or not to animate the closing.
     */
    void willCloseTab(Tab tab, boolean animate);

    /**
     * Called right after {@code tab} has been destroyed.
     *
     * @param tabId The ID of the tab that was destroyed.
     * @param incognito True if the closed tab was incognito.
     */
    void didCloseTab(int tabId, boolean incognito);

    /**
     * Called before a tab will be added to the {@link TabModel}.
     *
     * @param tab The tab about to be added.
     * @param type The type of tab launch.
     */
    void willAddTab(Tab tab, @TabLaunchType int type);

    /**
     * Called after a tab has been added to the {@link TabModel}.
     *
     * @param tab The newly added tab.
     * @param type The type of tab launch.
     */
    void didAddTab(Tab tab, @TabLaunchType int type);

    /**
     * Called after a tab has been moved from one position in the {@link TabModel} to another.
     *
     * @param tab The tab which has been moved.
     * @param newIndex The new index of the tab in the model.
     * @param curIndex The old index of the tab in the model.
     */
    void didMoveTab(Tab tab, int newIndex, int curIndex);

    /**
     * Called when a tab is pending closure, i.e. the user has just closed it, but it can still be
     * undone.  At this point, the Tab has been removed from the TabModel and can only be accessed
     * via {@link TabModel#getComprehensiveModel()}.
     *
     * @param tab The tab that is pending closure.
     */
    void tabPendingClosure(Tab tab);

    /**
     * Called when a tab closure is undone.
     *
     * @param tab The tab that has been reopened.
     */
    void tabClosureUndone(Tab tab);

    /**
     * Called when a tab closure is committed and can't be undone anymore.
     *
     * @param tab The tab that has been closed.
     */
    void tabClosureCommitted(Tab tab);

    /**
     * Called when multiple tabs are pending closure.
     *
     * @param tabs The tabs that are pending closure.
     * @param isAllTabs Whether |tabs| are all the tabs.
     */
    void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs);

    /**
     * Called when an "all tabs" closure has been committed and can't be undone anymore.
     */
    void allTabsClosureCommitted();

    /**
     * Called after a tab has been removed. At this point, the tab is no longer in the tab model.
     *
     * @param tab The tab that has been removed.
     */
    void tabRemoved(Tab tab);

    /**
     * Called after all {@link org.chromium.chrome.browser.tab.TabState}s within {@link TabModel}
     * are loaded from storage.
     */
    void restoreCompleted();
}
