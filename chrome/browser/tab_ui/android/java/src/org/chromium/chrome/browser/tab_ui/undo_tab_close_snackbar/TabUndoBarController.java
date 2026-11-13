// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui.undo_tab_close_snackbar;

import android.content.Context;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshotFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;

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
    private final @Nullable Supplier<Boolean> mDialogVisibilitySupplier;
    private final Supplier<@Nullable TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private @Nullable UndoClosureSnapshot mReselectableSnapshot;

    /**
     * Creates an instance of a {@link TabUndoBarController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param selector The {@link TabModelSelector} that will be used to commit and undo tab
     *     closures.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     * @param dialogVisibilitySupplier The {@link Supplier} to get the visibility of TabGridDialog.
     * @param tabGroupUiActionHandlerSupplier The {@link Supplier} of the {@link
     *     TabGroupUiActionHandler} used to reopen hidden synced tab groups during undo.
     */
    public TabUndoBarController(
            Context context,
            TabModelSelector selector,
            SnackbarManageable snackbarManageable,
            @Nullable Supplier<Boolean> dialogVisibilitySupplier,
            Supplier<@Nullable TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier) {
        super(context, selector, snackbarManageable);
        mDialogVisibilitySupplier = dialogVisibilitySupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        switch (type) {
                            case TabSelectionType.FROM_USER,
                                    TabSelectionType.FROM_OMNIBOX,
                                    TabSelectionType.FROM_NEW,
                                    TabSelectionType.FROM_UNDO ->
                                    mReselectableSnapshot = null;
                        }
                    }

                    @Override
                    public void onDidActiveStateChange(TabModel tabModel, boolean active) {
                        if (!active) {
                            mReselectableSnapshot = null;
                        }
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        if (ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) return;
                        if (disableUndo(false)) return;
                        onTabsResolvedExternally(List.of(tab.getId()), /* undone= */ true);
                        Token groupId = tab.getTabGroupId();
                        if (groupId != null) {
                            onTabGroupRestored(groupId);
                        }
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        if (ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) return;
                        if (disableUndo(false)) return;
                        onTabsResolvedExternally(List.of(tab.getId()), /* undone= */ false);
                    }

                    @Override
                    public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                        if (ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) return;
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
                        if (ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) return;
                        mReselectableSnapshot = null;
                        if (disableUndo(true)) return;

                        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);
                        Profile profile = tabModel.getProfile();
                        boolean tabGroupSyncEnabled =
                                profile != null
                                        && profile.isNativeInitialized()
                                        && TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);

                        UndoClosureSnapshot snapshot =
                                UndoClosureSnapshotFactory.captureFromTabModel(
                                        tabModel,
                                        tabs,
                                        isAllTabs,
                                        tabGroupSyncEnabled,
                                        /* tabGroupSyncService= */ null,
                                        closingSource,
                                        /* entryId= */ TabRestoreEntryId
                                                .INVALID_TAB_RESTORE_ENTRY_ID,
                                        /* undoRunnable= */ null);
                        mReselectableSnapshot =
                                mTabModelSelector.isIncognitoSelected() ? null : snapshot;
                        queueUndoBar(snapshot);
                    }

                    @Override
                    public void allTabsClosureCommitted(boolean isIncognito) {
                        if (disableUndo(false)) return;
                        mReselectableSnapshot = null;
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

    /**
     * Handles an undoable tab closure snapshot captured when saving closed tabs.
     *
     * @param snapshot The pre-captured {@link UndoClosureSnapshot} for the closure.
     */
    public void onUndoableClosure(UndoClosureSnapshot snapshot) {
        if (!ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) return;
        mReselectableSnapshot = null;
        // For finalized closures, onFinishingMultipleTabClosure runs before didRemoveTabForClosure
        // where TabGridDialogMediator hides the dialog when its tab group becomes empty. If the
        // closure closes an entire tab group while TabGridDialog is open, still queue the undo bar
        // here; TabGridDialogCoordinator throttles showing it until the dialog's hide animation
        // finishes.
        if (disableUndo(/* showingUndoBar= */ true) && !isClosingEntireTabGroup(snapshot)) return;
        mReselectableSnapshot =
                (!mTabModelSelector.isIncognitoSelected() && hasSelectedTab(snapshot))
                        ? snapshot
                        : null;
        queueUndoBar(snapshot);
    }

    /**
     * Restores the closed tabs via {@link UndoClosureRestorer} when the rework flag is enabled, or
     * falls back to cancelling pending closures.
     */
    @Override
    protected void handleUndoAction(
            UndoClosureSnapshot snapshot, Set<@TabId Integer> remainingTabIds) {
        if (!ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) {
            super.handleUndoAction(snapshot, remainingTabIds);
            return;
        }
        // Reference equality is intended: only the exact snapshot instance most recently tracked
        // for reselection is eligible.
        boolean reselect =
                snapshot == mReselectableSnapshot && !mTabModelSelector.isIncognitoSelected();
        mReselectableSnapshot = null;
        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);
        UndoClosureRestorer.restoreClosure(
                tabModel,
                snapshot,
                reselect,
                mContext,
                mSnackbarManageable.getSnackbarManager(),
                mTabGroupUiActionHandlerSupplier.get());
    }

    /**
     * Leaves the already-finalized closure untouched when the rework flag is enabled, or falls back
     * to committing pending closures.
     */
    @Override
    protected void handleDismissNoAction(
            UndoClosureSnapshot snapshot, Set<@TabId Integer> remainingTabIds) {
        if (!ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) {
            super.handleDismissNoAction(snapshot, remainingTabIds);
            return;
        }
        if (snapshot == mReselectableSnapshot) {
            mReselectableSnapshot = null;
        }
    }

    /**
     * Decides whether we should disable an attempt to show/hide the undo bar.
     *
     * @param showingUndoBar indicates whether the expected behavior of the caller is to show or
     *     dismiss the undo bar.
     */
    private boolean disableUndo(boolean showingUndoBar) {
        // When closure(s) happen and we are trying to show the undo bar, check whether the
        // TabGridDialog is showing. If so, don't show the undo bar because TabGridDialog has its
        // own undo bar. If the TabGridDialog is closing because its tab group is now empty, we
        // still queue this undo bar and TabGridDialogCoordinator throttles showing it until the
        // hide animation is dismissed. See crbug.com/40714421. Note that we don't disable attempts
        // to dismiss snack bar to make sure that snack bar state is in sync with tab model.
        return showingUndoBar
                && mDialogVisibilitySupplier != null
                && mDialogVisibilitySupplier.get();
    }

    private static boolean isClosingEntireTabGroup(UndoClosureSnapshot snapshot) {
        if (snapshot.groups.isEmpty()) return false;
        for (GroupData groupData : snapshot.groups.values()) {
            if (groupData.survivesInModel) return false;
        }
        return true;
    }

    private static boolean hasSelectedTab(UndoClosureSnapshot snapshot) {
        for (ClosedTab tab : snapshot.tabs) {
            if (tab.wasSelected) return true;
        }
        return false;
    }
}
