// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.NormalizedAddressRequestDelegate;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentAddressTypeConverter;
import org.chromium.components.payments.PaymentApp;
import org.chromium.payments.mojom.PayerDetail;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentResponse;

/**
 * The helper class to create and prepare a PaymentResponse.
 */
public class PaymentResponseHelper implements NormalizedAddressRequestDelegate {
    /**
     * Observer to be notified when the payment response is completed.
     */
    public interface PaymentResponseRequesterDelegate {
        /*
         * Called when the payment response is ready to be sent to the merchant.
         *
         * @param response The payment response to send to the merchant.
         */
        void onPaymentResponseReady(PaymentResponse response);
    }

    private AutofillAddress mSelectedShippingAddress;
    private final AutofillContact mSelectedContact;
    private PaymentResponse mPaymentResponse;
    private PaymentResponseRequesterDelegate mDelegate;
    private boolean mIsWaitingForShippingNormalization;
    private boolean mIsWaitingForPaymentsDetails = true;
    private final PaymentApp mSelectedPaymentApp;
    private final PaymentOptions mPaymentOptions;
    private final boolean mSkipToGpay;
    private PayerData mPayerDataFromPaymentApp;

    /**
     * Builds a helper to contruct and fill a PaymentResponse.
     *
     * @param selectedShippingAddress The shipping address picked by the user.
     * @param selectedShippingOption  The shipping option picked by the user.
     * @param selectedContact         The contact info picked by the user.
     * @param selectedPaymentApp      The payment app picked by the user.
     * @param paymentOptions          The paymentOptions of the corresponding payment request.
     * @param skipToGpay              Whether or not Gpay bridge is activated for skip to Gpay.
     * @param delegate                The object that will receive the completed PaymentResponse.
     */
    public PaymentResponseHelper(EditableOption selectedShippingAddress,
            EditableOption selectedShippingOption, EditableOption selectedContact,
            PaymentApp selectedPaymentApp, PaymentOptions paymentOptions, boolean skipToGpay,
            PaymentResponseRequesterDelegate delegate) {
        mPaymentResponse = new PaymentResponse();
        mPaymentResponse.payer = new PayerDetail();

        mDelegate = delegate;
        mSelectedPaymentApp = selectedPaymentApp;
        mPaymentOptions = paymentOptions;
        mSkipToGpay = skipToGpay;

        // Contacts are created in ChromePaymentRequestService.init(). These should all be instances
        // of AutofillContact.
        mSelectedContact = (AutofillContact) selectedContact;

        // Set up the shipping option section of the response when it comes from payment sheet
        // (Shipping option comes from payment app when the app can handle shipping, or from Gpay
        // when skipToGpay is true).
        if (mPaymentOptions.requestShipping && !mSelectedPaymentApp.handlesShippingAddress()
                && !mSkipToGpay) {
            assert selectedShippingOption != null;
            assert selectedShippingOption.getIdentifier() != null;
            mPaymentResponse.shippingOption = selectedShippingOption.getIdentifier();
        }

        // Set up the shipping address section of the response when it comes from payment sheet
        // (Shipping address comes from payment app when the app can handle shipping, or from Gpay
        // when skipToGpay is true).
        if (mPaymentOptions.requestShipping && !mSelectedPaymentApp.handlesShippingAddress()
                && !mSkipToGpay) {
            assert selectedShippingAddress != null;
            // Shipping addresses are created in ChromePaymentRequestService.init(). These should
            // all be instances of AutofillAddress.
            mSelectedShippingAddress = (AutofillAddress) selectedShippingAddress;

            // Addresses to be sent to the merchant should always be complete.
            assert mSelectedShippingAddress.isComplete();

            // Record the use of the profile.
            PersonalDataManager.getInstance().recordAndLogProfileUse(
                    mSelectedShippingAddress.getProfile().getGUID());

            mPaymentResponse.shippingAddress = mSelectedShippingAddress.toPaymentAddress();

            // The shipping address needs to be normalized before sending the response to the
            // merchant.
            mIsWaitingForShippingNormalization = true;
            PersonalDataManager.getInstance().normalizeAddress(
                    mSelectedShippingAddress.getProfile(), this);
        }
    }

    /**
     * Called after the payment details were received.
     *
     * @param methodName         The payment method name being used for payment.
     * @param stringifiedDetails A string containing all the details of the payment.
     * @param payerData          The payer data received from the payment app.
     */
    public void onPaymentDetailsReceived(
            String methodName, String stringifiedDetails, PayerData payerData) {
        mPaymentResponse.methodName = methodName;
        mPaymentResponse.stringifiedDetails = stringifiedDetails;
        mPayerDataFromPaymentApp = payerData;

        mIsWaitingForPaymentsDetails = false;

        // Wait for the shipping address normalization before sending the response.
        if (!mIsWaitingForShippingNormalization) generatePaymentResponse();
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
        if (!mIsWaitingForPaymentsDetails) generatePaymentResponse();
    }

    @Override
    public void onCouldNotNormalize(AutofillProfile profile) {
        onAddressNormalized(profile);
    }

    private void generatePaymentResponse() {
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
            } else if (!mSkipToGpay) {
                assert mSelectedContact != null;
                mPaymentResponse.payer.name = mSelectedContact.getPayerName();
            } else {
                // Gpay provides contact info when skip to Gpay is true.
            }
        }
        if (mPaymentOptions.requestPayerPhone) {
            if (mSelectedPaymentApp.handlesPayerPhone()) {
                mPaymentResponse.payer.phone = mPayerDataFromPaymentApp.payerPhone;
            } else if (!mSkipToGpay) {
                assert mSelectedContact != null;
                mPaymentResponse.payer.phone = mSelectedContact.getPayerPhone();
            } else {
                // Gpay provides contact info when skip to Gpay is true.
            }
        }
        if (mPaymentOptions.requestPayerEmail) {
            if (mSelectedPaymentApp.handlesPayerEmail()) {
                mPaymentResponse.payer.email = mPayerDataFromPaymentApp.payerEmail;
            } else if (!mSkipToGpay) {
                assert mSelectedContact != null;
                mPaymentResponse.payer.email = mSelectedContact.getPayerEmail();
            } else {
                // Gpay provides contact info when skip to Gpay is true.
            }
        }

        // Normalize the phone number only if it's not null since this calls native code.
        if (mPaymentResponse.payer.phone != null) {
            mPaymentResponse.payer.phone =
                    PhoneNumberUtil.formatForResponse(mPaymentResponse.payer.phone);
        }

        mDelegate.onPaymentResponseReady(mPaymentResponse);
    }
}
