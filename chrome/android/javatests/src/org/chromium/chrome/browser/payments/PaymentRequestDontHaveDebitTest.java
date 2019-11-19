// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests debit card payment. The user does not
 * have a debit card on file, but has one card that has "unknown" card type, which should not be
 * pre-selected, but should be available for the user to select.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.WEB_PAYMENTS_RETURN_GOOGLE_PAY_IN_BASIC_CARD})
public class PaymentRequestDontHaveDebitTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_debit_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "jon.doe@google.com", "en-US"));

        // Should be available, but never pre-selected:
        helper.addServerCreditCard(new CreditCard("", "https://example.com", false, true, "Jon Doe",
                "6011111111111117", "1117", "12", "2050", "diners", R.drawable.diners_card,
                CardType.UNKNOWN, billingAddressId, "server-id-2"));

        // Should not be available:
        helper.addServerCreditCard(new CreditCard("", "https://example.com", false, true, "Jon Doe",
                "378282246310005", "0005", "12", "2050", "amex", R.drawable.amex_card,
                CardType.CREDIT, billingAddressId, "server-id-3"));
        helper.addServerCreditCard(new CreditCard("", "https://example.com", false, true, "Jon Doe",
                "5555555555554444", "4444", "12", "2050", "jcb", R.drawable.jcb_card,
                CardType.PREPAID, billingAddressId, "server-id-4"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUnknownCardTypeIsNotPreselectedButAvailable() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());

        Assert.assertTrue(
                mPaymentRequestTestRule.getPaymentInstrumentLabel(0).contains("Discover"));

        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // Verify that the type mismatch bit is recorded for the most complete payment method.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.MissingPaymentFields",
                        AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_TYPE_MISMATCH));

        Assert.assertTrue(
                mPaymentRequestTestRule.getPaymentInstrumentLabel(0).contains("Discover"));

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePaymentWithUnknownCardType() throws TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "canMakePayment", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});
    }
}
