// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** JNI wrapper for C++ AutofillAiSaveUpdateEntityPromptController. */
@JNINamespace("autofill")
@NullMarked
final class AutofillAiSaveUpdateEntityPromptController {
    private long mNativeAutofillAiSaveUpdateEntityPromptController;

    private AutofillAiSaveUpdateEntityPromptController(
            long nativeAutofillAiSaveUpdateEntityPromptController) {
        mNativeAutofillAiSaveUpdateEntityPromptController =
                nativeAutofillAiSaveUpdateEntityPromptController;
    }

    @CalledByNative
    @VisibleForTesting
    static AutofillAiSaveUpdateEntityPromptController create(
            long nativeAutofillAiSaveUpdateEntityPromptController) {
        return new AutofillAiSaveUpdateEntityPromptController(
                nativeAutofillAiSaveUpdateEntityPromptController);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeAutofillAiSaveUpdateEntityPromptController = 0;
    }

    public void openManagePasses() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != 0) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .openManagePasses(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    public void onPromptDismissed() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != 0) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onPromptDismissed(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    public void onUserAccepted() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != 0) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onUserAccepted(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    public void onUserDeclined() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != 0) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onUserDeclined(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    @NativeMethods
    interface Natives {
        void openManagePasses(long nativeAutofillAiSaveUpdateEntityPromptController);

        void onPromptDismissed(long nativeAutofillAiSaveUpdateEntityPromptController);

        void onUserAccepted(long nativeAutofillAiSaveUpdateEntityPromptController);

        void onUserDeclined(long nativeAutofillAiSaveUpdateEntityPromptController);
    }
}
