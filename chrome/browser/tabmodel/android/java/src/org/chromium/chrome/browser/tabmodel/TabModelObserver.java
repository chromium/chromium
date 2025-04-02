// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.List;

/**
 * An interface to be notified about changes to a TabModel.
 *
 * <p>NOTE: Any changes to this interface including the addition of new methods should be applied to
 * {@link TabGroupModelFilter} and {@link TabModelObserverJniBridge}.
 */
@NullMarked
public interface TabModelObserver {
    /**
     * Called when a tab is selected. This may not be called in some cases if this model is not the
     * active model. If observing the current tab in this model is desired consider using {@link
     * TabModel#getCurrentTabSupplier()} and observing that instead.
     *
     * @param tab The newly selected tab.
     * @param type The type of selection.
     * @param lastId The ID of the last selected tab, or {@link Tab#INVALID_TAB_ID} if no tab was
     *     selected.
     */
    default void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {}

    /**
     * Called when a tab starts closing.
     *
     * @param tab The tab to close.
     * @param didCloseAlone indicates whether tab will close by itself VS as part of multiple/all
     *     tab closures.
     */
    default void willCloseTab(Tab tab, boolean didCloseAlone) {}

    /**
     * Called right before {@code tab} will be destroyed. Called for each tab.
     *
     * @param tab The {@link Tab} that was closed.
     */
    default void onFinishingTabClosure(Tab tab) {}

    /**
     * Called right before each of {@code tabs} will be destroyed. Called as each closure event is
     * committed. Will be called per closure event i.e. {@link TabModel#closeTab()}, {@link
     * TabModel#closeAllTabs()}, and {@link TabModel#closeMultipleTabs()} will all trigger one event
     * when the tabs associated with a particular closure commit to closing.
     *
     * @param tabs The list of {@link Tab} that were closed.
     * @param canRestore Whether the closed tabs can be restored to the TabRestoreService.
     */
    default void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {}

    /**
     * Called before a tab will be added to the {@link TabModel}.
     *
     * @param tab The tab about to be added.
     * @param type The type of tab launch.
     */
    default void willAddTab(Tab tab, @TabLaunchType int type) {}

    /**
     * Called after a tab has been added to the {@link TabModel}.
     *
     * @param tab The newly added tab.
     * @param type The type of tab launch.
     * @param creationState How the tab was created.
     * @param markedForSelection Indicates whether the added tab will be selected.
     */
    default void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {}

    /**
     * Called after a tab has been moved from one position in the {@link TabModel} to another.
     *
     * @param tab The tab which has been moved.
     * @param newIndex The new index of the tab in the model.
     * @param curIndex The old index of the tab in the model.
     */
    default void didMoveTab(Tab tab, int newIndex, int curIndex) {}

    /**
     * Called when a tab is pending closure, i.e. the user has just closed it, but it can still be
     * undone. At this point, the Tab has been removed from the TabModel and can only be accessed
     * via {@link TabModel#getComprehensiveModel()}.
     *
     * @param tab The tab that is pending closure.
     * @param pendingToken The token that can be used to commit or undo the tab closure.
     */
    default void tabPendingClosure(Tab tab) {}

    /**
     * Called when multiple tabs are pending closure.
     *
     * @param tabs The tabs that are pending closure.
     * @param isAllTabs Whether |tabs| are all the tabs.
     */
    default void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {}

    /**
     * Called when a tab closure is undone.
     *
     * @param tab The tab that has been reopened.
     */
    default void tabClosureUndone(Tab tab) {}

    /**
     * Called when a tab closure is committed and can't be undone anymore.
     *
     * @param tab The tab that has been closed.
     */
    default void tabClosureCommitted(Tab tab) {}

    /**
     * Called when an "all tabs" closure will happen. If multiple tabs are closed, @{@link
     * TabModelObserver#willCloseMultipleTabs(boolean, List)} is invoked
     */
    default void willCloseAllTabs(boolean incognito) {}

    /**
     * Called when multiple tabs closure will happen. If "all tabs" are closed at once, @{@link
     * TabModelObserver#willCloseAllTabs(boolean)} is invoked.
     *
     * @param allowUndo If undo is allowed on the tab closure.
     * @param tabs being closed.
     */
    default void willCloseMultipleTabs(boolean allowUndo, List<Tab> tabs) {}

    /** Called when an "all tabs" closure has been committed and can't be undone anymore. */
    default void allTabsClosureCommitted(boolean isIncognito) {}

    /**
     * Called after a tab has been removed. At this point, the tab is no longer in the tab model.
     *
     * @param tab The tab that has been removed.
     */
    default void tabRemoved(Tab tab) {}

    /**
     * Called after all {@link org.chromium.chrome.browser.tab.TabState}s within {@link TabModel}
     * are loaded from storage.
     */
    default void restoreCompleted() {}

    //  TODO(crbug.com/381471263): The following methods are still in development and will
    //  replace the existing tab closure events in the near future. Methods being replaced are
    //  tabPendingClosure, multipleTabsPendingClosure, tabClosureUndone,
    //  allTabsClosureUndone, tabClosureCommitted, willCloseAllTabs,
    //  willCloseMultipleTabs and allTabsClosureCommitted.
    /**
     * Called right before {@code tabs} will be destroyed.
     *
     * @param tabs The list of {@link Tab}s that will be closed.
     * @param isAllTabs Whether tabs are all the tabs.
     */
    default void onTabCloseImmediate(List<Tab> tabs, boolean isAllTabs) {}

    /**
     * Called right before when tabs are pending closure, i.e. the user has just closed them, but it
     * can still be undone.
     *
     * @param tabs The list of {@link Tab}s that are pending closure.
     * @param isAllTabs Whether tabs are all the tabs.
     */
    default void onTabClosePending(List<Tab> tabs, boolean isAllTabs) {}

    /**
     * Called right before {@code tabs} closure is committed permanently and cannot be undone.
     *
     * @param tabs The list of {@link Tab}s that are closed.
     * @param isAllTabs Whether tabs are all the tabs.
     */
    default void onTabCloseCommitted(List<Tab> tabs, boolean isAllTabs) {}

    /**
     * Called just before {@code tabs} closed have been successfully restored by an undo action.
     *
     * @param tabs The list of {@link Tab}s that has been reopened.
     * @param isAllTabs Whether tabs are all the tabs.
     */
    default void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {}
}
