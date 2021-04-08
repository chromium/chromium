// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * JNI wrapper for C++ SaveAddressProfilePromptController.
 */
@JNINamespace("autofill")
final class SaveAddressProfilePromptController {
    private long mNativeSaveAddressProfilePromptController;

    private SaveAddressProfilePromptController(long nativeSaveAddressProfilePromptController) {
        mNativeSaveAddressProfilePromptController = nativeSaveAddressProfilePromptController;
    }

    @CalledByNative
    private static SaveAddressProfilePromptController create(
            long nativeSaveAddressProfilePromptController) {
        return new SaveAddressProfilePromptController(nativeSaveAddressProfilePromptController);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeSaveAddressProfilePromptController = 0;
    }

    public void onPromptDismissed() {
        if (mNativeSaveAddressProfilePromptController != 0) {
            SaveAddressProfilePromptControllerJni.get().onPromptDismissed(
                    mNativeSaveAddressProfilePromptController,
                    SaveAddressProfilePromptController.this);
        }
    }

    public void onUserAccepted() {
        if (mNativeSaveAddressProfilePromptController != 0) {
            SaveAddressProfilePromptControllerJni.get().onUserAccepted(
                    mNativeSaveAddressProfilePromptController,
                    SaveAddressProfilePromptController.this);
        }
    }

    public void onUserDeclined() {
        if (mNativeSaveAddressProfilePromptController != 0) {
            SaveAddressProfilePromptControllerJni.get().onUserDeclined(
                    mNativeSaveAddressProfilePromptController,
                    SaveAddressProfilePromptController.this);
        }
    }

    @NativeMethods
    interface Natives {
        void onPromptDismissed(long nativeSaveAddressProfilePromptController,
                SaveAddressProfilePromptController caller);
        void onUserAccepted(long nativeSaveAddressProfilePromptController,
                SaveAddressProfilePromptController caller);
        void onUserDeclined(long nativeSaveAddressProfilePromptController,
                SaveAddressProfilePromptController caller);
    }
}
