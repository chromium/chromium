// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import android.content.Context;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Shared helper that executes tab-closure undo against a {@link TabModel}. */
@NullMarked
public final class UndoClosureRestorer {
    private UndoClosureRestorer() {}

    /**
     * Restores the closed tabs captured in {@code snapshot} via {@code tabModel}, recording user
     * actions and running any post-restore callback on success, or showing a failure notification
     * snackbar when no tabs could be restored.
     *
     * @param tabModel The {@link TabModel} to restore closed tabs into.
     * @param snapshot The closure snapshot captured when the tabs were closed.
     * @param reselect Whether to reselect a tab that was selected prior to closure.
     * @param context The {@link Context} used to resolve snackbar strings.
     * @param snackbarManager Optional {@link SnackbarManager} used to show a failure notification.
     * @return {@code true} if at least one tab was restored, {@code false} otherwise.
     */
    public static boolean restoreClosure(
            TabModel tabModel,
            UndoClosureSnapshot snapshot,
            boolean reselect,
            Context context,
            @Nullable SnackbarManager snackbarManager) {
        int[] restoredTabIds = tabModel.restoreClosedTabsForUndo(snapshot, reselect);
        boolean anyRestored = false;
        for (int tabId : restoredTabIds) {
            if (tabId != Tab.INVALID_TAB_ID) {
                anyRestored = true;
                break;
            }
        }

        if (anyRestored) {
            if (snapshot.closingSource == TabClosingSource.TABLET_TAB_STRIP) {
                RecordUserAction.record("TabletTabStrip.UndoCloseTab");
            }
            if (snapshot.undoRunnable != null) {
                snapshot.undoRunnable.run();
            }
            return true;
        }

        if (snackbarManager != null) {
            snackbarManager.showSnackbar(
                    Snackbar.make(
                            context.getString(R.string.undo_tab_restore_failed_message),
                            /* controller= */ null,
                            Snackbar.TYPE_NOTIFICATION,
                            Snackbar.UMA_TAB_CLOSE_UNDO));
        }
        return false;
    }
}
