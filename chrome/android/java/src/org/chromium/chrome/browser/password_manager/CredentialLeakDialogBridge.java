// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue between the native password manager CredentialLeak class and Java objects. */
public class CredentialLeakDialogBridge {
    private long mNativeCredentialLeakDialogViewAndroid;
    private final PasswordManagerDialogCoordinator mCredentialLeakDialog;
    private final WindowAndroid mWindowAndroid;

    private CredentialLeakDialogBridge(
            @NonNull WindowAndroid windowAndroid, long nativeCredentialLeakDialogViewAndroid) {
        mNativeCredentialLeakDialogViewAndroid = nativeCredentialLeakDialogViewAndroid;
        mWindowAndroid = windowAndroid;

        mCredentialLeakDialog =
                new PasswordManagerDialogCoordinator(
                        windowAndroid.getModalDialogManager(),
                        windowAndroid.getActivity().get().findViewById(android.R.id.content),
                        BrowserControlsManagerSupplier.getValueOrNullFrom(windowAndroid));
    }

    @CalledByNative
    public static CredentialLeakDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new CredentialLeakDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(
            @JniType("std::u16string") String credentialLeakTitle,
            @JniType("std::u16string") String credentialLeakDetails,
            @JniType("std::u16string") String positiveButton,
            @Nullable String negativeButton) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return;

        @DrawableRes int headerDrawableId;
        headerDrawableId = R.drawable.password_check_header_red;

        PasswordManagerDialogContents contents =
                createDialogContents(
                        credentialLeakTitle,
                        credentialLeakDetails,
                        headerDrawableId,
                        positiveButton,
                        negativeButton);
        contents.setPrimaryButtonFilled(negativeButton != null);
        contents.setHelpButtonCallback(this::showHelpArticle);

        mCredentialLeakDialog.initialize(activity, contents);
        mCredentialLeakDialog.showDialog();
    }

    private PasswordManagerDialogContents createDialogContents(
            String credentialLeakTitle,
            String credentialLeakDetails,
            int illustrationId,
            String positiveButton,
            String negativeButton) {
        return new PasswordManagerDialogContents(
                credentialLeakTitle,
                credentialLeakDetails,
                illustrationId,
                positiveButton,
                negativeButton,
                this::onClick);
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
                CredentialLeakDialogBridgeJni.get()
                        .accepted(
                                mNativeCredentialLeakDialogViewAndroid,
                                CredentialLeakDialogBridge.this);
                return;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                CredentialLeakDialogBridgeJni.get()
                        .cancelled(
                                mNativeCredentialLeakDialogViewAndroid,
                                CredentialLeakDialogBridge.this);
                return;
            default:
                CredentialLeakDialogBridgeJni.get()
                        .closed(
                                mNativeCredentialLeakDialogViewAndroid,
                                CredentialLeakDialogBridge.this);
        }
    }

    private void showHelpArticle() {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return;

        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(mWindowAndroid);
        if (currentTab == null) return;

        Profile profile = currentTab.getProfile();
        HelpAndFeedbackLauncherImpl.getForProfile(profile)
                .show(
                        activity,
                        activity.getString(R.string.help_context_password_leak_detection),
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
