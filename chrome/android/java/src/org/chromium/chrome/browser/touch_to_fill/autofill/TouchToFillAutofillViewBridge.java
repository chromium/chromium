// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for C++ TouchToFillAutofillViewImpl. Delegates calls from native to Java. */
@JNINamespace("autofill")
@NullMarked
class TouchToFillAutofillViewBridge implements TouchToFillAutofillComponent.Delegate {
    private final TouchToFillAutofillComponent mComponent;
    private long mNativeViewImpl;

    private TouchToFillAutofillViewBridge(
            long nativeViewImpl,
            Context context,
            BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid) {
        mNativeViewImpl = nativeViewImpl;
        mComponent = new TouchToFillAutofillCoordinator();
        mComponent.initialize(
                context,
                bottomSheetController,
                this,
                new BottomSheetFocusHelper(bottomSheetController, windowAndroid));
    }

    @CalledByNative
    private static @Nullable TouchToFillAutofillViewBridge create(
            long nativeViewImpl, @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (context == null) return null;
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillAutofillViewBridge(
                nativeViewImpl, context, bottomSheetController, windowAndroid);
    }

    @CalledByNative
    private void show() {
        mComponent.show();
    }

    @CalledByNative
    private void hide() {
        mComponent.hide();
    }

    @Override
    public void onNoticeAcknowledged() {
        if (mNativeViewImpl != 0) {
            TouchToFillAutofillViewBridgeJni.get().onNoticeAcknowledged(mNativeViewImpl);
        }
    }

    @Override
    public void onSettingsLinkClicked() {
        if (mNativeViewImpl != 0) {
            TouchToFillAutofillViewBridgeJni.get().onSettingsLinkClicked(mNativeViewImpl);
        }
    }

    @Override
    public void onDismissed() {
        if (mNativeViewImpl != 0) {
            TouchToFillAutofillViewBridgeJni.get().onDismissed(mNativeViewImpl);
        }
    }

    @CalledByNative
    private void destroy() {
        mNativeViewImpl = 0;
        mComponent.destroy();
    }

    @NativeMethods
    interface Natives {
        void onNoticeAcknowledged(long nativeTouchToFillAutofillViewImpl);

        void onSettingsLinkClicked(long nativeTouchToFillAutofillViewImpl);

        void onDismissed(long nativeTouchToFillAutofillViewImpl);
    }
}
