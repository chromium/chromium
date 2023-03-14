// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 *  A controller responsible for setting up quick delete.
 */
public class QuickDeleteController {
    private static final MutableFlagWithSafeDefault sQuickDeleteForAndroidFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID, false);

    private final @NonNull QuickDeleteDialogDelegate mQuickDeleteDialogDelegate;
    private final @NonNull QuickDeleteSnackbarDelegate mQuickDeleteSnackbarDelegate;

    /**
     * Constructor for the QuickDeleteController with a dialog and confirmation snackbar.
     *
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete snackbar.
     */
    private QuickDeleteController(@NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager) {
        mQuickDeleteSnackbarDelegate = new QuickDeleteSnackbarDelegate(snackbarManager);
        mQuickDeleteDialogDelegate =
                new QuickDeleteDialogDelegate(modalDialogManager, mQuickDeleteSnackbarDelegate);
    }

    /**
     * A method to create the {@link QuickDeleteController} based on whether to show the dialog or
     * not.
     *
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete snackbar.
     *
     * @return {@link QuickDeleteController} The quick delete controller responsible for the
     *         triggering the quick delete flow.
     */
    public static @NonNull QuickDeleteController create(
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager) {
        return new QuickDeleteController(modalDialogManager, snackbarManager);
    }

    /**
     * @return True, if quick delete feature flag is enabled, false otherwise.
     */
    public static boolean isQuickDeleteEnabled() {
        return sQuickDeleteForAndroidFlag.isEnabled();
    }

    /**
     * A method responsible for triggering the quick delete flow.
     */
    public void triggerQuickDeleteFlow() {
        mQuickDeleteDialogDelegate.showDialog();
    }
}
