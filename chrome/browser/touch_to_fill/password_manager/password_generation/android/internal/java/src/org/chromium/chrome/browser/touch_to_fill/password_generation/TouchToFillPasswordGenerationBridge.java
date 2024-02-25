// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for C++ TouchToFillPasswordGenerationBridge. Delegates calls from native to Java. */
class TouchToFillPasswordGenerationBridge
        implements TouchToFillPasswordGenerationCoordinator.Delegate {
    private WindowAndroid mWindowAndroid;
    private TouchToFillPasswordGenerationCoordinator mCoordinator;
    private long mNativeTouchToFillPasswordGenerationBridge;

    @CalledByNative
    private static TouchToFillPasswordGenerationBridge create(
            WindowAndroid windowAndroid,
            WebContents webContents,
            PrefService prefService,
            long nativeTouchToFillPasswordGenerationBridge) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        return new TouchToFillPasswordGenerationBridge(
                nativeTouchToFillPasswordGenerationBridge,
                bottomSheetController,
                windowAndroid,
                webContents,
                prefService);
    }

    public TouchToFillPasswordGenerationBridge(
            long nativeTouchToFillPasswordGenerationBridge,
            BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid,
            WebContents webContents,
            PrefService prefService) {
        mNativeTouchToFillPasswordGenerationBridge = nativeTouchToFillPasswordGenerationBridge;
        mWindowAndroid = windowAndroid;
        mCoordinator =
                new TouchToFillPasswordGenerationCoordinator(
                        webContents,
                        prefService,
                        bottomSheetController,
                        KeyboardVisibilityDelegate.getInstance(),
                        this);
    }

    @CalledByNative
    public boolean show(String generatedPassword, String account) {
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return false;

        return mCoordinator.show(generatedPassword, account, context);
    }

    @CalledByNative
    public void hideFromNative() {
        mCoordinator.hideFromNative();
        mNativeTouchToFillPasswordGenerationBridge = 0;
    }

    @Override
    public void onDismissed(boolean passwordAccepted) {
        if (mNativeTouchToFillPasswordGenerationBridge == 0) return;

        TouchToFillPasswordGenerationBridgeJni.get()
                .onDismissed(mNativeTouchToFillPasswordGenerationBridge, passwordAccepted);
        mNativeTouchToFillPasswordGenerationBridge = 0;
    }

    @Override
    public void onGeneratedPasswordAccepted(String password) {
        if (mNativeTouchToFillPasswordGenerationBridge == 0) return;

        TouchToFillPasswordGenerationBridgeJni.get()
                .onGeneratedPasswordAccepted(mNativeTouchToFillPasswordGenerationBridge, password);
        // No need to reset mNativeTouchToFillPasswordGenerationBridge, onDismissed will do it
        // afterwards.
    }

    @Override
    public void onGeneratedPasswordRejected() {
        if (mNativeTouchToFillPasswordGenerationBridge == 0) return;

        TouchToFillPasswordGenerationBridgeJni.get()
                .onGeneratedPasswordRejected(mNativeTouchToFillPasswordGenerationBridge);
        // No need to reset mNativeTouchToFillPasswordGenerationBridge, onDismissed will do it
        // afterwards.
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeTouchToFillPasswordGenerationBridge, boolean passwordAccepted);

        void onGeneratedPasswordAccepted(
                long nativeTouchToFillPasswordGenerationBridge, String password);

        void onGeneratedPasswordRejected(long nativeTouchToFillPasswordGenerationBridge);
    }
}
