// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 *  A controller responsible for setting up quick delete.
 */
public class QuickDeleteController implements SnackbarManager.SnackbarController {
    private static final MutableFlagWithSafeDefault sQuickDeleteForAndroidFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID, false);

    private final boolean mShouldShowDialog;

    /** This is non null when quick delete dialog needs to be shown. */
    private final @Nullable QuickDeleteDialogDelegate mQuickDeleteDialogDelegate;
    private final @NonNull QuickDeleteSnackbarDelegate mQuickDeleteSnackbarDelegate;

    /**
     * Constructor to be called when both quick delete dialog and the snack bar needs to be shown.
     *
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete "Cancel" snack-bar.
     */
    private QuickDeleteController(@NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager) {
        mQuickDeleteSnackbarDelegate = new QuickDeleteSnackbarDelegate(snackbarManager, this);
        mQuickDeleteDialogDelegate =
                new QuickDeleteDialogDelegate(modalDialogManager, mQuickDeleteSnackbarDelegate);
        mShouldShowDialog = true;
    }

    /**
     * Constructor to be called when only the snack bar needs to be shown.
     *
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete "Cancel" snack-bar.
     */
    private QuickDeleteController(@NonNull SnackbarManager snackbarManager) {
        mQuickDeleteSnackbarDelegate = new QuickDeleteSnackbarDelegate(snackbarManager, this);
        mQuickDeleteDialogDelegate = null;
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
     * @return True, if quick delete feature flag is enabled, false otherwise.
     */
    public static boolean isQuickDeleteEnabled() {
        return sQuickDeleteForAndroidFlag.isEnabled();
    }

    /**
     * A method responsible for triggering the quick delete flow.
     */
    public void triggerQuickDeleteFlow() {
        if (mShouldShowDialog) {
            // Show quick delete dialog.
            mQuickDeleteDialogDelegate.showDialog();
        } else {
            // Show the quick delete snack-bar.
            mQuickDeleteSnackbarDelegate.showSnackbar();
        }
    }

    /**
     * Override from {@link SnackbarManager.SnackbarController}.
     *
     * TODO(crbug.com/1412087): Add integration logic with "Cancel / Undo" button to cancel the
     * quick delete operation.
     */
    @Override
    public void onAction(Object actionData) {}

    /**
     * Override from {@link SnackbarManager.SnackbarController}
     *
     * TODO(crbug.com/1412087): Add integration logic with Clear browsing data here.
     */
    @Override
    public void onDismissNoAction(Object actionData) {}
}
