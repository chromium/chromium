// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI call glue between native password generation and Java objects.
 */
public class PasswordGenerationDialogBridge {
    private long mNativePasswordGenerationDialogViewAndroid;
    // TODO(ioanap): Get the generated password from the model once editing is in place.
    private String mGeneratedPassword;

    private PasswordGenerationDialogBridge(
            WindowAndroid windowAndroid, long nativePasswordGenerationDialogViewAndroid) {
        mNativePasswordGenerationDialogViewAndroid = nativePasswordGenerationDialogViewAndroid;
    }

    @CalledByNative
    public static PasswordGenerationDialogBridge create(
            WindowAndroid windowAndroid, long nativeDialog) {
        return new PasswordGenerationDialogBridge(windowAndroid, nativeDialog);
    }

    @CalledByNative
    public void showDialog(String generatedPassword, String explanationString) {
        mGeneratedPassword = generatedPassword;
    }

    @CalledByNative
    private void destroy() {
        mNativePasswordGenerationDialogViewAndroid = 0;
    }

    @NativeMethods
    interface Natives {
        void passwordAccepted(long nativePasswordGenerationDialogViewAndroid,
                PasswordGenerationDialogBridge caller, String generatedPassword);
        void passwordRejected(long nativePasswordGenerationDialogViewAndroid,
                PasswordGenerationDialogBridge caller);
    }
}
