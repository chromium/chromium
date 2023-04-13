// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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

    private final @NonNull Context mContext;
    private final @NonNull QuickDeleteDelegate mDelegate;
    private final @NonNull QuickDeleteDialogDelegate mDialogDelegate;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull LayoutManager mLayoutManager;

    /**
     * Constructor for the QuickDeleteController with a dialog and confirmation snackbar.
     *
     * @param context The associated {@link Context}.
     * @param delegate A {@link QuickDeleteDelegate} to perform the quick delete.
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager A {@link SnackbarManager} to show the quick delete snackbar.
     * @param layoutManager {@link LayoutManager} to use for showing the regular overview mode.
     * @param tabModelSelector {@link TabModelSelector} to use for opening the links in search
     *         history disambiguation notice.
     */
    public QuickDeleteController(@NonNull Context context, @NonNull QuickDeleteDelegate delegate,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager, @NonNull LayoutManager layoutManager,
            @NonNull TabModelSelector tabModelSelector) {
        mContext = context;
        mDelegate = delegate;
        mSnackbarManager = snackbarManager;
        mLayoutManager = layoutManager;
        mDialogDelegate = new QuickDeleteDialogDelegate(
                context, modalDialogManager, this::onDialogDismissed, tabModelSelector);
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
        mDialogDelegate.showDialog();
    }

    /**
     * A method called when the user confirms or cancels the dialog.
     *
     * TODO(crbug.com/1412087): Add implementation logic for the deletion.
     */
    private void onDialogDismissed(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED);
                mDelegate.performQuickDelete(this::onQuickDeleteFinished);
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED);
                break;
            default:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY);
                break;
        }
    }

    private void onQuickDeleteFinished() {
        navigateToTabSwitcher();
        triggerHapticFeedback();
        // TODO(crbug.com/1412087): Show post-delete animation.
        showSnackbar();
    }

    /**
     * A method to navigate to tab switcher.
     */
    private void navigateToTabSwitcher() {
        if (mLayoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) return;
        mLayoutManager.showLayout(LayoutType.TAB_SWITCHER, /*animate=*/true);
    }

    private void triggerHapticFeedback() {
        Vibrator v = (Vibrator) mContext.getSystemService(Context.VIBRATOR_SERVICE);
        final long duration = 50;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            v.vibrate(VibrationEffect.createOneShot(duration, VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            // Deprecated in API 26.
            v.vibrate(duration);
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
