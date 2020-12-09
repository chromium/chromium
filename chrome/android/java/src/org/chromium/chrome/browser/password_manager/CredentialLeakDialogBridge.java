// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

import java.lang.ref.WeakReference;

/** JNI call glue between the native password manager CredentialLeak class and Java objects. */
public class CredentialLeakDialogBridge {
    private long mNativeCredentialLeakDialogViewAndroid;
    private final PasswordManagerDialogCoordinator mCredentialLeakDialog;
    private final WeakReference<ChromeActivity> mActivity;

    private CredentialLeakDialogBridge(
            WindowAndroid windowAndroid, long nativeCredentialLeakDialogViewAndroid) {
        mNativeCredentialLeakDialogViewAndroid = nativeCredentialLeakDialogViewAndroid;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mActivity = new WeakReference<>(activity);
        mCredentialLeakDialog = new PasswordManagerDialogCoordinator(
                activity.getModalDialogManager(), activity.findViewById(android.R.id.content),
                activity.getBrowserControlsManager(), activity.getControlContainerHeightResource());
    }

    @CalledByNative
    public static CredentialLeakDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new CredentialLeakDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String credentialLeakTitle, String credentialLeakDetails,
            String positiveButton, String negativeButton) {
        if (mActivity.get() == null) return;

        PasswordManagerDialogContents contents = createDialogContents(
                credentialLeakTitle, credentialLeakDetails, positiveButton, negativeButton);
        contents.setPrimaryButtonFilled(negativeButton != null);
        contents.setHelpButtonCallback(this::showHelpArticle);

        mCredentialLeakDialog.initialize(mActivity.get(), contents);
        mCredentialLeakDialog.showDialog();
    }

    private PasswordManagerDialogContents createDialogContents(String credentialLeakTitle,
            String credentialLeakDetails, String positiveButton, String negativeButton) {
        return new PasswordManagerDialogContents(credentialLeakTitle, credentialLeakDetails,
                R.drawable.password_checkup_warning, positiveButton, negativeButton, this::onClick);
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

    private void showHelpArticle() {
        if (mActivity.get() == null) return;

        Profile profile = Profile.fromWebContents(
                mActivity.get().getActivityTabProvider().get().getWebContents());
        HelpAndFeedbackLauncherImpl.getInstance().show(mActivity.get(),
                mActivity.get().getString(R.string.help_context_password_leak_detection), profile,
                null);
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
