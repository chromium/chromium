// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.facilitated_payments.core.metrics.AccountLinkingPromptUserAction;
import org.chromium.components.facilitated_payments.core.metrics.FacilitatedPaymentsType;
import org.chromium.components.facilitated_payments.core.ui_utils.UiEvent;

/** JNI wrapper for C++ FacilitatedPaymentsController. */
@JNINamespace("payments::facilitated")
@NullMarked
class FacilitatedPaymentsPaymentMethodsControllerBridge
        implements FacilitatedPaymentsPaymentMethodsComponent.Delegate {
    @JniType("::FacilitatedPaymentsController")
    interface NativeFacilitatedPaymentsController extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeFacilitatedPaymentsController>
            mNativeFacilitatedPaymentsController;

    private FacilitatedPaymentsPaymentMethodsControllerBridge(
            JniRawPtr<NativeFacilitatedPaymentsController> nativeFacilitatedPaymentsController) {
        mNativeFacilitatedPaymentsController = nativeFacilitatedPaymentsController;
    }

    @CalledByNative
    static FacilitatedPaymentsPaymentMethodsControllerBridge create(
            JniRawPtr<NativeFacilitatedPaymentsController> nativeFacilitatedPaymentsController) {
        return new FacilitatedPaymentsPaymentMethodsControllerBridge(
                nativeFacilitatedPaymentsController);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        if (mNativeFacilitatedPaymentsController != null) {
            mNativeFacilitatedPaymentsController.release();
            mNativeFacilitatedPaymentsController = null;
        }
    }

    // FacilitatedPaymentsPaymentMethodsComponent.Delegate
    @Override
    public void onUiEvent(@UiEvent int uiEvent) {
        if (mNativeFacilitatedPaymentsController != null) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onUiEvent(mNativeFacilitatedPaymentsController, uiEvent);
        }
    }

    @Override
    public void onBankAccountSelected(long instrumentId) {
        if (mNativeFacilitatedPaymentsController != null) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onBankAccountSelected(mNativeFacilitatedPaymentsController, instrumentId);
        }
    }

    @Override
    public void onEwalletSelected(long instrumentId) {
        if (mNativeFacilitatedPaymentsController != null) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onEwalletSelected(mNativeFacilitatedPaymentsController, instrumentId);
        }
    }

    @Override
    public void onPaymentAppSelected(String packageName, String activityName) {
        if (mNativeFacilitatedPaymentsController != null) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onPaymentAppSelected(
                            mNativeFacilitatedPaymentsController, packageName, activityName);
        }
    }

    @Override
    public void onAccountLinkingPromptShown(@FacilitatedPaymentsType int type) {
        if (mNativeFacilitatedPaymentsController != null) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onAccountLinkingPromptShown(mNativeFacilitatedPaymentsController, type);
        }
    }

    @Override
    public void onAccountLinkingPromptAction(
            @FacilitatedPaymentsType int type, @AccountLinkingPromptUserAction int action) {
        if (mNativeFacilitatedPaymentsController != null) {
            FacilitatedPaymentsPaymentMethodsControllerBridgeJni.get()
                    .onAccountLinkingPromptAction(
                            mNativeFacilitatedPaymentsController, type, action);
        }
    }

    @NativeMethods
    interface Natives {
        void onUiEvent(JniPtr<NativeFacilitatedPaymentsController> self, @UiEvent int uiEvent);

        void onBankAccountSelected(
                JniPtr<NativeFacilitatedPaymentsController> self, long instrumentId);

        void onEwalletSelected(JniPtr<NativeFacilitatedPaymentsController> self, long instrumentId);

        void onAccountLinkingPromptShown(
                JniPtr<NativeFacilitatedPaymentsController> self,
                @FacilitatedPaymentsType int type);

        void onAccountLinkingPromptAction(
                JniPtr<NativeFacilitatedPaymentsController> self,
                @FacilitatedPaymentsType int type,
                @AccountLinkingPromptUserAction int action);

        void onPaymentAppSelected(
                JniPtr<NativeFacilitatedPaymentsController> self,
                @JniType("std::string") String packageName,
                @JniType("std::string") String activityName);
    }
}
