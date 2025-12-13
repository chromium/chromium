// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

/**
 * A {@link TabModelActionListener} that is used as part of handling an ungroup before pinning a
 * tab.
 */
@NullMarked
/*package*/ class TabPinnerActionListener implements TabModelActionListener {
    private final @Nullable TabModelActionListener mInnerListener;
    private final OnceRunnable mDoPinRunnable;
    private boolean mCollaborationDialogShown;

    /** A {@link Runnable} that is only run once. */
    private static class OnceRunnable implements Runnable {
        private @Nullable Runnable mRunnable;

        OnceRunnable(Runnable runnable) {
            mRunnable = runnable;
        }

        @Override
        public void run() {
            assumeNonNull(mRunnable);
            mRunnable.run();
            mRunnable = null;
        }
    }

    public TabPinnerActionListener(
            Runnable doPinRunnable, @Nullable TabModelActionListener innerListener) {
        mDoPinRunnable = new OnceRunnable(doPinRunnable);
        mInnerListener = innerListener;
    }

    /**
     * The collaboration dialog was shown so the ungroup has already happened. We should run the pin
     * operation now.
     */
    public void pinIfCollaborationDialogShown() {
        if (mCollaborationDialogShown) {
            mDoPinRunnable.run();
        }
    }

    @Override
    public void willPerformActionOrShowDialog(@DialogType int dialogType, boolean willSkipDialog) {
        if (mInnerListener != null) {
            mInnerListener.willPerformActionOrShowDialog(dialogType, willSkipDialog);
        }
        mCollaborationDialogShown = dialogType == DialogType.COLLABORATION && !willSkipDialog;
    }

    @Override
    public void onConfirmationDialogResult(
            @DialogType int dialogType, @ActionConfirmationResult int result) {
        if (mInnerListener != null) {
            mInnerListener.onConfirmationDialogResult(dialogType, result);
        }
        switch (dialogType) {
            case DialogType.SYNC:
                handleSyncDialogResult(result);
                break;
            case DialogType.COLLABORATION:
                break; // Intentional no-op work was already done.
            case DialogType.NONE:
                mDoPinRunnable.run();
                break;
            default:
                assert false : "Unexpected dialog type: " + dialogType;
        }
    }

    private void handleSyncDialogResult(@ActionConfirmationResult int result) {
        switch (result) {
            case ActionConfirmationResult.CONFIRMATION_NEGATIVE:
                // Action was cancelled.
                break;
            case ActionConfirmationResult.CONFIRMATION_POSITIVE: // Fallthrough.
            case ActionConfirmationResult.IMMEDIATE_CONTINUE:
                mDoPinRunnable.run();
                break;
            default:
                assert false : "Unexpected result: " + result;
        }
    }
}
