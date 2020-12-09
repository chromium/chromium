// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogContents;
import org.chromium.chrome.browser.password_manager.PasswordManagerDialogCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

import java.lang.ref.WeakReference;

/** JNI call glue between the native and Java for password reuse dialogs. */
@JNINamespace("safe_browsing")
public class SafeBrowsingPasswordReuseDialogBridge {
    // The address of its C++ counterpart.
    private long mNativePasswordReuseDialogViewAndroid;
    // The coordinator for the password manager illustration modal dialog. Manages the sub-component
    // objects.
    private final PasswordManagerDialogCoordinator mDialogCoordinator;
    // Used to initialize the custom view of the dialog.
    private final WeakReference<ChromeActivity> mActivity;

    private SafeBrowsingPasswordReuseDialogBridge(
            WindowAndroid windowAndroid, long nativePasswordReuseDialogViewAndroid) {
        mNativePasswordReuseDialogViewAndroid = nativePasswordReuseDialogViewAndroid;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mActivity = new WeakReference<>(activity);
        mDialogCoordinator = new PasswordManagerDialogCoordinator(activity.getModalDialogManager(),
                activity.findViewById(android.R.id.content), activity.getBrowserControlsManager(),
                activity.getControlContainerHeightResource());
    }

    @CalledByNative
    public static SafeBrowsingPasswordReuseDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new SafeBrowsingPasswordReuseDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String dialogTitle, String dialogDetails, String buttonText,
            int[] boldStartRanges, int[] boldEndRanges) {
        if (mActivity.get() == null) return;

        PasswordManagerDialogContents contents =
                createDialogContents(dialogTitle, dialogDetails, buttonText);
        contents.setBoldRanges(boldStartRanges, boldEndRanges);

        mDialogCoordinator.initialize(mActivity.get(), contents);
        mDialogCoordinator.showDialog();
    }

    private PasswordManagerDialogContents createDialogContents(
            String credentialLeakTitle, String credentialLeakDetails, String positiveButton) {
        return new PasswordManagerDialogContents(credentialLeakTitle, credentialLeakDetails,
                R.drawable.password_checkup_warning, positiveButton, null, this::onClick);
    }

    @CalledByNative
    private void destroy() {
        mNativePasswordReuseDialogViewAndroid = 0;
        mDialogCoordinator.dismissDialog(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    private void onClick(@DialogDismissalCause int dismissalCause) {
        // 0 indicates its C++ counterpart has already been destroyed.
        if (mNativePasswordReuseDialogViewAndroid == 0) return;
        SafeBrowsingPasswordReuseDialogBridgeJni.get().close(
                mNativePasswordReuseDialogViewAndroid, SafeBrowsingPasswordReuseDialogBridge.this);
    }

    @NativeMethods
    interface Natives {
        void close(long nativePasswordReuseDialogViewAndroid,
                SafeBrowsingPasswordReuseDialogBridge caller);
    }
}
