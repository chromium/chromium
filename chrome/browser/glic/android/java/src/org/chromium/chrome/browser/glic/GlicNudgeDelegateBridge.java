// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;

/** JNI Bridge to dispatch native C++ Glic nudge requests to the active tab strip. */
@JNINamespace("glic")
@NullMarked
public class GlicNudgeDelegateBridge implements ChromeAndroidTaskFeature {
    private final GlicNudgeDelegate mDelegate;
    private long mNativePtr;

    public GlicNudgeDelegateBridge(GlicNudgeDelegate delegate) {
        mDelegate = delegate;
    }

    public void destroy() {
        if (mNativePtr != 0) {
            GlicNudgeDelegateBridgeJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        long nativeBrowserWindowPtr = initInfo.nativeBrowserWindowPtr;
        if (nativeBrowserWindowPtr == 0) return;
        mNativePtr = GlicNudgeDelegateBridgeJni.get().create(nativeBrowserWindowPtr, this);
    }

    @Override
    public void onFeatureRemoved() {
        destroy();
    }

    @CalledByNative
    public void onTriggerGlicNudgeUi(
            String label, String anchoredMessageText, String promptSuggestion) {
        mDelegate.onTriggerGlicNudgeUi(label, anchoredMessageText, promptSuggestion);
    }

    @CalledByNative
    public void onHideGlicNudgeUi() {
        mDelegate.onHideGlicNudgeUi();
    }

    @CalledByNative
    public boolean getIsShowingGlicNudge() {
        return mDelegate.getIsShowingGlicNudge();
    }

    /** Notifies native side of user nudge activity. */
    public void onNudgeActivity(@GlicNudgeActivity int event) {
        if (mNativePtr != 0) {
            GlicNudgeDelegateBridgeJni.get().onNudgeActivity(mNativePtr, event);
        }
    }

    @NativeMethods
    public interface Natives {
        long create(long browserWindowInterfacePtr, GlicNudgeDelegateBridge delegate);

        void destroy(long nativeGlicNudgeDelegateAndroid);

        void onNudgeActivity(
                long nativeGlicNudgeDelegateAndroid, @JniType("GlicNudgeActivity") int event);
    }
}
