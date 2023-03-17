// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 *  A controller responsible for setting up quick delete.
 */
public class QuickDeleteController {
    private static final MutableFlagWithSafeDefault sQuickDeleteForAndroidFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.QUICK_DELETE_FOR_ANDROID, false);

    private final @NonNull QuickDeleteDialogDelegate mQuickDeleteDialogDelegate;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull Context mContext;

    /**
     * Constructor for the QuickDeleteController with a dialog and confirmation snackbar.
     *
     * @param context The associated {@link Context}.
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete snackbar.
     */
    public QuickDeleteController(@NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager) {
        mContext = context;
        mSnackbarManager = snackbarManager;
        mQuickDeleteDialogDelegate =
                new QuickDeleteDialogDelegate(context, modalDialogManager, this::onDialogDismissed);
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

    /**
     * A method called when the user confirms or cancels the dialog.
     *
     * TODO(crbug.com/1412087): Add implementation logic for the deletion.
     */
    private void onDialogDismissed(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                showSnackbar();
                break;
            default:
                break;
        }
    }

    /**
     * A method to show the quick delete snack-bar.
     */
    private void showSnackbar() {
        Snackbar snackbar = Snackbar.make(
                mContext.getString(R.string.quick_delete_snackbar_message),
                /*controller= */ null, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_QUICK_DELETE);

        mSnackbarManager.showSnackbar(snackbar);
    }
}
