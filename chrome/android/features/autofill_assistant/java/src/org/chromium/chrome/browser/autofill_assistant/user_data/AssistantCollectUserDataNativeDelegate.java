// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;

/** Delegate for the Collect user data UI which forwards events to a native counterpart. */
@JNINamespace("autofill_assistant")
public class AssistantCollectUserDataNativeDelegate implements AssistantCollectUserDataDelegate {
    private long mNativeAssistantCollectUserDataDelegate;

    @CalledByNative
    private static AssistantCollectUserDataNativeDelegate create(
            long nativeAssistantCollectUserDataDelegate) {
        return new AssistantCollectUserDataNativeDelegate(nativeAssistantCollectUserDataDelegate);
    }

    private AssistantCollectUserDataNativeDelegate(long nativeAssistantCollectUserDataDelegate) {
        mNativeAssistantCollectUserDataDelegate = nativeAssistantCollectUserDataDelegate;
    }

    @Override
    public void onContactInfoChanged(@Nullable AutofillContact contact) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            String name = null;
            String phone = null;
            String email = null;

            if (contact != null) {
                name = contact.getPayerName();
                phone = contact.getPayerPhone();
                email = contact.getPayerEmail();
            }

            AssistantCollectUserDataNativeDelegateJni.get().onContactInfoChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, name, phone, email);
        }
    }

    @Override
    public void onShippingAddressChanged(@Nullable AutofillAddress address) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onShippingAddressChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    address != null ? address.getProfile() : null);
        }
    }

    @Override
    public void onPaymentMethodChanged(@Nullable AutofillPaymentInstrument paymentInstrument) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onCreditCardChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    paymentInstrument != null ? paymentInstrument.getCard() : null,
                    paymentInstrument != null ? paymentInstrument.getBillingProfile() : null);
        }
    }

    @Override
    public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onTermsAndConditionsChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, state);
        }
    }

    @Override
    public void onTermsAndConditionsLinkClicked(int link) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onTermsAndConditionsLinkClicked(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, link);
        }
    }

    @Override
    public void onLoginChoiceChanged(AssistantLoginChoice loginChoice) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onLoginChoiceChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    loginChoice != null ? loginChoice.getIdentifier() : null);
        }
    }

    @Override
    public void onDateTimeRangeStartChanged(
            int year, int month, int day, int hour, int minute, int second) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeStartChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, year, month, day, hour, minute,
                    second);
        }
    }

    @Override
    public void onDateTimeRangeEndChanged(
            int year, int month, int day, int hour, int minute, int second) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeEndChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, year, month, day, hour, minute,
                    second);
        }
    }

    @Override
    public void onKeyValueChanged(String key, String value) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onKeyValueChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, key, value);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantCollectUserDataDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onContactInfoChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, @Nullable String payerName,
                @Nullable String payerPhone, @Nullable String payerEmail);
        void onShippingAddressChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable PersonalDataManager.AutofillProfile address);
        void onCreditCardChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable PersonalDataManager.CreditCard card,
                @Nullable PersonalDataManager.AutofillProfile billingProfile);
        void onTermsAndConditionsChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int state);
        void onTermsAndConditionsLinkClicked(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int link);
        void onLoginChoiceChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, String choice);
        void onDateTimeRangeStartChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int year, int month, int day,
                int hour, int minute, int second);
        void onDateTimeRangeEndChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int year, int month, int day,
                int hour, int minute, int second);
        void onKeyValueChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, String key, String value);
    }
}
