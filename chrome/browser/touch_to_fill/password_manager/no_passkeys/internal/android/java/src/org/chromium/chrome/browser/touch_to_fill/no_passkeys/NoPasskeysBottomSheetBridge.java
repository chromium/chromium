// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** JNI wrapper for C++ NoPasskeysBottomSheetBridge. Delegates calls from native to Java. */
class NoPasskeysBottomSheetBridge implements NoPasskeysBottomSheetCoordinator.NativeDelegate {
    private final NoPasskeysBottomSheetCoordinator mNoPasskeysSheet;
    private long mNativeBridge;

    @CalledByNative
    NoPasskeysBottomSheetBridge(long nativeNoPasskeysBottomSheetBridge, WindowAndroid window) {
        this(
                nativeNoPasskeysBottomSheetBridge,
                window.getContext(),
                new WeakReference<>(BottomSheetControllerProvider.from(window)));
    }

    @VisibleForTesting
    NoPasskeysBottomSheetBridge(
            long nativeNoPasskeysBottomSheetBridge,
            WeakReference<Context> context,
            WeakReference<BottomSheetController> bottomSheetController) {
        mNativeBridge = nativeNoPasskeysBottomSheetBridge;
        mNoPasskeysSheet =
                new NoPasskeysBottomSheetCoordinator(context, bottomSheetController, this);
    }

    @CalledByNative
    void show(String origin) {
        mNoPasskeysSheet.show(origin);
    }

    @CalledByNative
    void dismiss() {
        mNoPasskeysSheet.destroy();
        mNativeBridge = 0;
    }

    @Override
    public void onClickUseAnotherDevice() {
        if (mNativeBridge == 0) return;

        NoPasskeysBottomSheetBridgeJni.get().onClickUseAnotherDevice(mNativeBridge);
    }

    @Override
    public void onDismissed() {
        if (mNativeBridge == 0) return;

        NoPasskeysBottomSheetBridgeJni.get().onDismissed(mNativeBridge);
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeNoPasskeysBottomSheetBridge);

        void onClickUseAnotherDevice(long nativeNoPasskeysBottomSheetBridge);
    }
}
