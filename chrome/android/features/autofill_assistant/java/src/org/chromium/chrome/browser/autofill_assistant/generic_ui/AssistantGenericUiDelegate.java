// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Delegate for the generic user interface. */
@JNINamespace("autofill_assistant")
public class AssistantGenericUiDelegate {
    private long mNativeAssistantGenericUiDelegate;

    @CalledByNative
    private static AssistantGenericUiDelegate create(long nativeDelegate) {
        return new AssistantGenericUiDelegate(nativeDelegate);
    }

    private AssistantGenericUiDelegate(long nativeAssistantGenericUiDelegate) {
        mNativeAssistantGenericUiDelegate = nativeAssistantGenericUiDelegate;
    }

    void onViewClicked(String identifier) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onViewClicked(
                mNativeAssistantGenericUiDelegate, AssistantGenericUiDelegate.this, identifier);
    }

    void onValueChanged(String modelIdentifier, AssistantValue value) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onValueChanged(mNativeAssistantGenericUiDelegate,
                AssistantGenericUiDelegate.this, modelIdentifier, value);
    }

    void onTextLinkClicked(int link) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onTextLinkClicked(
                mNativeAssistantGenericUiDelegate, AssistantGenericUiDelegate.this, link);
    }

    void onGenericPopupDismissed(String popupIdentifier) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onGenericPopupDismissed(
                mNativeAssistantGenericUiDelegate, AssistantGenericUiDelegate.this,
                popupIdentifier);
    }

    void onViewContainerCleared(String viewIdentifier) {
        assert mNativeAssistantGenericUiDelegate != 0;
        AssistantGenericUiDelegateJni.get().onViewContainerCleared(
                mNativeAssistantGenericUiDelegate, AssistantGenericUiDelegate.this, viewIdentifier);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantGenericUiDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onViewClicked(long nativeAssistantGenericUiDelegate, AssistantGenericUiDelegate caller,
                String identifier);
        void onValueChanged(long nativeAssistantGenericUiDelegate,
                AssistantGenericUiDelegate caller, String modelIdentifier, AssistantValue value);
        void onTextLinkClicked(
                long nativeAssistantGenericUiDelegate, AssistantGenericUiDelegate caller, int link);
        void onGenericPopupDismissed(long nativeAssistantGenericUiDelegate,
                AssistantGenericUiDelegate caller, String popupIdentifier);
        void onViewContainerCleared(long nativeAssistantGenericUiDelegate,
                AssistantGenericUiDelegate caller, String viewIdentifier);
    }
}
