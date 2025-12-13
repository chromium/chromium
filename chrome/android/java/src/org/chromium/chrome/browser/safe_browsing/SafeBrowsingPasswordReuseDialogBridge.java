// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogContents;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue between the native and Java for password reuse dialogs. */
@JNINamespace("safe_browsing")
@NullMarked
public class SafeBrowsingPasswordReuseDialogBridge {
    // The address of its C++ counterpart.
    private long mNativePasswordReuseDialogViewAndroid;
    // The coordinator for the password manager illustration modal dialog. Manages the sub-component
    // objects.
    private final PasswordManagerDialogCoordinator mDialogCoordinator;
    // Used to initialize the custom view of the dialog.
    private final WindowAndroid mWindowAndroid;
    // Primarily used in tests to validate fields of PasswordManagerDialogContents.
    private @Nullable PasswordManagerDialogContents mPasswordManagerDialogContents;

    private SafeBrowsingPasswordReuseDialogBridge(
            WindowAndroid windowAndroid, long nativePasswordReuseDialogViewAndroid) {
        mNativePasswordReuseDialogViewAndroid = nativePasswordReuseDialogViewAndroid;
        mWindowAndroid = windowAndroid;
        mDialogCoordinator =
                new PasswordManagerDialogCoordinator(
                        assertNonNull(mWindowAndroid.getModalDialogManager()),
                        assumeNonNull(mWindowAndroid.getActivity().get())
                                .findViewById(android.R.id.content),
                        BrowserControlsManagerSupplier.getValueOrNullFrom(mWindowAndroid));
        mPasswordManagerDialogContents = null;
    }

    private SafeBrowsingPasswordReuseDialogBridge(
            WindowAndroid windowAndroid,
            long nativePasswordReuseDialogViewAndroid,
            PasswordManagerDialogCoordinator dialogCoordinator) {
        mNativePasswordReuseDialogViewAndroid = nativePasswordReuseDialogViewAndroid;
        mWindowAndroid = windowAndroid;
        mDialogCoordinator = dialogCoordinator;
        mPasswordManagerDialogContents = null;
    }

    public static SafeBrowsingPasswordReuseDialogBridge createForTests(
            WindowAndroid windowAndroid,
            long nativePasswordReuseDialogViewAndroid,
            PasswordManagerDialogCoordinator dialogCoordinator) {
        return new SafeBrowsingPasswordReuseDialogBridge(
                windowAndroid, nativePasswordReuseDialogViewAndroid, dialogCoordinator);
    }

    public @Nullable PasswordManagerDialogContents getPasswordManagerDialogContentsForTests() {
        return mPasswordManagerDialogContents;
    }

    @CalledByNative
    public static SafeBrowsingPasswordReuseDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new SafeBrowsingPasswordReuseDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(
            @JniType("std::u16string") String dialogTitle,
            @JniType("std::u16string") String dialogDetails,
            @JniType("std::u16string") String primaryButtonText,
            @JniType("std::u16string") String secondaryButtonText) {
        if (mWindowAndroid.getActivity().get() == null) return;

        boolean hasSecondaryButtonText = !secondaryButtonText.isEmpty();
        PasswordManagerDialogContents contents =
                createDialogContents(
                        dialogTitle,
                        dialogDetails,
                        primaryButtonText,
                        hasSecondaryButtonText ? secondaryButtonText : null);
        contents.setPrimaryButtonFilled(hasSecondaryButtonText);

        mDialogCoordinator.initialize(mWindowAndroid.getActivity().get(), contents);
        mDialogCoordinator.showDialog();
    }

    private PasswordManagerDialogContents createDialogContents(
            String credentialLeakTitle,
            String credentialLeakDetails,
            String positiveButton,
            @Nullable String negativeButton) {
        Callback<Integer> onClick =
                negativeButton != null
                        ? this::onClickWithNegativeButtonEnabled
                        : this::onClickWithNegativeButtonDisabled;

        mPasswordManagerDialogContents =
                new PasswordManagerDialogContents(
                        credentialLeakTitle,
                        credentialLeakDetails,
                        R.drawable.password_checkup_warning,
                        positiveButton,
                        negativeButton,
                        onClick);
        return mPasswordManagerDialogContents;
    }

    @CalledByNative
    private void destroy() {
        mNativePasswordReuseDialogViewAndroid = 0;
        mDialogCoordinator.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClickWithNegativeButtonDisabled(@DialogDismissalCause int dismissalCause) {
        // 0 indicates its C++ counterpart has already been destroyed.
        if (mNativePasswordReuseDialogViewAndroid == 0) return;

        SafeBrowsingPasswordReuseDialogBridgeJni.get().close(mNativePasswordReuseDialogViewAndroid);
    }

    private void onClickWithNegativeButtonEnabled(@DialogDismissalCause int dismissalCause) {
        // 0 indicates its C++ counterpart has already been destroyed.
        if (mNativePasswordReuseDialogViewAndroid == 0) return;

        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                SafeBrowsingPasswordReuseDialogBridgeJni.get()
                        .checkPasswords(mNativePasswordReuseDialogViewAndroid);
                return;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                SafeBrowsingPasswordReuseDialogBridgeJni.get()
                        .ignore(mNativePasswordReuseDialogViewAndroid);
                return;
            default:
                SafeBrowsingPasswordReuseDialogBridgeJni.get()
                        .close(mNativePasswordReuseDialogViewAndroid);
        }
    }

    @NativeMethods
    interface Natives {
        void checkPasswords(long nativePasswordReuseDialogViewAndroid);

        void ignore(long nativePasswordReuseDialogViewAndroid);

        void close(long nativePasswordReuseDialogViewAndroid);
    }
}
