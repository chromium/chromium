// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate that the use stats of Autofill profiles and credit cards
 * updated when they are used to complete a Payment Request transaction.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestUseStatsTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_free_shipping_test.html", this);

    AutofillTestHelper mHelper;
    String mBillingAddressId;
    String mCreditCardId;

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        mCreditCardId = mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));
        // Set specific use stats for the profile and credit card.
        mHelper.setProfileUseStatsForTesting(mBillingAddressId, 20, 5000);
        mHelper.setCreditCardUseStatsForTesting(mCreditCardId, 1, 5000);
    }

    /** Expect that using a profile and credit card to pay updates their usage stats. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testLogProfileAndCreditCardUse() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        // Get the current date value just before the start of the Payment Request.
        long timeBeforeRecord = mHelper.getCurrentDateForTesting();

        // Proceed with the payment.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Get the current date value just after the end of the Payment Request.
        long timeAfterRecord = mHelper.getCurrentDateForTesting();

        // Make sure the use counts were incremented and the use dates were set to the current time.
        Assert.assertEquals(21, mHelper.getProfileUseCountForTesting(mBillingAddressId));
        Assert.assertTrue(
                timeBeforeRecord <= mHelper.getProfileUseDateForTesting(mBillingAddressId));
        Assert.assertTrue(
                timeAfterRecord >= mHelper.getProfileUseDateForTesting(mBillingAddressId));
        Assert.assertEquals(2, mHelper.getCreditCardUseCountForTesting(mCreditCardId));
        Assert.assertTrue(
                timeBeforeRecord <= mHelper.getCreditCardUseDateForTesting(mCreditCardId));
        Assert.assertTrue(timeAfterRecord >= mHelper.getCreditCardUseDateForTesting(mCreditCardId));
    }
}
