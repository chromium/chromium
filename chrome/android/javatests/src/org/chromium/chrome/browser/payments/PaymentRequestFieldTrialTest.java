// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;

import android.support.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the different fields trials affecting payment requests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestFieldTrialTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_bobpay_and_cards_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk but no credit card.
        helper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "555-555-5555", "",
                "en-US"));
    }

    /**
     * Tests that the Payment Request is cancelled if the user has no credit card and the proper
     * command line flag is set.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("enable-features=NoCreditCardAbort")
    public void testAbortIfNoCard_Enabled_NoApp() throws TimeoutException {
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Payment method not supported"});
    }

    /**
     * Tests that the Payment Request UI is shown even if the user has no credit card and the proper
     * command line flag is set, if they have another payment app that is accepted by the merchant.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("enable-features=NoCreditCardAbort")
    public void testAbortIfNoCard_Enabled_WithApp() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
    }

    /**
     * Tests that the Payment Request UI is shown if the user has no credit card and the proper
     * command line flag is set.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testAbortIfNoCard_Disabled() throws TimeoutException {
        // Check that the Payment Request UI is shown.
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
    }
}
