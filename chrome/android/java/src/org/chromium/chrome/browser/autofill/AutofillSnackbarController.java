// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controller that allows the native autofill code to show a {@link Snackbar}. For example: After a
 * Virtual Card is auto-filled, a snackbar is shown with an informational message and an action
 * button.
 */
@JNINamespace("autofill")
public class AutofillSnackbarController implements SnackbarManager.SnackbarController {
    private final SnackbarManager mSnackbarManager;
    private long mNativeAutofillSnackbarView;

    @VisibleForTesting
    AutofillSnackbarController(long nativeAutofillSnackbarView, SnackbarManager snackbarManager) {
        this.mNativeAutofillSnackbarView = nativeAutofillSnackbarView;
        this.mSnackbarManager = snackbarManager;
    }

    @Override
    public void onAction(Object actionData) {
        if (mNativeAutofillSnackbarView == 0) {
            return;
        }
        // Notify the backend that the user clicked on the action button.
        AutofillSnackbarControllerJni.get().onActionClicked(mNativeAutofillSnackbarView);
        // Since the snackbar gets dismissed when the action is clicked, notify the backend about
        // the dismissal as well.
        AutofillSnackbarControllerJni.get().onDismissed(mNativeAutofillSnackbarView);
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        if (mNativeAutofillSnackbarView == 0) {
            return;
        }
        AutofillSnackbarControllerJni.get().onDismissed(mNativeAutofillSnackbarView);
    }

    @CalledByNative
    static AutofillSnackbarController create(
            long nativeAutofillSnackbarView, WindowAndroid windowAndroid) {
        return new AutofillSnackbarController(
                nativeAutofillSnackbarView, SnackbarManagerProvider.from(windowAndroid));
    }

    /**
     * Show the snackbar.
     *
     * @param message Message to be shown in the snackbar.
     * @param action Label for the action button in the snackbar.
     * @param duration The duration (in ms) for which the snackbar should be shown.
     */
    @CalledByNative
    void show(
            @JniType("std::u16string") String message,
            @JniType("std::u16string") String action,
            int duration) {
        Snackbar snackBar =
                Snackbar.make(
                                message,
                                this,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_AUTOFILL_VIRTUAL_CARD_FILLED)
                        .setAction(action, /* objectData= */ null);
        // Wrap the message text if it doesn't fit on a single line. The action text will not wrap
        // though.
        snackBar.setSingleLine(false);
        snackBar.setDuration(duration);
        mSnackbarManager.showSnackbar(snackBar);
    }

    /** Dismiss the autofill snackbar if it's showing. No-op if it's not showing. */
    @CalledByNative
    void dismiss() {
        mSnackbarManager.dismissSnackbars(this);
        mNativeAutofillSnackbarView = 0;
    }

    @NativeMethods
    public interface Natives {
        void onActionClicked(long nativeAutofillSnackbarViewAndroid);

        void onDismissed(long nativeAutofillSnackbarViewAndroid);
    }
}
