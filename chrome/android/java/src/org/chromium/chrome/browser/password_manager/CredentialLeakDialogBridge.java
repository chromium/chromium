// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** JNI call glue between the native password manager CredentialLeak class and Java objects. */
public class CredentialLeakDialogBridge {
    private long mNativeCredentialLeakDialogViewAndroid;
    private final PasswordManagerDialogCoordinator mCredentialLeakDialog;

    private CredentialLeakDialogBridge(
            WindowAndroid windowAndroid, long nativeCredentialLeakDialogViewAndroid) {
        mNativeCredentialLeakDialogViewAndroid = nativeCredentialLeakDialogViewAndroid;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mCredentialLeakDialog = new PasswordManagerDialogCoordinator(
                windowAndroid.getContext().get(), activity.getModalDialogManager(),
                activity.findViewById(android.R.id.content), activity.getFullscreenManager(),
                activity.getControlContainerHeightResource());
    }

    @CalledByNative
    public static CredentialLeakDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new CredentialLeakDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String credentialLeakTitle, String credentialLeakDetails,
            String positiveButton, String negativeButton) {
        boolean primaryButtonFilled = negativeButton != null;
        mCredentialLeakDialog.showDialog(credentialLeakTitle, credentialLeakDetails,
                R.drawable.password_check_warning, positiveButton, negativeButton, this::onClick,
                primaryButtonFilled, ModalDialogManager.ModalDialogType.APP);
    }

    @CalledByNative
    private void destroy() {
        mNativeCredentialLeakDialogViewAndroid = 0;
        mCredentialLeakDialog.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClick(@DialogDismissalCause int dismissalCause) {
        if (mNativeCredentialLeakDialogViewAndroid == 0) return;
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                CredentialLeakDialogBridgeJni.get().accepted(
                        mNativeCredentialLeakDialogViewAndroid, CredentialLeakDialogBridge.this);
                return;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                CredentialLeakDialogBridgeJni.get().cancelled(
                        mNativeCredentialLeakDialogViewAndroid, CredentialLeakDialogBridge.this);
                return;
            default:
                CredentialLeakDialogBridgeJni.get().closed(
                        mNativeCredentialLeakDialogViewAndroid, CredentialLeakDialogBridge.this);
        }
    }

    @NativeMethods
    interface Natives {
        void accepted(
                long nativeCredentialLeakDialogViewAndroid, CredentialLeakDialogBridge caller);
        void cancelled(
                long nativeCredentialLeakDialogViewAndroid, CredentialLeakDialogBridge caller);
        void closed(long nativeCredentialLeakDialogViewAndroid, CredentialLeakDialogBridge caller);
    }
}
