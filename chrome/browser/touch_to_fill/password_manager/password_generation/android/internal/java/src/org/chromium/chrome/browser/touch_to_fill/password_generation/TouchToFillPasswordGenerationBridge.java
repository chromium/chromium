// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

/** JNI wrapper for C++ TouchToFillPasswordGenerationBridge. Delegates calls from native to Java. */
@NullMarked
class TouchToFillPasswordGenerationBridge
        implements TouchToFillPasswordGenerationCoordinator.Delegate {
    @JniType("::TouchToFillPasswordGenerationBridgeImpl")
    interface NativeTouchToFillPasswordGenerationBridgeImpl extends JniTypeToken {}

    private final WindowAndroid mWindowAndroid;
    private final TouchToFillPasswordGenerationCoordinator mCoordinator;
    private @Nullable JniRawPtr<NativeTouchToFillPasswordGenerationBridgeImpl>
            mNativeTouchToFillPasswordGenerationBridge;

    @CalledByNative
    private static TouchToFillPasswordGenerationBridge create(
            @JniType("ui::WindowAndroid*") WindowAndroid windowAndroid,
            @JniType("content::WebContents*") WebContents webContents,
            @JniType("PrefService*") PrefService prefService,
            JniRawPtr<NativeTouchToFillPasswordGenerationBridgeImpl>
                    nativeTouchToFillPasswordGenerationBridge) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        assert bottomSheetController != null : "BottomSheetController should not be null.";
        return new TouchToFillPasswordGenerationBridge(
                nativeTouchToFillPasswordGenerationBridge,
                bottomSheetController,
                windowAndroid,
                webContents,
                prefService);
    }

    public TouchToFillPasswordGenerationBridge(
            JniRawPtr<NativeTouchToFillPasswordGenerationBridgeImpl>
                    nativeTouchToFillPasswordGenerationBridge,
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
    public boolean show(
            @JniType("std::u16string") String generatedPassword,
            @JniType("std::string") String account) {
        Context context = mWindowAndroid.getContext().get();
        if (context == null) return false;

        return mCoordinator.show(generatedPassword, account, context);
    }

    @CalledByNative
    public void hideFromNative() {
        mCoordinator.hideFromNative();
        if (mNativeTouchToFillPasswordGenerationBridge != null) {
            mNativeTouchToFillPasswordGenerationBridge.release();
            mNativeTouchToFillPasswordGenerationBridge = null;
        }
    }

    @Override
    public void onDismissed(boolean passwordAccepted) {
        if (mNativeTouchToFillPasswordGenerationBridge == null) return;

        TouchToFillPasswordGenerationBridgeJni.get()
                .onDismissed(mNativeTouchToFillPasswordGenerationBridge, passwordAccepted);
        mNativeTouchToFillPasswordGenerationBridge.release();
        mNativeTouchToFillPasswordGenerationBridge = null;
    }

    @Override
    public void onGeneratedPasswordAccepted(String password) {
        if (mNativeTouchToFillPasswordGenerationBridge == null) return;

        TouchToFillPasswordGenerationBridgeJni.get()
                .onGeneratedPasswordAccepted(mNativeTouchToFillPasswordGenerationBridge, password);
        // No need to reset mNativeTouchToFillPasswordGenerationBridge, onDismissed will do it
        // afterwards.
    }

    @Override
    public void onGeneratedPasswordRejected() {
        if (mNativeTouchToFillPasswordGenerationBridge == null) return;

        TouchToFillPasswordGenerationBridgeJni.get()
                .onGeneratedPasswordRejected(mNativeTouchToFillPasswordGenerationBridge);
        // No need to reset mNativeTouchToFillPasswordGenerationBridge, onDismissed will do it
        // afterwards.
    }

    @NativeMethods
    interface Natives {
        void onDismissed(
                JniPtr<NativeTouchToFillPasswordGenerationBridgeImpl> self,
                boolean passwordAccepted);

        void onGeneratedPasswordAccepted(
                JniPtr<NativeTouchToFillPasswordGenerationBridgeImpl> self,
                @JniType("std::u16string") String password);

        void onGeneratedPasswordRejected(
                JniPtr<NativeTouchToFillPasswordGenerationBridgeImpl> self);
    }
}
