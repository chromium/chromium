// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.widget.prefeditor.EditableOption;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodChangeResponse;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;

import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * The base class for a single payment instrument, e.g., a credit card.
 */
public abstract class PaymentInstrument extends EditableOption {
    /**
     * Whether complete and valid autofill data for merchant's request is available, e.g., if
     * merchant specifies `requestPayerEmail: true`, then this variable is true only if the autofill
     * data contains a valid email address. May be used in canMakePayment() for some types of
     * instruments, such as AutofillPaymentInstrument.
     */
    protected boolean mHaveRequestedAutofillData;

    /** Whether the instrument should be invoked for a microtransaction. */
    protected boolean mIsMicrotransaction;

    /**
     * The interface for the requester of instrument details.
     */
    public interface InstrumentDetailsCallback {
        /**
         * Called by the payment instrument to let Chrome know that the payment app's UI is
         * now hidden, but the payment instrument has not been returned yet. This is a good
         * time to show a "loading" progress indicator UI.
         */
        void onInstrumentDetailsLoadingWithoutUI();

        /**
         * Called after retrieving instrument details.
         *
         * @param methodName         Method name. For example, "visa".
         * @param stringifiedDetails JSON-serialized object. For example, {"card": "123"}.
         */
        void onInstrumentDetailsReady(String methodName, String stringifiedDetails);

        /**
         * Called if unable to retrieve instrument details.
         * @param errorMessage Developer-facing error message to be used when rejecting the promise
         *                     returned from PaymentRequest.show().
         */
        void onInstrumentDetailsError(String errorMessage);
    }

    /** The interface for the requester to abort payment. */
    public interface AbortCallback {
        /**
         * Called after aborting payment is finished.
         *
         * @param abortSucceeded Indicates whether abort is succeed.
         */
        void onInstrumentAbortResult(boolean abortSucceeded);
    }

    protected PaymentInstrument(String id, String label, String sublabel, Drawable icon) {
        super(id, label, sublabel, icon);
    }

    protected PaymentInstrument(
            String id, String label, String sublabel, String tertiarylabel, Drawable icon) {
        super(id, label, sublabel, tertiarylabel, icon);
    }

    /**
     * Sets the modified total for this payment instrument.
     *
     * @param modifiedTotal The new modified total to use.
     */
    public void setModifiedTotal(@Nullable String modifiedTotal) {
        updatePromoMessage(modifiedTotal);
    }

    /**
     * Returns a set of payment method names for this instrument, e.g., "visa" or
     * "mastercard" in basic card payments:
     * https://w3c.github.io/webpayments-methods-card/#method-id
     *
     * @return The method names for this instrument.
     */
    public abstract Set<String> getInstrumentMethodNames();

    /**
     * @return Whether this is an autofill instrument. All autofill instruments are sorted below all
     *         non-autofill instruments.
     */
    public boolean isAutofillInstrument() {
        return false;
    }

    /** @return Whether this is a server autofill instrument. */
    public boolean isServerAutofillInstrument() {
        return false;
    }

    /**
     * @return Whether this is a replacement for all server autofill instruments. If at least one of
     *         the displayed instruments returns true here, then all instruments that return true
     *         in isServerAutofillInstrument() should be hidden.
     */
    public boolean isServerAutofillInstrumentReplacement() {
        return false;
    }

    /**
     * @return Whether the instrument is exactly matching all filters provided by the merchant. For
     *         example, this can return false for unknown card types, if the merchant requested only
     *         debit cards.
     */
    public boolean isExactlyMatchingMerchantRequest() {
        return true;
    }

    /**
     * @return Whether the instrument supports the payment method with the method data. For example,
     *         supported card types and networks in the data should be verified for 'basic-card'
     *         payment method.
     */
    public boolean isValidForPaymentMethodData(String method, @Nullable PaymentMethodData data) {
        return getInstrumentMethodNames().contains(method);
    }

    /**
     * @return Whether the instrument can collect and return shipping address.
     */
    public boolean handlesShippingAddress() {
        return false;
    }

