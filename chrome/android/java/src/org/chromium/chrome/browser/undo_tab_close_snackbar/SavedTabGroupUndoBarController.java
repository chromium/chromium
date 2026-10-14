// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A controller that receives direct events to fire snackbars representsing cancelable tab and
 * {@link SavedTabGroup} closures.
 *
 * <p>This controller mixes both tabs and SavedTabGroups for undo operations, but requires an
 * explicit event to queue a snackbar rather than listen for tab changes to the model.
 */
@NullMarked
public class SavedTabGroupUndoBarController extends UndoBarController
        implements UndoBarExplicitTrigger {
    private final ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final List<Destroyable> mOrchestratorLeases = new ArrayList<>();

    /**
     * Creates an instance of a {@link SavedTabGroupUndoBarController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param archivedTabModelOrchestrator The {@link ArchivedTabModelOrchestrator} that will be
     *     used to commit and undo tab closures and hold a lease while undo is pending.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     * @param tabGroupSyncService The {@link TabGroupSyncService} to handle synced tab group
     *     actions.
     */
    public SavedTabGroupUndoBarController(
            Context context,
            ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            SnackbarManageable snackbarManageable,
            @Nullable TabGroupSyncService tabGroupSyncService) {
        super(
                context,
                assumeNonNull(archivedTabModelOrchestrator.getTabModelSelector()),
                snackbarManageable);
        mArchivedTabModelOrchestrator = archivedTabModelOrchestrator;
        mTabGroupSyncService = tabGroupSyncService;
    }

    /**
     * Explicitly queues an undo snackbar to show based on a triggered action.
     *
     * @param tabs The list of closed tabs to be shown by the snackbar.
     * @param savedTabGroupSyncIds The list of closed {@link SavedTabGroup} syncIds shown by the
     *     snackbar.
     */
    public void queueUndoBar(List<Tab> tabs, List<String> savedTabGroupSyncIds) {
        if (tabs.isEmpty() && savedTabGroupSyncIds.isEmpty()) return;
        if (!mSnackbarManageable.getSnackbarManager().canShowSnackbar()) {
            for (Tab tab : tabs) {
                TabModel model = mTabModelSelector.getModelForTabId(tab.getId());
                if (model != null) model.commitTabClosure(tab.getId());
            }
            return;
        }

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureSavedGroups(
                        tabs, savedTabGroupSyncIds, mTabGroupSyncService);

        mOrchestratorLeases.add(
                mArchivedTabModelOrchestrator.acquireLease(LeaseReason.ARCHIVED_TABS_DIALOG));
        queueUndoBar(snapshot);
    }

    /** Releases any remaining leases held for pending undo snackbars. */
    public void destroy() {
        for (Destroyable lease : mOrchestratorLeases) {
            lease.destroy();
        }
        mOrchestratorLeases.clear();
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
        if (!mOrchestratorLeases.isEmpty()) {
            mOrchestratorLeases.remove(0).destroy();
        }
    }

    private void cancelSavedTabGroupClosure(String syncId) {
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.updateArchivalStatus(syncId, true);
        }
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
