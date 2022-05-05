// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/** JNI call glue between the native password manager CredentialLeak class and Java objects. */
public class CredentialLeakDialogBridge {
    private long mNativeCredentialLeakDialogViewAndroid;
    private final WindowAndroid mWindowAndroid;

    private CredentialLeakDialogBridge(
            @NonNull WindowAndroid windowAndroid, long nativeCredentialLeakDialogViewAndroid) {
        mNativeCredentialLeakDialogViewAndroid = nativeCredentialLeakDialogViewAndroid;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public static CredentialLeakDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new CredentialLeakDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String credentialLeakTitle, String credentialLeakDetails,
            boolean isChangeAutomaticallyAvailable, String positiveButton, String negativeButton) {
    }

    @CalledByNative
    private void destroy() {
        mNativeCredentialLeakDialogViewAndroid = 0;
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
