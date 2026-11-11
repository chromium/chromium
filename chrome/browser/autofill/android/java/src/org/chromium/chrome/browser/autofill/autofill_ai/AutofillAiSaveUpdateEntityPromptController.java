// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

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
import org.chromium.components.autofill.autofill_ai.EntityInstance;

/** JNI wrapper for C++ AutofillAiSaveUpdateEntityPromptController. */
@JNINamespace("autofill")
@NullMarked
final class AutofillAiSaveUpdateEntityPromptController {
    @JniType("::autofill::AutofillAiSaveUpdateEntityPromptController")
    interface NativeAutofillAiSaveUpdateEntityPromptController extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeAutofillAiSaveUpdateEntityPromptController>
            mNativeAutofillAiSaveUpdateEntityPromptController;

    private AutofillAiSaveUpdateEntityPromptController(
            JniRawPtr<NativeAutofillAiSaveUpdateEntityPromptController>
                    nativeAutofillAiSaveUpdateEntityPromptController) {
        mNativeAutofillAiSaveUpdateEntityPromptController =
                nativeAutofillAiSaveUpdateEntityPromptController;
    }

    @CalledByNative
    @VisibleForTesting
    static AutofillAiSaveUpdateEntityPromptController create(
            JniRawPtr<NativeAutofillAiSaveUpdateEntityPromptController>
                    nativeAutofillAiSaveUpdateEntityPromptController) {
        return new AutofillAiSaveUpdateEntityPromptController(
                nativeAutofillAiSaveUpdateEntityPromptController);
    }

    @CalledByNative
    @VisibleForTesting
    void onNativeDestroyed() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != null) {
            mNativeAutofillAiSaveUpdateEntityPromptController.release();
            mNativeAutofillAiSaveUpdateEntityPromptController = null;
        }
    }

    public void onWalletLinkClicked() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != null) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onWalletLinkClicked(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    public void onPromptDismissed() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != null) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onPromptDismissed(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    public void onUserAccepted() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != null) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onUserAccepted(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    public void onUserEdited(EntityInstance entityInstance) {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != null) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onUserEdited(
                            mNativeAutofillAiSaveUpdateEntityPromptController, entityInstance);
        }
    }

    public void onUserDeclined() {
        if (mNativeAutofillAiSaveUpdateEntityPromptController != null) {
            AutofillAiSaveUpdateEntityPromptControllerJni.get()
                    .onUserDeclined(mNativeAutofillAiSaveUpdateEntityPromptController);
        }
    }

    @NativeMethods
    interface Natives {
        void onWalletLinkClicked(JniPtr<NativeAutofillAiSaveUpdateEntityPromptController> self);

        void onPromptDismissed(JniPtr<NativeAutofillAiSaveUpdateEntityPromptController> self);

        void onUserAccepted(JniPtr<NativeAutofillAiSaveUpdateEntityPromptController> self);

        void onUserEdited(
                JniPtr<NativeAutofillAiSaveUpdateEntityPromptController> self,
                @JniType("autofill::EntityInstanceAndroid") EntityInstance entityInstance);

        void onUserDeclined(JniPtr<NativeAutofillAiSaveUpdateEntityPromptController> self);
    }
}