    /**
     * @return Whether the instrument can collect and return payer's name.
     */
    public boolean handlesPayerName() {
        return false;
    }

    /**
     * @return Whether the instrument can collect and return payer's email.
     */
    public boolean handlesPayerEmail() {
        return false;
    }

    /**
     * @return Whether the instrument can collect and return payer's phone.
     */
    public boolean handlesPayerPhone() {
        return false;
    }

    /** @return The country code (or null if none) associated with this payment instrument. */
    @Nullable
    public String getCountryCode() {
        return null;
    }

    /**
     * @param haveRequestedAutofillData Whether complete and valid autofill data for merchant's
     *                                  request is available.
     */
    /* package*/ void setHaveRequestedAutofillData(boolean haveRequestedAutofillData) {
        mHaveRequestedAutofillData = haveRequestedAutofillData;
    }

    /**
     * @return Whether presence of this payment instrument should cause the
     *         PaymentRequest.canMakePayment() to return true.
     */
    public boolean canMakePayment() {
        return true;
    }

    /** @return Whether this payment instrument can be pre-selected for immediate payment. */
    public boolean canPreselect() {
        return true;
    }

    /** @return Whether skip-UI flow with this instrument requires a user gesture. */
    public boolean isUserGestureRequiredToSkipUi() {
        return true;
    }

    /**
     * Invoke the payment app to retrieve the instrument details.
     *
     * The callback will be invoked with the resulting payment details or error.
     *
     * @param id               The unique identifier of the PaymentRequest.
     * @param merchantName     The name of the merchant.
     * @param origin           The origin of this merchant.
     * @param iframeOrigin     The origin of the iframe that invoked PaymentRequest.
     * @param certificateChain The site certificate chain of the merchant. Can be null for localhost
     *                         or local file, which are secure contexts without SSL.
     * @param methodDataMap    The payment-method specific data for all applicable payment methods,
     *                         e.g., whether the app should be invoked in test or production, a
     *                         merchant identifier, or a public key.
     * @param total            The total amount.
     * @param displayItems     The shopping cart items.
     * @param modifiers        The relevant payment details modifiers.
     * @param callback         The object that will receive the instrument details.
     */
    public abstract void invokePaymentApp(String id, String merchantName, String origin,
            String iframeOrigin, @Nullable byte[][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap, PaymentItem total,
            List<PaymentItem> displayItems, Map<String, PaymentDetailsModifier> modifiers,
            InstrumentDetailsCallback callback);

    /**
     * Update the payment information in response to payment method, shipping address, or shipping
     * option change events.
     *
     * @param response The merchant's response to the payment method, shipping address, or shipping
     *         option change events.
     */
    public void updateWith(PaymentRequestDetailsUpdate response) {}

    // TODO(sahel): Remove this stub after updating clank code. crbug.com/984694
    public void updateWith(PaymentMethodChangeResponse response) {}

    /** Called when the merchant ignored the payment method change event. */
    public void noUpdatedPaymentDetails() {}

    /**
     * @return True after changePaymentMethodFromInvokedApp(), before update updateWith() or
     * noUpdatedPaymentDetails().
     */
    public boolean isChangingPaymentMethod() {
        return false;
    }

    /**
     * Abort invocation of the payment app.
     *
     * @param id       The unique identifier of the PaymentRequest.
     * @param callback The callback to return abort result.
     */
    public void abortPaymentApp(String id, AbortCallback callback) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                callback.onInstrumentAbortResult(false);
            }
        });
    }

    /**
     * Cleans up any resources held by the payment instrument. For example, closes server
     * connections.
     */
    public abstract void dismissInstrument();

    /** @return Whether the payment instrument is ready for a microtransaction (no UI flow.) */
    public boolean isReadyForMicrotransaction() {
        return false;
    }

    /** @return Account balance for microtransaction flow. */
    @Nullable
    public String accountBalance() {
        return null;
    }

    /** Switch the instrument into the microtransaction mode. */
    public void setMicrontransactionMode() {
        mIsMicrotransaction = true;
    }
}
