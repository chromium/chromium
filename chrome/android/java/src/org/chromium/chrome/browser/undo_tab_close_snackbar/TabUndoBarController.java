// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import android.content.Context;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/**
 * A controller that listens to and visually represents cancelable tab closures.
 *
 * <p>Each time a tab is undoably closed via {@link TabModelObserver#onTabClosurePending()},' this
 * controller saves the tab ids and title to the stack of SnackbarManager. It will then let
 * SnackbarManager to show a snackbar representing the top entry in of stack. Each added entry
 * resets the timeout that tracks when to commit the undoable actions.
 *
 * <p>When the undo button is clicked, it will cancel the tab closure if any. All pending closing
 * will be committed.
 *
 * <p>This class also responds to external changes to the undo state by monitoring {@link
 * TabModelObserver#tabClosureUndone(Tab)} and {@link TabModelObserver#tabClosureCommitted(Tab)} to
 * properly keep it's internal state in sync with the model.
 */
@NullMarked
public class TabUndoBarController extends UndoBarController {
    private final TabModelObserver mTabModelObserver;

    /**
     * Creates an instance of a {@link TabUndoBarController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param selector The {@link TabModelSelector} that will be used to commit and undo tab
     *     closures.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     * @param dialogVisibilitySupplier The {@link Supplier} to get the visibility of TabGridDialog.
     */
    public TabUndoBarController(
            Context context,
            TabModelSelector selector,
            SnackbarManageable snackbarManageable,
            @Nullable Supplier<Boolean> dialogVisibilitySupplier) {
        super(context, selector, snackbarManageable);
        mTabModelObserver =
                new TabModelObserver() {
                    /**
                     * Decides whether we should disable an attempt to show/hide the undo bar.
                     *
                     * @param showingUndoBar indicates whether the expected behavior of the caller
                     *     is to show or dismiss the undo bar.
                     */
                    private boolean disableUndo(boolean showingUndoBar) {
                        // When closure(s) happen and we are trying to show the undo bar, check
                        // whether the TabGridDialog is showing. If so, don't show the undo bar
                        // because TabGridDialog has its own undo bar. See crbug.com/40714421. Note
                        // that we don't disable attempts to dismiss snack bar to make sure that
                        // snack bar state is in sync with tab model.
                        return showingUndoBar
                                && dialogVisibilitySupplier != null
                                && dialogVisibilitySupplier.get();
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        if (disableUndo(false)) return;
                        onTabsResolvedExternally(List.of(tab.getId()), /* undone= */ true);
                        Token groupId = tab.getTabGroupId();
                        if (groupId != null) {
                            onTabGroupRestored(groupId);
                        }
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        if (disableUndo(false)) return;
                        onTabsResolvedExternally(List.of(tab.getId()), /* undone= */ false);
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        if (disableUndo(false)) return;
                        // A set keeps the per-snapshot membership checks O(1) for bulk closures.
                        Set<@TabId Integer> tabIds = new HashSet<>(TabModelUtils.getTabIds(tabs));
                        onTabsResolvedExternally(tabIds, /* undone= */ false);
                    }

                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs,
                            boolean isAllTabs,
                            @TabClosingSource int closingSource) {
                        if (disableUndo(true)) return;

                        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);
                        Profile profile = tabModel.getProfile();
                        boolean tabGroupSyncEnabled =
                                profile != null
                                        && profile.isNativeInitialized()
                                        && TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);

                        queueUndoBar(
                                UndoClosureSnapshotFactory.captureFromTabModel(
                                        tabModel, tabs, isAllTabs, tabGroupSyncEnabled));
                    }

                    @Override
                    public void allTabsClosureCommitted(boolean isIncognito) {
                        if (disableUndo(false)) return;
                        clearEventQueue();
                        mSnackbarManageable
                                .getSnackbarManager()
                                .dismissSnackbars(TabUndoBarController.this);
                    }
                };
    }

    /**
     * Carry out native library dependent operations like registering observers and notifications.
     */
    public void initialize() {
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
    }

    /**
     * Cleans up this class, unregistering for application notifications from the {@link
     * TabModelSelector}.
     */
    public void destroy() {
        TabModel model = mTabModelSelector.getModel(false);
        if (model != null) model.removeObserver(mTabModelObserver);
    }
}
