// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** JNI wrapper for C++ FacilitatedPaymentsController. */
@JNINamespace("payments::facilitated")
class FacilitatedPaymentsPaymentMethodsControllerBridge
        implements FacilitatedPaymentsPaymentMethodsComponent.Delegate {
    private long mNativeFacilitatedPaymentsController;

    private FacilitatedPaymentsPaymentMethodsControllerBridge(
            long nativeFacilitatedPaymentsController) {
        mNativeFacilitatedPaymentsController = nativeFacilitatedPaymentsController;
    }

    @CalledByNative
    static FacilitatedPaymentsPaymentMethodsControllerBridge create(
            long nativeFacilitatedPaymentsController) {
        return new FacilitatedPaymentsPaymentMethodsControllerBridge(
                nativeFacilitatedPaymentsController);
    }

    // FacilitatedPaymentsPaymentMethodsComponent.Delegate
    @Override
    public void onDismissed() {
        if (mNativeFacilitatedPaymentsController != 0) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onDismissed(mNativeFacilitatedPaymentsController);
        }
    }

    @Override
    public void onBankAccountSelected(long instrumentId) {
        if (mNativeFacilitatedPaymentsController != 0) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onBankAccountSelected(mNativeFacilitatedPaymentsController, instrumentId);
        }
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeFacilitatedPaymentsController);

        void onBankAccountSelected(long nativeFacilitatedPaymentsController, long instrumentId);
    }
}
