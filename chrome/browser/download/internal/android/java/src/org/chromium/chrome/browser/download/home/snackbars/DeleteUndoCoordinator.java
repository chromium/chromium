// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.snackbars;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/**
 * A coordinator that is responsible for processing {@link OfflineItem} deletion requests and -
 * based on a user decision or inaction - approve or reject the request.
 */
public class DeleteUndoCoordinator {
    private final SnackbarManager mView;
    private final SnackbarController mController;

    /** Creates a {@link DeleteUndoCoordinator} instance. */
    public DeleteUndoCoordinator(SnackbarManager snackbarManager) {
        mView = snackbarManager;
        mController = new SnackbarControllerImpl();
    }

    /** Destroys this coordinator.  This will dismiss all outstanding snackbars with no action. */
    public void destroy() {
        mView.dismissSnackbars(mController);
    }

    /**
     * Shows a snackbar for an undoable delete action on {@link OfflineItem}s.
     *
     * @param itemsSelected The {@link OfflineItem}s the user explicitly selected to delete.
     * @param callback      The {@link Callback} to notify when the snackbar is finished showing.
     */
    public void showSnackbar(Collection<OfflineItem> itemsSelected, Callback<Boolean> callback) {
        assert !itemsSelected.isEmpty();

        Snackbar snackbar =
                Snackbar.make(
                        UndoUiUtils.getTitleFor(itemsSelected),
                        mController,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_DOWNLOAD_DELETE_UNDO);
        snackbar.setAction(ContextUtils.getApplicationContext().getString(R.string.undo), callback);
        snackbar.setTemplateText(UndoUiUtils.getTemplateTextFor(itemsSelected));
        snackbar.setActionAccessibilityAnnouncement(
                UndoUiUtils.getAccessibilityActionAnnouncementTextFor(itemsSelected));
        mView.showSnackbar(snackbar);
    }

    private static class SnackbarControllerImpl implements SnackbarController {
        // SnackbarController implementation.
        @Override
        public void onAction(Object actionData) {
            notifyCallback(actionData, false);
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            notifyCallback(actionData, true);
        }

        @SuppressWarnings("unchecked")
        private static void notifyCallback(Object actionData, boolean delete) {
            ((Callback<Boolean>) actionData).onResult(delete);
        }
    }
}
