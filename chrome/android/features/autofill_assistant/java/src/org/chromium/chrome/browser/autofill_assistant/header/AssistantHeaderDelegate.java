// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

@JNINamespace("autofill_assistant")
class AssistantHeaderDelegate {
    private long mNativeAssistantHeaderDelegate;

    @CalledByNative
    private static AssistantHeaderDelegate create(long nativeAssistantHeaderDelegate) {
        return new AssistantHeaderDelegate(nativeAssistantHeaderDelegate);
    }

    private AssistantHeaderDelegate(long nativeAssistantHeaderDelegate) {
        mNativeAssistantHeaderDelegate = nativeAssistantHeaderDelegate;
    }

    void onFeedbackButtonClicked() {
        if (mNativeAssistantHeaderDelegate != 0) {
            AssistantHeaderDelegateJni.get().onFeedbackButtonClicked(
                    mNativeAssistantHeaderDelegate, AssistantHeaderDelegate.this);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantHeaderDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onFeedbackButtonClicked(
                long nativeAssistantHeaderDelegate, AssistantHeaderDelegate caller);
    }
}
