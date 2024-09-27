// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.download.home.rename;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class to manage Rename Dialog and Rename Extension Dialog display sequence.
 * Decides the state transition of two dialog, and provide controller for action events.
 */
public class RenameDialogManager {
    /**
     * Helper interface for handling rename attempts by the UI, must be called when user click
     * submit and make the attempt to rename the download item, allows the UI to
     * response to result of a rename attempt from the backend.
     */
    @FunctionalInterface
    public interface RenameCallback {
        void attemptRename(String name, Callback</*@RenameResult*/ Integer> callback);
    }

    private final RenameDialogCoordinator mRenameDialogCoordinator;
    private final RenameExtensionDialogCoordinator mRenameExtensionDialogCoordinator;

    private String mOriginalName;
    private String mLastAttemptedName;
    private @RenameResult int mLastRenameAttemptResult;

    private RenameCallback mRenameCallback;
    private @RenameDialogState int mCurState;

    @IntDef({
        RenameDialogState.NO_DIALOG,
        RenameDialogState.RENAME_DIALOG_DEFAULT,
        RenameDialogState.RENAME_DIALOG_CANCEL,
        RenameDialogState.RENAME_DIALOG_COMMIT_ERROR,
        RenameDialogState.RENAME_EXTENSION_DIALOG_DEFAULT,
        RenameDialogState.RENAME_EXTENSION_DIALOG_CANCEL,
        RenameDialogState.RENAME_EXTENSION_DIALOG_COMMIT_ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface RenameDialogState {
        /** Initial State, should not show any dialog. */
        int NO_DIALOG = 0;

        /** Should show the rename dialog, asking user to input. */
        int RENAME_DIALOG_DEFAULT = 1;

        /** Rename dialog intent is aborted. */
        int RENAME_DIALOG_CANCEL = 2;

        /**
         * Get error message after rename attempt, should show the rename dialog with error
         * message.
         */
        int RENAME_DIALOG_COMMIT_ERROR = 3;

        /**
         * Should show the rename extension dialog, asking user to confirm the intent of changing
         * extension.
         */
        int RENAME_EXTENSION_DIALOG_DEFAULT = 4;

        /** Cancel the intent of changing the extension. */
        int RENAME_EXTENSION_DIALOG_CANCEL = 5;

        /**
         * Get error message after rename attempt after confirming the change of extension,
         * should show the rename dialog with error message.
         */
        int RENAME_EXTENSION_DIALOG_COMMIT_ERROR = 6;
    }

    public RenameDialogManager(Context context, ModalDialogManager modalDialogManager) {
        mRenameDialogCoordinator =
                new RenameDialogCoordinator(context, modalDialogManager, this::onRenameDialogClick);

        mRenameExtensionDialogCoordinator =
                new RenameExtensionDialogCoordinator(
                        context,
                        modalDialogManager,
                        this::onRenameExtensionDialogClick,
                        this::onRenameExtensionDialogDismiss);

        mLastRenameAttemptResult = RenameResult.FAILURE_UNKNOWN;
        mCurState = RenameDialogState.NO_DIALOG;
    }

    public void destroy() {
        mRenameDialogCoordinator.destroy();
        mRenameExtensionDialogCoordinator.destroy();
    }

    /**
     * Function that will be triggered by UI to show a rename dialog showing {@code originalName}.
     * @param originalName The original name for the download item.
     * @param callback  The callback that talks to the backend.
     */
    public void startRename(String originalName, RenameCallback callback) {
        mRenameCallback = callback;
        mOriginalName = originalName;
        mLastAttemptedName = originalName;
        mLastRenameAttemptResult = RenameResult.SUCCESS;
        mCurState = RenameDialogState.NO_DIALOG;
        processDialogState(
                RenameDialogState.RENAME_DIALOG_DEFAULT, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    /**
     * Decider to telling the right order to dialog coordinators depending on the state transition
     * update.
     */
    private void processDialogState(@RenameDialogState int nextState, int dismissalCause) {
        switch (nextState) {
            case RenameDialogState.NO_DIALOG:
                if (mCurState == RenameDialogState.RENAME_EXTENSION_DIALOG_DEFAULT
                        || mCurState == RenameDialogState.RENAME_EXTENSION_DIALOG_COMMIT_ERROR) {
                    mRenameExtensionDialogCoordinator.dismissDialog(dismissalCause);
                } else if (mCurState == RenameDialogState.RENAME_DIALOG_DEFAULT
                        || mCurState == RenameDialogState.RENAME_DIALOG_COMMIT_ERROR) {
                    mRenameDialogCoordinator.dismissDialog(dismissalCause);
                }
                break;
            case RenameDialogState.RENAME_DIALOG_DEFAULT:
                mRenameDialogCoordinator.showDialog(mOriginalName);
                break;
            case RenameDialogState.RENAME_DIALOG_COMMIT_ERROR:
                mRenameDialogCoordinator.showDialogWithErrorMessage(
                        mLastAttemptedName, mLastRenameAttemptResult);
                break;
            case RenameDialogState.RENAME_DIALOG_CANCEL:
                mRenameDialogCoordinator.dismissDialog(dismissalCause);
                break;
            case RenameDialogState.RENAME_EXTENSION_DIALOG_DEFAULT:
                mRenameExtensionDialogCoordinator.showDialog();
                mRenameDialogCoordinator.dismissDialog(dismissalCause);
                break;
            case RenameDialogState.RENAME_EXTENSION_DIALOG_CANCEL:
                mRenameExtensionDialogCoordinator.dismissDialog(dismissalCause);
                mRenameDialogCoordinator.showDialog(mLastAttemptedName);
                break;
            case RenameDialogState.RENAME_EXTENSION_DIALOG_COMMIT_ERROR:
                mRenameExtensionDialogCoordinator.dismissDialog(dismissalCause);
                mRenameDialogCoordinator.showDialogWithErrorMessage(
                        mLastAttemptedName, mLastRenameAttemptResult);
                break;
            default:
                break;
        }
        mCurState = nextState;
    }

    private void runRenameCallback() {
        mRenameCallback.attemptRename(
                mLastAttemptedName,
                result -> {
                    mLastRenameAttemptResult = result;
                    if (result == RenameResult.SUCCESS) {
                        processDialogState(
                                RenameDialogState.NO_DIALOG,
                                DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    } else {
                        processDialogState(
                                mCurState == RenameDialogState.RENAME_EXTENSION_DIALOG_DEFAULT
                                        ? RenameDialogState.RENAME_EXTENSION_DIALOG_COMMIT_ERROR
                                        : RenameDialogState.RENAME_DIALOG_COMMIT_ERROR,
                                DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    }
                });
    }

    private void onRenameDialogClick(boolean isPositiveButton) {
        if (isPositiveButton) {
            mLastAttemptedName = mRenameDialogCoordinator.getCurSuggestedName();

            if (TextUtils.equals(mLastAttemptedName, mOriginalName)) {
                processDialogState(
                        RenameDialogState.RENAME_DIALOG_CANCEL,
                        DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                return;
            }

            if (!RenameUtils.getFileExtension(mLastAttemptedName)
                    .equalsIgnoreCase(RenameUtils.getFileExtension(mOriginalName))) {
                processDialogState(
                        RenameDialogState.RENAME_EXTENSION_DIALOG_DEFAULT,
                        DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                return;
            }
            runRenameCallback();
        } else {
            mRenameDialogCoordinator.dismissDialog(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    private void onRenameExtensionDialogClick(boolean isPositiveButton) {
        if (isPositiveButton) {
            runRenameCallback();
        } else {
            processDialogState(
                    RenameDialogState.RENAME_EXTENSION_DIALOG_CANCEL,
                    DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    private void onRenameExtensionDialogDismiss(int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
            processDialogState(RenameDialogState.RENAME_EXTENSION_DIALOG_CANCEL, dismissalCause);
        }
    }
}
