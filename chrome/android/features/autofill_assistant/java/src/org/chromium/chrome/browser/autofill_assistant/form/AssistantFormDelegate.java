// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Delegate for the form UI. */
@JNINamespace("autofill_assistant")
class AssistantFormDelegate {
    private long mNativeAssistantFormDelegate;

    @CalledByNative
    private static AssistantFormDelegate create(long nativeAssistantFormDelegate) {
        return new AssistantFormDelegate(nativeAssistantFormDelegate);
    }

    private AssistantFormDelegate(long nativeAssistantFormDelegate) {
        mNativeAssistantFormDelegate = nativeAssistantFormDelegate;
    }

    void onCounterChanged(int inputIndex, int counterIndex, int value) {
        if (mNativeAssistantFormDelegate != 0) {
            AssistantFormDelegateJni.get().onCounterChanged(mNativeAssistantFormDelegate,
                    AssistantFormDelegate.this, inputIndex, counterIndex, value);
        }
    }
    void onChoiceSelectionChanged(int inputIndex, int choiceIndex, boolean selected) {
        if (mNativeAssistantFormDelegate != 0) {
            AssistantFormDelegateJni.get().onChoiceSelectionChanged(mNativeAssistantFormDelegate,
                    AssistantFormDelegate.this, inputIndex, choiceIndex, selected);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantFormDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onCounterChanged(long nativeAssistantFormDelegate, AssistantFormDelegate caller,
                int inputIndex, int counterIndex, long nativeAssistantOverlayDelegate);
        void onChoiceSelectionChanged(long nativeAssistantFormDelegate,
                AssistantFormDelegate caller, int inputIndex, int choiceIndex, boolean selected);
    }
}
