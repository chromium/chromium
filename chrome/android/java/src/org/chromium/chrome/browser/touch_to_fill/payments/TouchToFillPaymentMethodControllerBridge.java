// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

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
import org.chromium.chrome.browser.autofill.AutofillFallbackSurfaceLauncher;
import org.chromium.chrome.browser.autofill.GoogleWalletLauncher;
import org.chromium.components.autofill.LoyaltyCard;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * JNI wrapper for C++ TouchToFillPaymentMethodViewController. Delegates calls from Java to native.
 */
@JNINamespace("autofill")
@NullMarked
class TouchToFillPaymentMethodControllerBridge
        implements TouchToFillPaymentMethodComponent.Delegate {
    @JniType("::autofill::TouchToFillPaymentMethodViewController")
    interface NativeTouchToFillPaymentMethodViewController extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeTouchToFillPaymentMethodViewController>
            mNativeTouchToFillPaymentMethodViewController;
    private final WeakReference<Context> mContext;

    private TouchToFillPaymentMethodControllerBridge(
            JniRawPtr<NativeTouchToFillPaymentMethodViewController>
                    nativeTouchToFillPaymentMethodViewController,
            WeakReference<Context> context) {
        mNativeTouchToFillPaymentMethodViewController =
                nativeTouchToFillPaymentMethodViewController;
        mContext = context;
    }

    @CalledByNative
    static @Nullable TouchToFillPaymentMethodControllerBridge create(
            JniRawPtr<NativeTouchToFillPaymentMethodViewController>
                    nativeTouchToFillPaymentMethodViewController,
            WindowAndroid windowAndroid) {
        if (windowAndroid == null) {
            nativeTouchToFillPaymentMethodViewController.release();
            return null;
        }
        return new TouchToFillPaymentMethodControllerBridge(
                nativeTouchToFillPaymentMethodViewController, windowAndroid.getContext());
    }

    @CalledByNative
    void onNativeDestroyed() {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            mNativeTouchToFillPaymentMethodViewController.release();
            mNativeTouchToFillPaymentMethodViewController = null;
        }
    }

    @Override
    public void onDismissed(boolean dismissedByUser, boolean shouldReshow) {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .onDismissed(
                            mNativeTouchToFillPaymentMethodViewController,
                            dismissedByUser,
                            shouldReshow);
        }
    }

    @Override
    public void scanCreditCard() {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .scanCreditCard(mNativeTouchToFillPaymentMethodViewController);
        }
    }

    @Override
    public void showPaymentMethodSettings() {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .showPaymentMethodSettings(mNativeTouchToFillPaymentMethodViewController);
        }
    }

    @Override
    public void showGoogleWalletSettings() {
        if (mContext.get() != null) {
            GoogleWalletLauncher.openGoogleWalletPassesSettings(
                    mContext.get(), mContext.get().getPackageManager());
        }
    }

    @Override
    public void creditCardSuggestionSelected(String uniqueId, boolean isVirtual) {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .creditCardSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, uniqueId, isVirtual);
        }
    }

    @Override
    public void localIbanSuggestionSelected(String guid) {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .localIbanSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, guid);
        }
    }

    @Override
    public void serverIbanSuggestionSelected(long instrumentId) {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .serverIbanSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, instrumentId);
        }
    }

    @Override
    public void loyaltyCardSuggestionSelected(LoyaltyCard loyaltyCard) {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .loyaltyCardSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, loyaltyCard);
        }
    }

    @Override
    public void bnplSuggestionSelected(@Nullable Long extractedAmount) {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .bnplSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, extractedAmount);
        }
    }

    @Override
    public void openPassesManagementUi() {
        if (mContext.get() != null) {
            AutofillFallbackSurfaceLauncher.openGoogleWalletPassesPage(mContext.get());
        }
    }

    @Override
    public void onBnplIssuerSuggestionSelected(String issuerId) {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .onBnplIssuerSuggestionSelected(
                            mNativeTouchToFillPaymentMethodViewController, issuerId);
        }
    }

    @Override
    public void onBnplTosAccepted() {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .onBnplTosAccepted(mNativeTouchToFillPaymentMethodViewController);
        }
    }

    @Override
    public void onUserDecisionToUseSavedCards() {
        if (mNativeTouchToFillPaymentMethodViewController != null) {
            TouchToFillPaymentMethodControllerBridgeJni.get()
                    .onUserDecisionToUseSavedCards(mNativeTouchToFillPaymentMethodViewController);
        }
    }

    @NativeMethods
    interface Natives {
        void onDismissed(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self,
                boolean dismissedByUser,
                boolean shouldReshow);

        void scanCreditCard(JniPtr<NativeTouchToFillPaymentMethodViewController> self);

        void showPaymentMethodSettings(JniPtr<NativeTouchToFillPaymentMethodViewController> self);

        void creditCardSuggestionSelected(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self,
                @JniType("std::string") String uniqueId,
                boolean isVirtual);

        void bnplSuggestionSelected(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self,
                @JniType("std::optional<int64_t>") @Nullable Long extractedAmount);

        void localIbanSuggestionSelected(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self,
                @JniType("std::string") String guid);

        void serverIbanSuggestionSelected(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self, long instrumentId);

        void loyaltyCardSuggestionSelected(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self,
                @JniType("LoyaltyCard") LoyaltyCard loyaltyCardNumber);

        void onBnplIssuerSuggestionSelected(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self,
                @JniType("std::string") String issuerId);

        void onBnplTosAccepted(JniPtr<NativeTouchToFillPaymentMethodViewController> self);

        void onUserDecisionToUseSavedCards(
                JniPtr<NativeTouchToFillPaymentMethodViewController> self);
    }
}
