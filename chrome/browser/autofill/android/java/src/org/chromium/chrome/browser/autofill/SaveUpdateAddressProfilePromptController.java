// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillProfile;

/** JNI wrapper for C++ SaveUpdateAddressProfilePromptController. */
@JNINamespace("autofill")
@NullMarked
final class SaveUpdateAddressProfilePromptController {
    @JniType("::autofill::SaveUpdateAddressProfilePromptController")
    interface NativeSaveUpdateAddressProfilePromptController extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeSaveUpdateAddressProfilePromptController>
            mNativeSaveUpdateAddressProfilePromptController;

    private SaveUpdateAddressProfilePromptController(
            JniRawPtr<NativeSaveUpdateAddressProfilePromptController>
                    nativeSaveUpdateAddressProfilePromptController) {
        mNativeSaveUpdateAddressProfilePromptController =
                nativeSaveUpdateAddressProfilePromptController;
    }

    @CalledByNative
    @VisibleForTesting
    static SaveUpdateAddressProfilePromptController create(
            JniRawPtr<NativeSaveUpdateAddressProfilePromptController>
                    nativeSaveUpdateAddressProfilePromptController) {
        return new SaveUpdateAddressProfilePromptController(
                nativeSaveUpdateAddressProfilePromptController);
    }

    @CalledByNative
    @VisibleForTesting
    void onNativeDestroyed() {
        if (mNativeSaveUpdateAddressProfilePromptController != null) {
            mNativeSaveUpdateAddressProfilePromptController.release();
            mNativeSaveUpdateAddressProfilePromptController = null;
        }
    }

    public void onPromptDismissed() {
        if (mNativeSaveUpdateAddressProfilePromptController != null) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onPromptDismissed(mNativeSaveUpdateAddressProfilePromptController);
        }
    }

    public void onUserAccepted() {
        if (mNativeSaveUpdateAddressProfilePromptController != null) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserAccepted(mNativeSaveUpdateAddressProfilePromptController);
        }
    }

    public void onUserDeclined() {
        if (mNativeSaveUpdateAddressProfilePromptController != null) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserDeclined(mNativeSaveUpdateAddressProfilePromptController);
        }
    }

    public void onUserEdited(AutofillProfile profile) {
        if (mNativeSaveUpdateAddressProfilePromptController != null) {
            SaveUpdateAddressProfilePromptControllerJni.get()
                    .onUserEdited(mNativeSaveUpdateAddressProfilePromptController, profile);
        }
    }

    @NativeMethods
    interface Natives {
        void onPromptDismissed(JniPtr<NativeSaveUpdateAddressProfilePromptController> self);

        void onUserAccepted(JniPtr<NativeSaveUpdateAddressProfilePromptController> self);

        void onUserDeclined(JniPtr<NativeSaveUpdateAddressProfilePromptController> self);

        void onUserEdited(
                JniPtr<NativeSaveUpdateAddressProfilePromptController> self,
                AutofillProfile profile);
    }
}
