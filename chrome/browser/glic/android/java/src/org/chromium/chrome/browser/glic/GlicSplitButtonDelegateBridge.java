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

/** JNI Bridge to dispatch native C++ GlicSplitButtonDelegate requests and UI events. */
@JNINamespace("glic")
@NullMarked
public class GlicSplitButtonDelegateBridge implements ChromeAndroidTaskFeature {
    private final GlicSplitButtonDelegate mDelegate;
    private long mNativePtr;

    public GlicSplitButtonDelegateBridge(GlicSplitButtonDelegate delegate) {
        mDelegate = delegate;
    }

    public void destroy() {
        if (mNativePtr != 0) {
            GlicSplitButtonDelegateBridgeJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        long nativeBrowserWindowPtr = initInfo.nativeBrowserWindowPtr;
        if (nativeBrowserWindowPtr == 0) return;
        mNativePtr = GlicSplitButtonDelegateBridgeJni.get().create(nativeBrowserWindowPtr, this);
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

    @CalledByNative
    public void setGlicShowState(boolean show) {
        mDelegate.setGlicShowState(show);
    }

    @CalledByNative
    public void setGlicPanelIsOpen(boolean open) {
        mDelegate.setGlicPanelIsOpen(open);
    }

    @CalledByNative
    public void showGlicActorTaskIcon() {
        mDelegate.showGlicActorTaskIcon();
    }

    @CalledByNative
    public void hideGlicActorTaskIcon() {
        mDelegate.hideGlicActorTaskIcon();
    }

    @CalledByNative
    public boolean getIsShowingGlicActorTaskIconNudge() {
        return mDelegate.getIsShowingGlicActorTaskIconNudge();
    }

    @CalledByNative
    public void setGlicActorNudgeLabel(String nudgeLabel) {
        mDelegate.setGlicActorNudgeLabel(nudgeLabel);
    }

    @CalledByNative
    public void triggerGlicActorNudge(String nudgeText) {
        mDelegate.triggerGlicActorNudge(nudgeText);
    }

    @CalledByNative
    public void setGlicActorNudgePressedState(boolean pressed) {
        mDelegate.setGlicActorNudgePressedState(pressed);
    }

    @CalledByNative
    public void showActorTaskListBubble() {
        mDelegate.showActorTaskListBubble();
    }

    @CalledByNative
    public void closeActorTaskListBubble() {
        mDelegate.closeActorTaskListBubble();
    }

    @CalledByNative
    public boolean isActorTaskListBubbleShowing() {
        return mDelegate.isActorTaskListBubbleShowing();
    }

    /** Notifies native side of user nudge activity. */
    public void onNudgeActivity(@GlicNudgeActivity int event) {
        if (mNativePtr != 0) {
            GlicSplitButtonDelegateBridgeJni.get().onNudgeActivity(mNativePtr, event);
        }
    }

    /** Notifies native side that an actor task row was clicked. */
    public void onTaskRowClicked(int taskId) {
        if (mNativePtr != 0) {
            GlicSplitButtonDelegateBridgeJni.get().onTaskRowClicked(mNativePtr, taskId);
        }
    }

    /** Notifies native side that the Glic actor button was clicked. */
    public void onGlicActorButtonClicked() {
        if (mNativePtr != 0) {
            GlicSplitButtonDelegateBridgeJni.get().onGlicActorButtonClicked(mNativePtr);
        }
    }

    @NativeMethods
    public interface Natives {
        long create(long browserWindowInterfacePtr, GlicSplitButtonDelegateBridge delegate);

        void destroy(long nativeGlicSplitButtonDelegateAndroid);

        void onNudgeActivity(
                long nativeGlicSplitButtonDelegateAndroid, @JniType("GlicNudgeActivity") int event);

        void onTaskRowClicked(long nativeGlicSplitButtonDelegateAndroid, int taskId);

        void onGlicActorButtonClicked(long nativeGlicSplitButtonDelegateAndroid);
    }
}
