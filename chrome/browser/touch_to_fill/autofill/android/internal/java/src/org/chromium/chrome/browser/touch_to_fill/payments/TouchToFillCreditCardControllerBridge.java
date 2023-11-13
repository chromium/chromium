// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * JNI wrapper for C++ TouchToFillCreditCardViewController. Delegates calls from Java to native.
 */
@JNINamespace("autofill")
class TouchToFillCreditCardControllerBridge implements TouchToFillCreditCardComponent.Delegate {
    private long mNativeTouchToFillCreditCardViewController;

    private TouchToFillCreditCardControllerBridge(long nativeTouchToFillCreditCardViewController) {
        mNativeTouchToFillCreditCardViewController = nativeTouchToFillCreditCardViewController;
    }

    @CalledByNative
    private static TouchToFillCreditCardControllerBridge create(
            long nativeTouchToFillCreditCardViewController) {
        return new TouchToFillCreditCardControllerBridge(nativeTouchToFillCreditCardViewController);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeTouchToFillCreditCardViewController = 0;
    }

    @Override
    public void onDismissed(boolean dismissedByUser) {
        if (mNativeTouchToFillCreditCardViewController != 0) {
            TouchToFillCreditCardControllerBridgeJni.get().onDismissed(
                    mNativeTouchToFillCreditCardViewController, dismissedByUser);
        }
    }

    @Override
    public void scanCreditCard() {
        if (mNativeTouchToFillCreditCardViewController != 0) {
            TouchToFillCreditCardControllerBridgeJni.get().scanCreditCard(
                    mNativeTouchToFillCreditCardViewController);
        }
    }

    @Override
    public void showCreditCardSettings() {
        if (mNativeTouchToFillCreditCardViewController != 0) {
            TouchToFillCreditCardControllerBridgeJni.get().showCreditCardSettings(
                    mNativeTouchToFillCreditCardViewController);
        }
    }

    @Override
    public void suggestionSelected(String uniqueId, boolean isVirtual) {
        if (mNativeTouchToFillCreditCardViewController != 0) {
            TouchToFillCreditCardControllerBridgeJni.get().suggestionSelected(
                    mNativeTouchToFillCreditCardViewController, uniqueId, isVirtual);
        }
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeTouchToFillCreditCardViewController, boolean dismissedByUser);
        void scanCreditCard(long nativeTouchToFillCreditCardViewController);
        void showCreditCardSettings(long nativeTouchToFillCreditCardViewController);
        void suggestionSelected(
                long nativeTouchToFillCreditCardViewController, String uniqueId, boolean isVirtual);
    }
}
