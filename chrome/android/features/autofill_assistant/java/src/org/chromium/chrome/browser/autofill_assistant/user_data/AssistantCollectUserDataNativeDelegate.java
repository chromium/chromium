// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantValue;

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
    public void onContactInfoChanged(
            @Nullable AssistantCollectUserDataModel.ContactModel contactModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onContactInfoChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    contactModel == null ? null : contactModel.mOption.getProfile(), eventType);
        }
    }

    @Override
    public void onShippingAddressChanged(
            @Nullable AssistantCollectUserDataModel.AddressModel addressModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onShippingAddressChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    addressModel == null ? null : addressModel.mOption.getProfile(), eventType);
        }
    }

    @Override
    public void onPaymentMethodChanged(
            @Nullable AssistantCollectUserDataModel.PaymentInstrumentModel paymentInstrumentModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onCreditCardChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    paymentInstrumentModel == null ? null
                                                   : paymentInstrumentModel.mOption.getCard(),
                    paymentInstrumentModel == null
                            ? null
                            : paymentInstrumentModel.mOption.getBillingProfile(),
                    eventType);
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
    public void onTextLinkClicked(int link) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onTextLinkClicked(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, link);
        }
    }

    @Override
    public void onLoginChoiceChanged(
            @Nullable AssistantCollectUserDataModel.LoginChoiceModel loginChoiceModel,
            @AssistantUserDataEventType int eventType) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onLoginChoiceChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this,
                    loginChoiceModel == null ? null : loginChoiceModel.mOption.getIdentifier(),
                    eventType);
        }
    }

    @Override
    public void onDateTimeRangeStartDateChanged(@Nullable AssistantDateTime date) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            if (date != null) {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeStartDateChanged(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this, date.getYear(),
                        date.getMonth(), date.getDay());
            } else {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeStartDateCleared(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this);
            }
        }
    }

    @Override
    public void onDateTimeRangeStartTimeSlotChanged(@Nullable Integer index) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            if (index != null) {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeStartTimeSlotChanged(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this, (int) index);
            } else {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeStartTimeSlotCleared(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this);
            }
        }
    }

    @Override
    public void onDateTimeRangeEndDateChanged(@Nullable AssistantDateTime date) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            if (date != null) {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeEndDateChanged(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this, date.getYear(),
                        date.getMonth(), date.getDay());
            } else {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeEndDateCleared(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this);
            }
        }
    }

    @Override
    public void onDateTimeRangeEndTimeSlotChanged(@Nullable Integer index) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            if (index != null) {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeEndTimeSlotChanged(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this, (int) index);
            } else {
                AssistantCollectUserDataNativeDelegateJni.get().onDateTimeRangeEndTimeSlotCleared(
                        mNativeAssistantCollectUserDataDelegate,
                        AssistantCollectUserDataNativeDelegate.this);
            }
        }
    }

    @Override
    public void onKeyValueChanged(String key, AssistantValue value) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onKeyValueChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, key, value);
        }
    }

    @Override
    public void onInputTextFocusChanged(boolean isFocused) {
        if (mNativeAssistantCollectUserDataDelegate != 0) {
            AssistantCollectUserDataNativeDelegateJni.get().onInputTextFocusChanged(
                    mNativeAssistantCollectUserDataDelegate,
                    AssistantCollectUserDataNativeDelegate.this, isFocused);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantCollectUserDataDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onContactInfoChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable PersonalDataManager.AutofillProfile contactProfile, int eventType);
        void onShippingAddressChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable PersonalDataManager.AutofillProfile address, int eventType);
        void onCreditCardChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller,
                @Nullable PersonalDataManager.CreditCard card,
                @Nullable PersonalDataManager.AutofillProfile billingProfile, int eventType);
        void onTermsAndConditionsChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int state);
        void onTextLinkClicked(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int link);
        void onLoginChoiceChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, String choice, int eventType);
        void onDateTimeRangeStartDateChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int year, int month, int day);
        void onDateTimeRangeStartTimeSlotChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int index);
        void onDateTimeRangeEndDateChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int year, int month, int day);
        void onDateTimeRangeEndTimeSlotChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, int index);
        void onDateTimeRangeStartDateCleared(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller);
        void onDateTimeRangeStartTimeSlotCleared(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller);
        void onDateTimeRangeEndDateCleared(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller);
        void onDateTimeRangeEndTimeSlotCleared(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller);
        void onKeyValueChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, String key, AssistantValue value);
        void onInputTextFocusChanged(long nativeAssistantCollectUserDataDelegate,
                AssistantCollectUserDataNativeDelegate caller, boolean isFocused);
    }
}
