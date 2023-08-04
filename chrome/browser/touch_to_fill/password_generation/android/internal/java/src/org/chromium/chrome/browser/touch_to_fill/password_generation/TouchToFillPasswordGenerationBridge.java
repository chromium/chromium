// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI wrapper for C++ TouchToFillPasswordGenerationBridge. Delegates calls from native to Java.
 */
class TouchToFillPasswordGenerationBridge
        implements TouchToFillPasswordGenerationCoordinator.Delegate {
    private TouchToFillPasswordGenerationCoordinator mCoordinator;
    private long mNativeTouchToFillPasswordGenerationBridge;

    @CalledByNative
    private static TouchToFillPasswordGenerationBridge create(
            WindowAndroid windowAndroid, long nativeTouchToFillPasswordGenerationBridge) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        Context context = windowAndroid.getContext().get();
        return new TouchToFillPasswordGenerationBridge(
                nativeTouchToFillPasswordGenerationBridge, bottomSheetController, context);
    }

    public TouchToFillPasswordGenerationBridge(long nativeTouchToFillPasswordGenerationBridge,
            BottomSheetController bottomSheetController, Context context) {
        mNativeTouchToFillPasswordGenerationBridge = nativeTouchToFillPasswordGenerationBridge;
        mCoordinator =
                new TouchToFillPasswordGenerationCoordinator(bottomSheetController, context, this);
    }

    @CalledByNative
    public boolean show(String generatedPassword, String account) {
        return mCoordinator.show(generatedPassword, account);
    }

    @CalledByNative
    public void hide() {
        mCoordinator.hide();
    }

    @Override
    public void onDismissed() {
        if (mNativeTouchToFillPasswordGenerationBridge == 0) return;

        TouchToFillPasswordGenerationBridgeJni.get().onDismissed(
                mNativeTouchToFillPasswordGenerationBridge);
    }

    @Override
    public void onGeneratedPasswordAccepted(String password) {
        if (mNativeTouchToFillPasswordGenerationBridge == 0) return;

        TouchToFillPasswordGenerationBridgeJni.get().onGeneratedPasswordAccepted(
                mNativeTouchToFillPasswordGenerationBridge, password);
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeTouchToFillPasswordGenerationBridge);
        void onGeneratedPasswordAccepted(
                long nativeTouchToFillPasswordGenerationBridge, String password);
    }
}
