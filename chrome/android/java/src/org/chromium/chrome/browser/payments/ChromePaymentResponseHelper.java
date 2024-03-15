// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.AddressNormalizerFactory;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.components.autofill.AddressNormalizer.NormalizedAddressRequestDelegate;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentAddressTypeConverter;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentResponseHelperInterface;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentResponse;

/** The helper class to create and prepare a PaymentResponse. */
public class ChromePaymentResponseHelper
        implements NormalizedAddressRequestDelegate, PaymentResponseHelperInterface {
    @Nullable private final AutofillContact mSelectedContact;
    private final PaymentApp mSelectedPaymentApp;
    private final PaymentOptions mPaymentOptions;
    private final PaymentResponse mPaymentResponse;
    private AutofillAddress mSelectedShippingAddress;
    private PaymentResponseResultCallback mResultCallback;
    private boolean mIsWaitingForShippingNormalization;
    private boolean mIsWaitingForPaymentsDetails = true;
    private PayerData mPayerDataFromPaymentApp;

    /**
     * Builds a helper to construct and fill a PaymentResponse.
     *
     * @param selectedShippingAddress The shipping address picked by the user.
     * @param selectedShippingOption The shipping option picked by the user.
     * @param selectedContact The contact info picked by the user, can be null.
     * @param selectedPaymentApp The payment app picked by the user.
     * @param paymentOptions The paymentOptions of the corresponding payment request.
     * @param personalDataManager The context appropriate PersonalDataManager reference.
     */
    public ChromePaymentResponseHelper(
            EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption,
            @Nullable AutofillContact selectedContact,
            PaymentApp selectedPaymentApp,
            PaymentOptions paymentOptions,
            PersonalDataManager personalDataManager) {
        mPaymentResponse = new PaymentResponse();
        mPaymentResponse.payer = new PayerDetail();

        mSelectedPaymentApp = selectedPaymentApp;
        mPaymentOptions = paymentOptions;

        mSelectedContact = selectedContact;

        // Set up the shipping option section of the response when it comes from payment sheet.
        // (Shipping option comes from payment app when the app can handle shipping.)
        if (mPaymentOptions.requestShipping && !mSelectedPaymentApp.handlesShippingAddress()) {
            assert selectedShippingOption != null;
            assert selectedShippingOption.getIdentifier() != null;
            mPaymentResponse.shippingOption = selectedShippingOption.getIdentifier();
        }

        // Set up the shipping address section of the response when it comes from payment sheet.
        // (Shipping address comes from payment app when the app can handle shipping.)
        if (mPaymentOptions.requestShipping && !mSelectedPaymentApp.handlesShippingAddress()) {
            assert selectedShippingAddress != null;
            // Shipping addresses are created in ChromePaymentRequestService.init(). These should
            // all be instances of AutofillAddress.
            mSelectedShippingAddress = (AutofillAddress) selectedShippingAddress;

            // Addresses to be sent to the merchant should always be complete.
            assert mSelectedShippingAddress.isComplete();

            // Record the use of the profile.
            personalDataManager.recordAndLogProfileUse(
                    mSelectedShippingAddress.getProfile().getGUID());

            mPaymentResponse.shippingAddress = mSelectedShippingAddress.toPaymentAddress();

            // The shipping address needs to be normalized before sending the response to the
            // merchant.
            mIsWaitingForShippingNormalization = true;
            AddressNormalizerFactory.getInstance()
                    .normalizeAddress(mSelectedShippingAddress.getProfile(), this);
        }
    }

    @Override
    public void generatePaymentResponse(
            String methodName,
            String stringifiedDetails,
            PayerData payerData,
            PaymentResponseResultCallback resultCallback) {
        mResultCallback = resultCallback;
        mPaymentResponse.methodName = methodName;
        mPaymentResponse.stringifiedDetails = stringifiedDetails;
        mPayerDataFromPaymentApp = payerData;

        mIsWaitingForPaymentsDetails = false;

        // Wait for the shipping address normalization before sending the response.
        if (!mIsWaitingForShippingNormalization) onAllDataReady();
    }

    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        // Check if a normalization is still required.
        if (!mIsWaitingForShippingNormalization) return;
        mIsWaitingForShippingNormalization = false;

        if (profile != null) {
            // The normalization finished first: use the normalized address.
            mSelectedShippingAddress.completeAddress(profile);
            mPaymentResponse.shippingAddress = mSelectedShippingAddress.toPaymentAddress();
        }

        // Wait for the payment details before sending the response.
        if (!mIsWaitingForPaymentsDetails) onAllDataReady();
    }

    @Override
    public void onCouldNotNormalize(AutofillProfile profile) {
        onAddressNormalized(profile);
    }

    private void onAllDataReady() {
        assert !mIsWaitingForPaymentsDetails;
        assert !mIsWaitingForShippingNormalization;

        // Set up the shipping section of the response when it comes from payment app.
        if (mPaymentOptions.requestShipping && mSelectedPaymentApp.handlesShippingAddress()) {
            mPaymentResponse.shippingAddress =
                    PaymentAddressTypeConverter.convertAddressToMojoPaymentAddress(
                            mPayerDataFromPaymentApp.shippingAddress);
            mPaymentResponse.shippingOption = mPayerDataFromPaymentApp.selectedShippingOptionId;
        }

        // Set up the contact section of the response.
        if (mPaymentOptions.requestPayerName) {
            if (mSelectedPaymentApp.handlesPayerName()) {
                mPaymentResponse.payer.name = mPayerDataFromPaymentApp.payerName;
            } else {
                assert mSelectedContact != null;
                mPaymentResponse.payer.name = mSelectedContact.getPayerName();
            }
        }
        if (mPaymentOptions.requestPayerPhone) {
            if (mSelectedPaymentApp.handlesPayerPhone()) {
                mPaymentResponse.payer.phone = mPayerDataFromPaymentApp.payerPhone;
            } else {
                assert mSelectedContact != null;
                mPaymentResponse.payer.phone = mSelectedContact.getPayerPhone();
            }
        }
        if (mPaymentOptions.requestPayerEmail) {
            if (mSelectedPaymentApp.handlesPayerEmail()) {
                mPaymentResponse.payer.email = mPayerDataFromPaymentApp.payerEmail;
            } else {
                assert mSelectedContact != null;
                mPaymentResponse.payer.email = mSelectedContact.getPayerEmail();
            }
        }

        // Normalize the phone number only if it's not null since this calls native code.
        if (mPaymentResponse.payer.phone != null) {
            mPaymentResponse.payer.phone =
                    PhoneNumberUtil.formatForResponse(mPaymentResponse.payer.phone);
        }

        mResultCallback.onPaymentResponseReady(mPaymentResponse);
    }
}
