// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillEnableResurrectingPaymentsUsersTreatmentArm;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/** JNI bridge entry point to show the Payments Churned Users bottom sheet. */
@JNINamespace("autofill")
@NullMarked
public class AutofillPaymentsChurnedUsersBottomSheetBridge
        implements AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate {
    @JniType("::autofill::AutofillPaymentsChurnedUsersBottomSheetBridge")
    interface NativeAutofillPaymentsChurnedUsersBottomSheetBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeAutofillPaymentsChurnedUsersBottomSheetBridge> mNativeBridge;
    private final WindowAndroid mWindowAndroid;
    private @Nullable AutofillPaymentsChurnedUsersBottomSheetCoordinator mCoordinator;

    @CalledByNative
    public AutofillPaymentsChurnedUsersBottomSheetBridge(
            JniRawPtr<NativeAutofillPaymentsChurnedUsersBottomSheetBridge> nativeBridge,
            WindowAndroid windowAndroid) {
        mNativeBridge = nativeBridge;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    public void requestShowContent(
            @AutofillEnableResurrectingPaymentsUsersTreatmentArm int treatmentArm) {
        Context context = mWindowAndroid.getContext().get();
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(mWindowAndroid);
        if (context == null || bottomSheetController == null) {
            onUiNotShown();
            return;
        }

        if (mCoordinator != null) {
            mCoordinator.destroy();
        }

        mCoordinator =
                new AutofillPaymentsChurnedUsersBottomSheetCoordinator(
                        context, bottomSheetController, treatmentArm, this);
        mCoordinator.requestShowContent();
    }

    @Override
    public void onUiAccepted() {
        if (mNativeBridge == null) return;
        AutofillPaymentsChurnedUsersBottomSheetBridgeJni.get().onUiAccepted(mNativeBridge);
    }

    @Override
    public void onShowConfirmation() {
        if (mNativeBridge == null) return;
        AutofillPaymentsChurnedUsersBottomSheetBridgeJni.get().onShowConfirmation(mNativeBridge);
    }

    @Override
    public void onUiCanceled() {
        if (mNativeBridge == null) return;
        AutofillPaymentsChurnedUsersBottomSheetBridgeJni.get().onUiCanceled(mNativeBridge);
    }

    @Override
    public void onUiDismissed() {
        if (mNativeBridge == null) return;
        AutofillPaymentsChurnedUsersBottomSheetBridgeJni.get().onUiDismissed(mNativeBridge);
    }

    @Override
    public void onUiNotShown() {
        if (mNativeBridge == null) return;
        AutofillPaymentsChurnedUsersBottomSheetBridgeJni.get().onUiNotShown(mNativeBridge);
    }

    @CalledByNative
    public void destroy() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
        if (mNativeBridge != null) {
            mNativeBridge.release();
            mNativeBridge = null;
        }
    }

    @Nullable AutofillPaymentsChurnedUsersBottomSheetCoordinator getCoordinatorForTesting() {
        return mCoordinator;
    }

    @NativeMethods
    interface Natives {
        void onUiAccepted(JniPtr<NativeAutofillPaymentsChurnedUsersBottomSheetBridge> self);

        void onShowConfirmation(JniPtr<NativeAutofillPaymentsChurnedUsersBottomSheetBridge> self);

        void onUiCanceled(JniPtr<NativeAutofillPaymentsChurnedUsersBottomSheetBridge> self);

        void onUiDismissed(JniPtr<NativeAutofillPaymentsChurnedUsersBottomSheetBridge> self);

        void onUiNotShown(JniPtr<NativeAutofillPaymentsChurnedUsersBottomSheetBridge> self);
    }
}
