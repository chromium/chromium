// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

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
    public void onDismissed() {
        if (mNativeTouchToFillCreditCardViewController != 0) {
            TouchToFillCreditCardControllerBridgeJni.get().onDismissed(
                    mNativeTouchToFillCreditCardViewController);
        }
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeTouchToFillCreditCardViewController);
    }
}
