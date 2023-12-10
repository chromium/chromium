// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.autofill.AutofillProfile;

/** JNI wrapper for C++ SaveUpdateAddressProfilePromptController. */
@JNINamespace("autofill")
final class SaveUpdateAddressProfilePromptController {
    private long mNativeSaveUpdateAddressProfilePromptController;

    private SaveUpdateAddressProfilePromptController(
            long nativeSaveUpdateAddressProfilePromptController) {
        mNativeSaveUpdateAddressProfilePromptController =
                nativeSaveUpdateAddressProfilePromptController;
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static SaveUpdateAddressProfilePromptController create(
            long nativeSaveUpdateAddressProfilePromptController) {
        return new SaveUpdateAddressProfilePromptController(
                nativeSaveUpdateAddressProfilePromptController);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeSaveUpdateAddressProfilePromptController = 0;
    }

    public void onPromptDismissed() {
        if (mNativeSaveUpdateAddressProfilePromptController != 0) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onPromptDismissed(
                            mNativeSaveUpdateAddressProfilePromptController,
                            SaveUpdateAddressProfilePromptController.this);
        }
    }

    public void onUserAccepted() {
        if (mNativeSaveUpdateAddressProfilePromptController != 0) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserAccepted(
                            mNativeSaveUpdateAddressProfilePromptController,
                            SaveUpdateAddressProfilePromptController.this);
        }
    }

    public void onUserDeclined() {
        if (mNativeSaveUpdateAddressProfilePromptController != 0) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserDeclined(
                            mNativeSaveUpdateAddressProfilePromptController,
                            SaveUpdateAddressProfilePromptController.this);
        }
    }

    public void onUserEdited(AutofillProfile profile) {
        if (mNativeSaveUpdateAddressProfilePromptController != 0) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserEdited(
                            mNativeSaveUpdateAddressProfilePromptController,
                            SaveUpdateAddressProfilePromptController.this,
                            profile);
        }
    }

    @NativeMethods
    interface Natives {
        void onPromptDismissed(
                long nativeSaveUpdateAddressProfilePromptController,
                SaveUpdateAddressProfilePromptController caller);

        void onUserAccepted(
                long nativeSaveUpdateAddressProfilePromptController,
                SaveUpdateAddressProfilePromptController caller);

        void onUserDeclined(
                long nativeSaveUpdateAddressProfilePromptController,
                SaveUpdateAddressProfilePromptController caller);

        void onUserEdited(
                long nativeSaveUpdateAddressProfilePromptController,
                SaveUpdateAddressProfilePromptController caller,
                AutofillProfile profile);
    }
}
