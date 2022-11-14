// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.text.TextUtils;
import android.util.JsonWriter;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.FullCardRequestDelegate;
import org.chromium.chrome.browser.autofill.PersonalDataManager.NormalizedAddressRequestDelegate;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.PayerData;
import org.chromium.components.payments.PaymentApp.InstrumentDetailsCallback;
import org.chromium.content_public.browser.WebContents;

import java.io.IOException;
import java.io.StringWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The locally stored credit card payment instrument.
 */
// TODO(crbug.com/1209835): Move this class into autofill now that it no longer interacts with
// Payments code.
public class AutofillPaymentInstrument extends EditableOption
        implements FullCardRequestDelegate, NormalizedAddressRequestDelegate {
    // Bit field values are identical to CreditCardCompletionStatus fields in
    // autofill_card_validation.h. Please modify autofill_card_validation.h after changing these.
    @IntDef({CompletionStatus.COMPLETE, CompletionStatus.CREDIT_CARD_EXPIRED,
            CompletionStatus.CREDIT_CARD_NO_CARDHOLDER, CompletionStatus.CREDIT_CARD_NO_NUMBER,
            CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS,
            CompletionStatus.CREDIT_CARD_TYPE_MISMATCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CompletionStatus {
        int COMPLETE = 0;
        int CREDIT_CARD_EXPIRED = 1 << 0;
        int CREDIT_CARD_NO_CARDHOLDER = 1 << 1;
        int CREDIT_CARD_NO_NUMBER = 1 << 2;
        int CREDIT_CARD_NO_BILLING_ADDRESS = 1 << 3;
        int CREDIT_CARD_TYPE_MISMATCH = 1 << 4;
    }

    private final WebContents mWebContents;
    private CreditCard mCard;
    private String mSecurityCode;
    @Nullable
    private AutofillProfile mBillingAddress;
    @Nullable
    private String mMethodName;
    @Nullable
    private InstrumentDetailsCallback mCallback;
    private boolean mIsWaitingForBillingNormalization;
    private boolean mIsWaitingForFullCardDetails;
    private boolean mHasValidNumberAndName;

    /**
     * Builds a payment instrument for the given credit card.
     *
     * @param webContents                    The web contents where PaymentRequest was invoked.
     * @param card                           The autofill card that can be used for payment.
     * @param billingAddress                 The billing address for the card.
     * @param methodName                     The payment method name, e.g., "basic-card", "visa",
     *                                       amex", or null.
     */
    public AutofillPaymentInstrument(WebContents webContents, CreditCard card,
            @Nullable AutofillProfile billingAddress, @Nullable String methodName) {
        super(card.getGUID(), card.getNetworkAndLastFourDigits(), card.getName(), null);
        mWebContents = webContents;
        mCard = card;
        mBillingAddress = billingAddress;
        mIsEditable = true;
        mMethodName = methodName;

        Context context = ContextUtils.getApplicationContext();
        if (context == null) return;

        if (card.getIssuerIconDrawableId() != 0) {
            updateDrawableIcon(
                    AppCompatResources.getDrawable(context, card.getIssuerIconDrawableId()));
        }

        checkAndUpdateCardCompleteness(context);
    }

    @Override
    public void onFullCardDetails(CreditCard updatedCard, String cvc) {
        // Keep the cvc for after the normalization.
        mSecurityCode = cvc;

        // The card number changes for unmasked cards.
        assert updatedCard.getNumber().length() > 4;
        mCard.setNumber(updatedCard.getNumber());

        // Update the card's expiration date.
        mCard.setMonth(updatedCard.getMonth());
        mCard.setYear(updatedCard.getYear());

        mIsWaitingForFullCardDetails = false;

        // Show the loading UI while the address gets normalized.
        mCallback.onInstrumentDetailsLoadingWithoutUI();

        // Wait for the billing address normalization before sending the instrument details.
        if (!mIsWaitingForBillingNormalization) sendInstrumentDetails();
    }

    @Override
    public void onAddressNormalized(AutofillProfile profile) {
        if (!mIsWaitingForBillingNormalization) return;
        mIsWaitingForBillingNormalization = false;

        // If the normalization finished first, use the normalized address.
        if (profile != null) mBillingAddress = profile;

        // Wait for the full card details before sending the instrument details.
        if (!mIsWaitingForFullCardDetails) sendInstrumentDetails();
    }

    @Override
    public void onCouldNotNormalize(AutofillProfile profile) {
        onAddressNormalized(null);
    }

    /**
     * Stringify the card details and send the resulting string and the method name to the
     * registered callback.
     */
    private void sendInstrumentDetails() {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            json.beginObject();

            json.name("cardholderName").value(mCard.getName());
            json.name("cardNumber").value(mCard.getNumber());
            json.name("expiryMonth").value(mCard.getMonth());
            json.name("expiryYear").value(mCard.getYear());
            json.name("cardSecurityCode").value(mSecurityCode);

            json.name("billingAddress").beginObject();

            json.name("country").value(ensureNotNull(mBillingAddress.getCountryCode()));
            json.name("region").value(ensureNotNull(mBillingAddress.getRegion()));
            json.name("city").value(ensureNotNull(mBillingAddress.getLocality()));
            json.name("dependentLocality")
                    .value(ensureNotNull(mBillingAddress.getDependentLocality()));

            json.name("addressLine").beginArray();
            String multipleLines = ensureNotNull(mBillingAddress.getStreetAddress());
            if (!TextUtils.isEmpty(multipleLines)) {
                String[] lines = multipleLines.split("\n");
                for (int i = 0; i < lines.length; i++) {
                    json.value(lines[i]);
                }
            }
            json.endArray();

            json.name("postalCode").value(ensureNotNull(mBillingAddress.getPostalCode()));
            json.name("sortingCode").value(ensureNotNull(mBillingAddress.getSortingCode()));
            json.name("languageCode").value(ensureNotNull(mBillingAddress.getLanguageCode()));
            json.name("organization").value(ensureNotNull(mBillingAddress.getCompanyName()));
            json.name("recipient").value(ensureNotNull(mBillingAddress.getFullName()));
            json.name("phone").value(ensureNotNull(mBillingAddress.getPhoneNumber()));

            json.endObject();

            json.endObject();
        } catch (IOException e) {
            onFullCardError();
            return;
        } finally {
            mSecurityCode = "";
        }

        mCallback.onInstrumentDetailsReady(mMethodName, stringWriter.toString(), new PayerData());
        mCallback = null;
    }

    private static String ensureNotNull(@Nullable String value) {
        return value == null ? "" : value;
    }

    @Override
    public void onFullCardError() {
        // There's no need to disambiguate between user cancelling the CVC unmask and other types of
        // failures, because a failure to unmask an Autofill card will show the Payment Request UI
        // again and prompt the user to attempt to complete a transaction using a different card.
        mCallback.onInstrumentDetailsError(ErrorStrings.USER_CANCELLED);
        mCallback = null;
    }

    /**
     * @return Whether the card is complete and ready to be sent to the merchant as-is. If true,
     * this card has a valid card number, a non-empty name on card, and a complete billing address.
     */
    @Override
    public boolean isComplete() {
        return mIsComplete;
    }

    /**
     * Updates the instrument and marks it "complete." Called after the user has edited this
     * instrument.
     *
     * @param card           The new credit card to use. The GUID should not change.
     * @param methodName     The payment method name to use for this instrument, e.g., "visa",
     *                       "basic-card".
     * @param billingAddress The billing address for the card. The GUID should match the billing
     *                       address ID of the new card to use.
     */
    public void completeInstrument(
            CreditCard card, String methodName, AutofillProfile billingAddress) {
        assert card != null;
        assert methodName != null;
        assert billingAddress != null;
        assert card.getBillingAddressId() != null;
        assert card.getBillingAddressId().equals(billingAddress.getGUID());
        assert card.getIssuerIconDrawableId() != 0;
        assert AutofillAddress.checkAddressCompletionStatus(
                billingAddress, AutofillAddress.CompletenessCheckType.IGNORE_PHONE)
                == AutofillAddress.CompletionStatus.COMPLETE;

        mCard = card;
        mMethodName = methodName;
        mBillingAddress = billingAddress;

        Context context = ContextUtils.getApplicationContext();
        if (context == null) return;

        updateIdentifierLabelsAndIcon(card.getGUID(), card.getNetworkAndLastFourDigits(),
                card.getName(), null,
                AppCompatResources.getDrawable(context, card.getIssuerIconDrawableId()));
        checkAndUpdateCardCompleteness(context);
        assert mIsComplete;
        assert mHasValidNumberAndName;
    }

    /**
     * Checks whether card is complete, i.e., can be sent to the merchant as-is without editing
     * first. And updates edit message, edit title and complete status.
     *
     * For both local and server cards, verifies that the billing address is present. For local
     * cards also verifies that the card number is valid and the name on card is not empty.
     *
     * Does not check that the billing address has all of the required fields. This is done
     * elsewhere to filter out such billing addresses entirely.
     *
     * Does not check the expiration date for mIsComplete. If the card is expired, the user has the
     * opportunity update the expiration date when providing their CVC in the card unmask dialog.
     *
     * Does not check that the card type is accepted by the merchant. This is done elsewhere to
     * filter out such cards from view entirely.
     *
     * Completeness weights for all fields are identiacal to their equivalent in
     * GetCompletenessScore from autofill_card_validation.cc, Please modify the weights in both
     * files if needed.
     */
    private void checkAndUpdateCardCompleteness(Context context) {
        int editMessageResId = 0; // Zero is the invalid resource Id.
        int editTitleResId = R.string.payments_edit_card;
        int invalidFieldsCount = 0;
        mCompletenessScore = 0;
        int missingFields = getMissingFields();

        // Even though expiration date does not affect mIsComplete, cards with valid expiration date
        // still score higher than expired cards. This is used to list expired cards after
        // non-expired cards after sorting instruments.
        if ((missingFields & CompletionStatus.CREDIT_CARD_EXPIRED) == 0) mCompletenessScore += 6;

        if ((missingFields & CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS) == 0) {
            // Add 10 for complete address.
            mCompletenessScore += 10;
        } else {
            editMessageResId = R.string.payments_billing_address_required;
            editTitleResId = R.string.payments_add_billing_address;
            invalidFieldsCount++;
        }

        mHasValidNumberAndName = true;
        if ((missingFields & CompletionStatus.CREDIT_CARD_NO_CARDHOLDER) == 0) {
            // Add 8 for complete card holder's name.
            mCompletenessScore += 8;
        } else {
            mHasValidNumberAndName = false;
            editMessageResId = R.string.payments_name_on_card_required;
            editTitleResId = R.string.payments_add_name_on_card;
            invalidFieldsCount++;
        }
        if ((missingFields & CompletionStatus.CREDIT_CARD_NO_NUMBER) == 0) {
            // Add 13 for valid card number.
            mCompletenessScore += 13;
        } else {
            mHasValidNumberAndName = false;
            editMessageResId = R.string.payments_card_number_invalid_validation_message;
            editTitleResId = R.string.payments_add_valid_card_number;
            invalidFieldsCount++;
        }

        if (invalidFieldsCount > 1) {
            editMessageResId = R.string.payments_more_information_required;
            editTitleResId = R.string.payments_add_more_information;
        }

        mEditMessage = editMessageResId == 0 ? null : context.getString(editMessageResId);
        mEditTitle = context.getString(editTitleResId);
        mIsComplete = mEditMessage == null;
    }

    /** @return The credit card represented by this payment instrument. */
    public CreditCard getCard() {
        return mCard;
    }

    /** @return The billing address associated with this payment instrument. */
    @Nullable
    public AutofillProfile getBillingProfile() {
        return mBillingAddress;
    }

    @Override
    public String getPreviewString(String labelSeparator, int maxLength) {
        StringBuilder previewString = new StringBuilder(getLabel());
        if (maxLength < 0) return previewString.toString();

        int networkNameEndIndex = previewString.indexOf(" ");
        if (networkNameEndIndex > 0) {
            // Only display card network name.
            previewString.delete(networkNameEndIndex, previewString.length());
        }
        if (previewString.length() < maxLength) return previewString.toString();
        return previewString.substring(0, maxLength / 2);
    }

    /** @return a bit vector of the card's missing fields. */
    public int getMissingFields() {
        int missingFields = CompletionStatus.COMPLETE;
        if (mBillingAddress == null) {
            missingFields |= CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS;
        }
        if (!mCard.hasValidCreditCardExpirationDate()) {
            missingFields |= CompletionStatus.CREDIT_CARD_EXPIRED;
        }

        if (mCard.getIsLocal()) {
            if (TextUtils.isEmpty(mCard.getName())) {
                missingFields |= CompletionStatus.CREDIT_CARD_NO_CARDHOLDER;
            }

            if (PersonalDataManager.getInstance().getBasicCardIssuerNetwork(
                        mCard.getNumber().toString(), true)
                    == null) {
                missingFields |= CompletionStatus.CREDIT_CARD_NO_NUMBER;
            }
        }

        return missingFields;
    }
}
