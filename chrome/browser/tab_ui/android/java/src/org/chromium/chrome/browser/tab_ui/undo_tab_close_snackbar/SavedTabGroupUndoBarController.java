// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui.undo_tab_close_snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.ArraySet;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshotFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/**
 * A controller that receives direct events to fire snackbars representsing cancelable tab and
 * {@link SavedTabGroup} closures.
 *
 * <p>This controller mixes both tabs and SavedTabGroups for undo operations, but requires an
 * explicit event to queue a snackbar rather than listen for tab changes to the model.
 */
@NullMarked
public class SavedTabGroupUndoBarController extends UndoBarController
        implements UndoBarExplicitTrigger, Destroyable {
    private final Supplier<Destroyable> mLeaseSupplier;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final List<Destroyable> mLeases = new ArrayList<>();
    private @Nullable UndoClosureSnapshot mPendingSnapshot;

    /**
     * Creates an instance of a {@link SavedTabGroupUndoBarController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param tabModelSelector The {@link TabModelSelector} that will be used to commit and undo tab
     *     closures.
     * @param leaseSupplier Supplies a {@link Destroyable} lease to hold while undo is pending.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     * @param tabGroupSyncService The {@link TabGroupSyncService} to handle synced tab group
     *     actions.
     */
    public SavedTabGroupUndoBarController(
            Context context,
            TabModelSelector tabModelSelector,
            Supplier<Destroyable> leaseSupplier,
            SnackbarManageable snackbarManageable,
            @Nullable TabGroupSyncService tabGroupSyncService) {
        super(context, tabModelSelector, snackbarManageable);
        mLeaseSupplier = leaseSupplier;
        mTabGroupSyncService = tabGroupSyncService;
    }

    /**
     * Records the pre-close {@link UndoClosureSnapshot} captured when archived tabs are finalized
     * and saved to {@code TabRestoreService}.
     *
     * @param snapshot The pre-captured {@link UndoClosureSnapshot} for the archived tab closure.
     */
    public void onUndoableClosure(UndoClosureSnapshot snapshot) {
        if (!ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) return;
        mPendingSnapshot = snapshot;
    }

    /**
     * Explicitly queues an undo snackbar to show based on a triggered action.
     *
     * @param tabs The list of closed tabs to be shown by the snackbar.
     * @param savedTabGroupSyncIds The list of closed {@link SavedTabGroup} syncIds shown by the
     *     snackbar.
     */
    public void queueUndoBar(List<Tab> tabs, List<String> savedTabGroupSyncIds) {
        UndoClosureSnapshot pendingSnapshot = consumePendingSnapshot(tabs);
        if (tabs.isEmpty() && savedTabGroupSyncIds.isEmpty()) return;

        boolean reworkEnabled = ChromeFeatureList.sUndoableTabClosureRework.isEnabled();
        if (!mSnackbarManageable.getSnackbarManager().canShowSnackbar()) {
            if (!reworkEnabled) {
                for (Tab tab : tabs) {
                    TabModel model = mTabModelSelector.getModelForTabId(tab.getId());
                    if (model != null) model.commitTabClosure(tab.getId());
                }
            }
            return;
        }

        UndoClosureSnapshot snapshot;
        if (!reworkEnabled) {
            snapshot =
                    UndoClosureSnapshotFactory.captureArchived(
                            tabs, savedTabGroupSyncIds, mTabGroupSyncService);
        } else if (savedTabGroupSyncIds.isEmpty()) {
            snapshot = pendingSnapshot;
        } else {
            snapshot =
                    UndoClosureSnapshotFactory.captureArchivedSavedGroups(
                            pendingSnapshot, savedTabGroupSyncIds, mTabGroupSyncService);
        }
        if (snapshot == null) return;

        mLeases.add(mLeaseSupplier.get());
        super.queueUndoBar(snapshot);
    }

    /** Releases any remaining leases held for pending undo snackbars. */
    @Override
    public void destroy() {
        mPendingSnapshot = null;
        clearEventQueue();
        mSnackbarManageable.getSnackbarManager().dismissSnackbars(this);
        List<Destroyable> leases = new ArrayList<>(mLeases);
        mLeases.clear();
        for (Destroyable lease : leases) {
            lease.destroy();
        }
    }

    @Override
    protected void handleUndoAction(
            UndoClosureSnapshot snapshot, Set<@TabId Integer> remainingTabIds) {
        if (!ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) {
            super.handleUndoAction(snapshot, remainingTabIds);
            return;
        }
        if (!snapshot.tabs.isEmpty()
                && snapshot.entryId != TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID) {
            UndoClosureRestorer.restoreClosure(
                    mTabModelSelector.getModel(/* incognito= */ false),
                    snapshot,
                    /* reselect= */ false,
                    mContext,
                    mSnackbarManageable.getSnackbarManager(),
                    /* tabGroupUiActionHandler= */ null);
        }
    }

    @Override
    protected void handleDismissNoAction(
            UndoClosureSnapshot snapshot, Set<@TabId Integer> remainingTabIds) {
        if (!ChromeFeatureList.sUndoableTabClosureRework.isEnabled()) {
            super.handleDismissNoAction(snapshot, remainingTabIds);
        }
    }

    // SnackbarManager.SnackbarController implementation.

    @Override
    public void onAction(@Nullable Object actionData) {
        try {
            super.onAction(actionData);
            UndoBarContent barContent = assumeNonNull((UndoBarContent) actionData);
            List<String> closedSavedTabGroupSyncIds = barContent.snapshot.savedTabGroupSyncIds;
            if (!closedSavedTabGroupSyncIds.isEmpty()) {
                for (String syncId : closedSavedTabGroupSyncIds) {
                    cancelSavedTabGroupClosure(syncId);
                }
            }
        } finally {
            releaseNextLease();
        }
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        try {
            super.onDismissNoAction(actionData);
        } finally {
            releaseNextLease();
        }
    }

    private void releaseNextLease() {
        if (!mLeases.isEmpty()) {
            mLeases.remove(0).destroy();
        }
    }

    private void cancelSavedTabGroupClosure(String syncId) {
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.updateArchivalStatus(syncId, true);
        }
    }

    private @Nullable UndoClosureSnapshot consumePendingSnapshot(List<Tab> tabs) {
        UndoClosureSnapshot pending = mPendingSnapshot;
        mPendingSnapshot = null;
        if (!tabs.isEmpty()
                && pending != null
                && pending.entryId != TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID
                && matchesClosedTabs(pending, tabs)) {
            return pending;
        }
        return null;
    }

    private static boolean matchesClosedTabs(UndoClosureSnapshot snapshot, List<Tab> tabs) {
        if (snapshot.tabs.size() != tabs.size()) return false;
        if (tabs.size() == 1) {
            return snapshot.tabs.get(0).id == tabs.get(0).getId();
        }
        Set<@TabId Integer> snapshotTabIds = new ArraySet<>(snapshot.tabs.size());
        for (ClosedTab closedTab : snapshot.tabs) {
            snapshotTabIds.add(closedTab.id);
        }
        for (Tab tab : tabs) {
            if (!snapshotTabIds.remove(tab.getId())) return false;
        }
        return snapshotTabIds.isEmpty();
    }

    // UndoBarExplicitTrigger implementation.

    @Override
    public void triggerSnackbarForSavedTabGroup(String syncId) {
        queueUndoBar(Collections.emptyList(), List.of(syncId));
    }

    @Override
    public void triggerSnackbarForTab(Tab tab) {
        queueUndoBar(List.of(tab), Collections.emptyList());
    }
}
