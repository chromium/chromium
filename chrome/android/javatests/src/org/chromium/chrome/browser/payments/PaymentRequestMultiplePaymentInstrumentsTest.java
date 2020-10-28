// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping
 * and user that has 5 addresses stored in autofill settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestMultiplePaymentInstrumentsTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_no_shipping_test.html", this);

    private static final AutofillProfile AUTOFILL_PROFILE = new AutofillProfile("" /* guid */,
            "https://www.example.com" /* origin */, "" /* honorific prefix */, "Lisa Simpson",
            "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
            "555 123-4567", "lisa@simpson.com", "" /* languageCode */);

    private static final CreditCard[] CREDIT_CARDS = {
            // CARD_0 missing billing address.
            new CreditCard("", "https://example.com", true /* isLocal */, true /* isCached */,
                    "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa",
                    R.drawable.visa_card, "" /* billingAddressId */, "" /* serverId */),

            // For the rest of the cards billing address id will be added in
            // onMainActivityStarted().
            // CARD_1 complete card.
            new CreditCard("", "https://example.com", true /* isLocal */, true /* isCached */,
                    "John Smith", "4111111111113333", "3333", "10", "2050", "visa",
                    R.drawable.amex_card, "" /* billingAddressId */, "" /* serverId */),

            // CARD_2 complete card, different from CARD_1.
            new CreditCard("", "https://example.com", true /* isLocal */, true /* isCached */,
                    "Jane Doe", "4111111111112222", "2222", "07", "2077", "visa",
                    R.drawable.visa_card, "" /* billingAddressId */, "" /* serverId */),

            // CARD_3 expired card.
            new CreditCard("", "https://example.com", true /* isLocal */, true /* isCached */,
                    "Lisa Simpson", "4111111111111111", "1111", "12", "2010", "visa",
                    R.drawable.visa_card, "" /* billingAddressId */, "" /* serverId */),

            // CARD_4 missing name.
            new CreditCard("", "https://example.com", true /* isLocal */, true /* isCached */,
                    "" /* name */, "4012888888881881", "1881", "06", "2049", "visa",
                    R.drawable.visa_card, "" /* billingAddressId */, "" /* serverId */),
    };

    private CreditCard[] mCreditCardsToAdd;
    private int[] mCountsToSet;
    private int[] mDatesToSet;

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();

        // The user has a complete autofill profile.
        String billingAddressId = helper.setProfile(AUTOFILL_PROFILE);
        // Add the autofill card.
        ArrayList<String> guids = new ArrayList<>();
        for (int i = 0; i < mCreditCardsToAdd.length; i++) {
            // CREDIT_CARDS[0] has no billing address.
            if (mCreditCardsToAdd[i] != CREDIT_CARDS[0]) {
                mCreditCardsToAdd[i].setBillingAddressId(billingAddressId);
            }
            String creditCardId = helper.setCreditCard(mCreditCardsToAdd[i]);
            guids.add(creditCardId);
        }

        // Set up the autofill card use stats.
        for (int i = 0; i < guids.size(); i++) {
            PaymentPreferencesUtil.setPaymentAppUseCountForTest(guids.get(i), mCountsToSet[i]);
            PaymentPreferencesUtil.setPaymentAppLastUseDate(guids.get(i), mDatesToSet[i]);
        }
    }

    /**
     * Make sure the address suggestions are in the correct order and that only the top 4
     * suggestions are shown. They should be ordered by frecency and complete addresses should be
     * suggested first.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCreditCardSuggestionOrdering() throws TimeoutException {
        mCreditCardsToAdd = new CreditCard[] {CREDIT_CARDS[0], CREDIT_CARDS[3], CREDIT_CARDS[2],
                CREDIT_CARDS[1], CREDIT_CARDS[4]};
        mCountsToSet = new int[] {20, 15, 10, 25, 30};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000, 5000};

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(5, mPaymentRequestTestRule.getNumberOfPaymentApps());
        int i = 0;
        // The two complete cards are sorted by frecency.
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(i++).contains(
                "John Smith"));
        Assert.assertTrue(
                mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(i++).contains("Jane Doe"));
        // Incomplete cards have this order: 1-expired 2-missing name 3-missing address 4-missing
        // card number.
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(i++).contains(
                "Lisa Simpson"));
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(i++).contains(
                "Cardholder name required"));
        Assert.assertTrue(
                mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(i++).contains("Jon Doe"));

        // Verify that no shipping fields is recorded since there is at least one complete
        // suggestion.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.MissingPaymentFields"));
    }

    /**
     * Make sure the name bit is recorded in missing fields when an incomplete card with missing
     * name is the most complete one.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testMissingNameFieldRecorded() throws TimeoutException {
        // Add a card with invalid billing address and another one with missing name.
        mCreditCardsToAdd = new CreditCard[] {CREDIT_CARDS[4], CREDIT_CARDS[0]};
        mCountsToSet = new int[] {5, 5};
        mDatesToSet = new int[] {5000, 5000};

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // Verify that the missing fields of the most complete payment method has been recorded.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.MissingPaymentFields",
                        AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_CARDHOLDER));
    }

    /**
     * Make sure the billing address bit is recorded in missing fields when an incomplete card
     * with missing address is the most complete one.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testMissingBillingAddressFieldRecorded() throws TimeoutException {
        // Add a card with invalid billing address.
        mCreditCardsToAdd = new CreditCard[] {CREDIT_CARDS[0]};
        mCountsToSet = new int[] {5};
        mDatesToSet = new int[] {5000};

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // Verify that the missing fields of the most complete payment method has been recorded.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.MissingPaymentFields",
                        AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_NO_BILLING_ADDRESS));
    }

    /**
     * Make sure all fields are recorded when no card exists.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=StrictHasEnrolledAutofillInstrument")
    public void testAllMissingFieldsRecorded() throws TimeoutException {
        // Don't add any cards
        mCreditCardsToAdd = new CreditCard[] {};
        mCountsToSet = new int[] {};
        mDatesToSet = new int[] {};

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // Verify that the missing fields of the most complete payment method has been recorded.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.MissingPaymentFields",
                        AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_EXPIRED
                                | AutofillPaymentInstrument.CompletionStatus
                                          .CREDIT_CARD_NO_CARDHOLDER
                                | AutofillPaymentInstrument.CompletionStatus
                                          .CREDIT_CARD_NO_BILLING_ADDRESS
                                | AutofillPaymentInstrument.CompletionStatus
                                          .CREDIT_CARD_NO_NUMBER));
    }
}
