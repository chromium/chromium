// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.settings.SettingsLauncherFactory;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

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

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeFacilitatedPaymentsController = 0;
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

    @Override
    public boolean showFinancialAccountsManagementSettings(Context context) {
        if (context == null) {
            return false;
        }
        SettingsLauncherFactory.createSettingsLauncher()
                .launchSettingsActivity(
                        context, SettingsLauncher.SettingsFragment.FINANCIAL_ACCOUNTS);
        return true;
    }

    @Override
    public boolean showManagePaymentMethodsSettings(Context context) {
        if (context == null) {
            return false;
        }
        SettingsLauncherFactory.createSettingsLauncher()
                .launchSettingsActivity(context, SettingsLauncher.SettingsFragment.PAYMENT_METHODS);
        return true;
    }

    @NativeMethods
    interface Natives {
        void onDismissed(long nativeFacilitatedPaymentsController);

        void onBankAccountSelected(long nativeFacilitatedPaymentsController, long instrumentId);
    }
}
