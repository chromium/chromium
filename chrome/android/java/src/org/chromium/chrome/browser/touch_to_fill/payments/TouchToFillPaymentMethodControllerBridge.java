// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.AutofillFallbackSurfaceLauncher;
import org.chromium.chrome.browser.autofill.settings.SettingsNavigationHelper;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * JNI wrapper for C++ TouchToFillPaymentMethodViewController. Delegates calls from Java to native.
 */
@JNINamespace("autofill")
@NullMarked
class TouchToFillPaymentMethodControllerBridge
        implements TouchToFillPaymentMethodComponent.Delegate {
    private long mNativeTouchToFillPaymentMethodViewController;
    private final WeakReference<Context> mContext;

    private TouchToFillPaymentMethodControllerBridge(
            long nativeTouchToFillPaymentMethodViewController, WeakReference<Context> context) {
        mNativeTouchToFillPaymentMethodViewController =
                nativeTouchToFillPaymentMethodViewController;
        mContext = context;
    }

    @CalledByNative
    private static @Nullable TouchToFillPaymentMethodControllerBridge create(
            long nativeTouchToFillPaymentMethodViewController, WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return new TouchToFillPaymentMethodControllerBridge(
                nativeTouchToFillPaymentMethodViewController, windowAndroid.getContext());
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeTouchToFillPaymentMethodViewController = 0;
    }

    @Override
    public void onDismissed(boolean dismissedByUser) {
        if (mNativeTouchToFillPaymentMethodViewController != 0) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .onDismissed(mNativeTouchToFillPaymentMethodViewController, dismissedByUser);
        }
    }

    @Override
    public void scanCreditCard() {
        if (mNativeTouchToFillPaymentMethodViewController != 0) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .scanCreditCard(mNativeTouchToFillPaymentMethodViewController);
        }
    }

    @Override
    public void showPaymentMethodSettings() {
        if (mNativeTouchToFillPaymentMethodViewController != 0) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .showPaymentMethodSettings(mNativeTouchToFillPaymentMethodViewController);
        }
    }

    @Override
    public void showGoogleWalletSettings() {
        if (mContext.get() != null) {
            SettingsNavigationHelper.showGoogleWalletSettings(mContext.get());
        }
    }

    @Override
    public void creditCardSuggestionSelected(String uniqueId, boolean isVirtual) {
        if (mNativeTouchToFillPaymentMethodViewController != 0) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .creditCardSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, uniqueId, isVirtual);
        }
    }

    @Override
    public void localIbanSuggestionSelected(String guid) {
        if (mNativeTouchToFillPaymentMethodViewController != 0) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .localIbanSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, guid);
        }
    }

    @Override
    public void serverIbanSuggestionSelected(long instrumentId) {
        if (mNativeTouchToFillPaymentMethodViewController != 0) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .serverIbanSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, instrumentId);
        }
    }

    @Override
    public void loyaltyCardSuggestionSelected(String loyaltyCard) {
        if (mNativeTouchToFillPaymentMethodViewController != 0) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .loyaltyCardSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, loyaltyCard);
        }
    }

    @Override
    public void openPassesManagementUi() {
        if (mContext.get() != null) {
            AutofillFallbackSurfaceLauncher.openGoogleWalletPassesPage(mContext.get());
        }
    }

    @NativeMethods
    interface Natives {
        void onDismissed(
                long nativeTouchToFillPaymentMethodViewController, boolean dismissedByUser);

        void scanCreditCard(long nativeTouchToFillPaymentMethodViewController);

        void showPaymentMethodSettings(long nativeTouchToFillPaymentMethodViewController);

        void creditCardSuggestionSelected(
                long nativeTouchToFillPaymentMethodViewController,
                String uniqueId,
                boolean isVirtual);

        void localIbanSuggestionSelected(
                long nativeTouchToFillPaymentMethodViewController, String guid);

        void serverIbanSuggestionSelected(
                long nativeTouchToFillPaymentMethodViewController, long instrumentId);

        void loyaltyCardSuggestionSelected(
                long nativeTouchToFillPaymentMethodViewController,
                @JniType("std::string") String loyaltyCardNumber);
    }
}
