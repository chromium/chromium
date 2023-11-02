// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controller that allows the native autofill code to show a {@link Snackbar}.
 * For example: After a Virtual Card is auto-filled, a snackbar is shown with an informational
 * message and an action button.
 */
@JNINamespace("autofill")
public class AutofillSnackbarController implements SnackbarManager.SnackbarController {
    // Duration in milliseconds for which the snackbar should be shown.
    private static final int DURATION_MS = 10000;

    private final long mNativeAutofillSnackbarView;
    private final SnackbarManager mSnackbarManager;

    public AutofillSnackbarController(
            long nativeAutofillSnackbarView, SnackbarManager snackbarManager) {
        this.mNativeAutofillSnackbarView = nativeAutofillSnackbarView;
        this.mSnackbarManager = snackbarManager;
    }

    @Override
    public void onAction(Object actionData) {
        AutofillSnackbarControllerJni.get().onActionClicked(mNativeAutofillSnackbarView);
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        AutofillSnackbarControllerJni.get().onDismissed(mNativeAutofillSnackbarView);
    }

    @CalledByNative
    public static AutofillSnackbarController create(
            long nativeAutofillSnackbarView, WindowAndroid windowAndroid) {
        return new AutofillSnackbarController(
                nativeAutofillSnackbarView, SnackbarManagerProvider.from(windowAndroid));
    }

    /**
     * Show the snackbar.
     *
     * @param message Message to be shown in the snackbar.
     * @param action Label for the action button in the snackbar.
     */
    @CalledByNative
    public void show(String message, String action) {
        Snackbar snackBar = Snackbar.make(message, this, Snackbar.TYPE_ACTION,
                                            Snackbar.UMA_AUTOFILL_VIRTUAL_CARD_FILLED)
                                    .setAction(action, /*objectData=*/null);
        // Wrap the message text if it doesn't fit on a single line. The action text will not wrap
        // though.
        snackBar.setSingleLine(false);
        snackBar.setDuration(DURATION_MS);
        mSnackbarManager.showSnackbar(snackBar);
    }

    /** Dismiss the autofill snackbar if it's showing. No-op if it's not showing. */
    @CalledByNative
    public void dismiss() {
        mSnackbarManager.dismissSnackbars(this);
    }

    @NativeMethods
    public interface Natives {
        void onActionClicked(long nativeAutofillSnackbarViewAndroid);
        void onDismissed(long nativeAutofillSnackbarViewAndroid);
    }
}
