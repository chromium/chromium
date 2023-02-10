// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 *  A controller responsible for setting up quick delete.
 *
 *  TODO(crbug.com/1412087): Follow up on the implementation.
 */
public class QuickDeleteController {
    private static final MutableFlagWithSafeDefault sQuickDeleteForAndroidFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID, false);

    private final boolean mShouldShowDialog;

    /**
     * Constructor to be called when both quick delete dialog and the snack bar needs to be shown.
     *
     * TODO(crbug.com/1412087): Add logic to create quick delete dialog and snack-bar.
     *
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete "Cancel" snack-bar.
     */
    private QuickDeleteController(@NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager) {
        mShouldShowDialog = true;
    }

    /**
     * Constructor to be called when only the snack bar needs to be shown.
     *
     * TODO(crbug.com/1412087): Add logic to create quick delete snack-bar.
     *
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete "Cancel" snack-bar.
     */
    private QuickDeleteController(@NonNull SnackbarManager snackbarManager) {
        mShouldShowDialog = false;
    }

    /**
     * A method to create the {@link QuickDeleteController} based on whether to show the dialog or
     * not.
     *
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete "Cancel" snack-bar.
     * @param prefService A {@link PrefService} to query whether the dialog needs to be suppressed.
     *
     * @return {@link QuickDeleteController} The quick delete controller responsible for the
     *         triggering the quick delete flow.
     */
    public static @NonNull QuickDeleteController create(
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager, @NonNull PrefService prefService) {
        if (prefService.getBoolean(Pref.QUICK_DELETE_DIALOG_SUPPRESSED)) {
            return new QuickDeleteController(snackbarManager);
        } else {
            return new QuickDeleteController(modalDialogManager, snackbarManager);
        }
    }

    /**
     * A method responsible for triggering the quick delete flow.
     *
     * TODO(crbug.com/1227656): Add the triggering logic here.
     */
    public void triggerQuickDeleteFlow() {
        if (mShouldShowDialog) {
            // Show quick delete dialog.
        } else {
            // Show the quick delete snack-bar.
        }
    }

    public static boolean isQuickDeleteEnabled() {
        return sQuickDeleteForAndroidFlag.isEnabled();
    }
}
