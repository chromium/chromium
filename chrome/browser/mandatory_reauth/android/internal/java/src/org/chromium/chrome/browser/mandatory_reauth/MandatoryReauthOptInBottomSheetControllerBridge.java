// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.autofill.PaymentsBubbleClosedReason;

/** JNI wrapper for C++ MandatoryReauthBubbleController. Delegates calls from Java to native. */
@JNINamespace("autofill")
class MandatoryReauthOptInBottomSheetControllerBridge
        implements MandatoryReauthOptInBottomSheetComponent.Delegate {
    private long mNativeMandatoryReauthBubbleControllerImpl;

    MandatoryReauthOptInBottomSheetControllerBridge(
            long nativeMandatoryReauthBubbleControllerImpl) {
        mNativeMandatoryReauthBubbleControllerImpl = nativeMandatoryReauthBubbleControllerImpl;
    }

    @CalledByNative
    private static MandatoryReauthOptInBottomSheetControllerBridge create(
            long nativeMandatoryReauthBubbleControllerImpl) {
        return new MandatoryReauthOptInBottomSheetControllerBridge(
                nativeMandatoryReauthBubbleControllerImpl);
    }

    @Override
    public void onClosed(@PaymentsBubbleClosedReason int closedReason) {
        if (mNativeMandatoryReauthBubbleControllerImpl != 0) {
            MandatoryReauthOptInBottomSheetControllerBridgeJni.get()
                    .onClosed(mNativeMandatoryReauthBubbleControllerImpl, closedReason);
        }
    }

    /** Marks the current instance as being freed. */
    @CalledByNative
    private void destroy() {
        mNativeMandatoryReauthBubbleControllerImpl = 0;
    }

    @NativeMethods
    interface Natives {
        void onClosed(
                long nativeMandatoryReauthBubbleControllerImpl,
                @PaymentsBubbleClosedReason int closedReason);
    }
}
