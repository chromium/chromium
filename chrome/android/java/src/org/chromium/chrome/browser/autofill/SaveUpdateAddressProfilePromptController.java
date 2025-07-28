// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.AutofillProfile;

/** JNI wrapper for C++ SaveUpdateAddressProfilePromptController. */
@JNINamespace("autofill")
@NullMarked
final class SaveUpdateAddressProfilePromptController {
    private long mNativeSaveUpdateAddressProfilePromptController;

    private SaveUpdateAddressProfilePromptController(
            long nativeSaveUpdateAddressProfilePromptController) {
        mNativeSaveUpdateAddressProfilePromptController =
                nativeSaveUpdateAddressProfilePromptController;
    }

    @CalledByNative
    @VisibleForTesting
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
                    .onPromptDismissed(mNativeSaveUpdateAddressProfilePromptController);
        }
    }

    public void onUserAccepted() {
        if (mNativeSaveUpdateAddressProfilePromptController != 0) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserAccepted(mNativeSaveUpdateAddressProfilePromptController);
        }
    }

    public void onUserDeclined() {
        if (mNativeSaveUpdateAddressProfilePromptController != 0) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserDeclined(mNativeSaveUpdateAddressProfilePromptController);
        }
    }

    public void onUserEdited(AutofillProfile profile) {
        if (mNativeSaveUpdateAddressProfilePromptController != 0) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserEdited(mNativeSaveUpdateAddressProfilePromptController, profile);
        }
    }

    @NativeMethods
    interface Natives {
        void onPromptDismissed(long nativeSaveUpdateAddressProfilePromptController);

        void onUserAccepted(long nativeSaveUpdateAddressProfilePromptController);

        void onUserDeclined(long nativeSaveUpdateAddressProfilePromptController);

        void onUserEdited(
                long nativeSaveUpdateAddressProfilePromptController, AutofillProfile profile);
    }
}
