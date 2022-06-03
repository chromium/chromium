// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Delegate for the bottom bar which forwards events to a native counterpart. */
@JNINamespace("autofill_assistant")
public class AssistantBottomBarNativeDelegate implements AssistantBottomBarDelegate {
    private long mNativeAssistantBottomBarDelegate;

    @CalledByNative
    private static AssistantBottomBarNativeDelegate create(long nativeAssistantBottomBarDelegate) {
        return new AssistantBottomBarNativeDelegate(nativeAssistantBottomBarDelegate);
    }

    private AssistantBottomBarNativeDelegate(long nativeAssistantBottomBarDelegate) {
        mNativeAssistantBottomBarDelegate = nativeAssistantBottomBarDelegate;
    }

    @Override
    public boolean onBackButtonPressed() {
        if (mNativeAssistantBottomBarDelegate != 0) {
            return AssistantBottomBarNativeDelegateJni.get().onBackButtonClicked(
                    mNativeAssistantBottomBarDelegate, AssistantBottomBarNativeDelegate.this);
        }
        return false;
    }

    @Override
    public void onBottomSheetClosedWithSwipe() {
        if (mNativeAssistantBottomBarDelegate != 0) {
            AssistantBottomBarNativeDelegateJni.get().onBottomSheetClosedWithSwipe(
                    mNativeAssistantBottomBarDelegate, AssistantBottomBarNativeDelegate.this);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantBottomBarDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        boolean onBackButtonClicked(
                long nativeAssistantBottomBarDelegate, AssistantBottomBarNativeDelegate caller);
        void onBottomSheetClosedWithSwipe(
                long nativeAssistantBottomBarDelegate, AssistantBottomBarNativeDelegate caller);
    }
}
