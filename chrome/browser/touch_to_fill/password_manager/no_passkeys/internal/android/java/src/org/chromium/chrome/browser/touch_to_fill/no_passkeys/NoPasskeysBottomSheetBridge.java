// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

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
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** JNI wrapper for C++ NoPasskeysBottomSheetBridge. Delegates calls from native to Java. */
@NullMarked
class NoPasskeysBottomSheetBridge implements NoPasskeysBottomSheetCoordinator.NativeDelegate {
    @JniType("::NoPasskeysBottomSheetBridge")
    interface NativeNoPasskeysBottomSheetBridge extends JniTypeToken {}

    private final NoPasskeysBottomSheetCoordinator mNoPasskeysSheet;
    private @Nullable JniRawPtr<NativeNoPasskeysBottomSheetBridge> mNativeBridge;

    @CalledByNative
    NoPasskeysBottomSheetBridge(
            JniRawPtr<NativeNoPasskeysBottomSheetBridge> nativeNoPasskeysBottomSheetBridge,
            @JniType("ui::WindowAndroid*") WindowAndroid window) {
        this(
                nativeNoPasskeysBottomSheetBridge,
                window.getContext(),
                new WeakReference<>(BottomSheetControllerProvider.from(window)));
    }

    @VisibleForTesting
    NoPasskeysBottomSheetBridge(
            JniRawPtr<NativeNoPasskeysBottomSheetBridge> nativeNoPasskeysBottomSheetBridge,
            WeakReference<Context> context,
            WeakReference<@Nullable BottomSheetController> bottomSheetController) {
        mNativeBridge = nativeNoPasskeysBottomSheetBridge;
        mNoPasskeysSheet =
                new NoPasskeysBottomSheetCoordinator(context, bottomSheetController, this);
    }

    @CalledByNative
    void show(@JniType("std::string") String origin) {
        mNoPasskeysSheet.show(origin);
    }

    @CalledByNative
    void dismiss() {
        mNoPasskeysSheet.destroy();
        if (mNativeBridge != null) {
            mNativeBridge.release();
            mNativeBridge = null;
        }
    }

    @Override
    public void onClickUseAnotherDevice() {
        if (mNativeBridge == null) return;

        NoPasskeysBottomSheetBridgeJni.get().onClickUseAnotherDevice(mNativeBridge);
    }

    @Override
    public void onDismissed() {
        if (mNativeBridge == null) return;

        NoPasskeysBottomSheetBridgeJni.get().onDismissed(mNativeBridge);
        mNativeBridge.release();
        mNativeBridge = null;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(JniPtr<NativeNoPasskeysBottomSheetBridge> self);

        void onClickUseAnotherDevice(JniPtr<NativeNoPasskeysBottomSheetBridge> self);
    }
}
