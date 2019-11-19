// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * JNI call glue between native password generation and Java objects.
 */
public class PasswordGenerationDialogBridge {
    private long mNativePasswordGenerationDialogViewAndroid;
    private final PasswordGenerationDialogCoordinator mPasswordGenerationDialog;
    // TODO(ioanap): Get the generated password from the model once editing is in place.
    private String mGeneratedPassword;

    private PasswordGenerationDialogBridge(
            WindowAndroid windowAndroid, long nativePasswordGenerationDialogViewAndroid) {
        mNativePasswordGenerationDialogViewAndroid = nativePasswordGenerationDialogViewAndroid;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mPasswordGenerationDialog = new PasswordGenerationDialogCoordinator(activity);
    }

    @CalledByNative
    public static PasswordGenerationDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new PasswordGenerationDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String generatedPassword, String explanationString) {
        mGeneratedPassword = generatedPassword;
        mPasswordGenerationDialog.showDialog(generatedPassword,
                explanationString,
                this::onPasswordAcceptedOrRejected);
    }

    @CalledByNative
    private void destroy() {
        mNativePasswordGenerationDialogViewAndroid = 0;
        mPasswordGenerationDialog.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onPasswordAcceptedOrRejected(boolean accepted) {
        if (mNativePasswordGenerationDialogViewAndroid == 0) return;

        if (accepted) {
            PasswordGenerationDialogBridgeJni.get().passwordAccepted(
                    mNativePasswordGenerationDialogViewAndroid, PasswordGenerationDialogBridge.this,
                    mGeneratedPassword);
        } else {
            PasswordGenerationDialogBridgeJni.get().passwordRejected(
                    mNativePasswordGenerationDialogViewAndroid,
                    PasswordGenerationDialogBridge.this);
        }
        mPasswordGenerationDialog.dismissDialog(DialogDismissalCause.ACTION_ON_CONTENT);
    }

    @NativeMethods
    interface Natives {
        void passwordAccepted(long nativePasswordGenerationDialogViewAndroid,
                PasswordGenerationDialogBridge caller, String generatedPassword);
        void passwordRejected(long nativePasswordGenerationDialogViewAndroid,
                PasswordGenerationDialogBridge caller);
    }
}
