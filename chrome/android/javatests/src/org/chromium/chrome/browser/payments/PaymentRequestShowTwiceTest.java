// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.NotShownReason;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that calls show() twice.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowTwiceTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_show_twice_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(AutofillProfile.builder()
                                                            .setFullName("Jon Doe")
                                                            .setCompanyName("Google")
                                                            .setStreetAddress("340 Main St")
                                                            .setRegion("CA")
                                                            .setLocality("Los Angeles")
                                                            .setPostalCode("90291")
                                                            .setCountryCode("US")
                                                            .setPhoneNumber("555-555-5555")
                                                            .setEmailAddress("en-US")
                                                            .build());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSecondShowRequestCancelled() throws TimeoutException {
        // Install two payment apps, so that the PaymentRequest UI is shown rather than skipped.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://alicepay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "showFirst()", mPaymentRequestTestRule.getReadyToPay());
        Assert.assertEquals(
                "\"Second request: AbortError: Another PaymentRequest UI is already showing in a different tab or window.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("showSecond()"));

        // The web payments UI was not aborted.
        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(-1 /* none */);

        // The second UI was never shown due to another web payments UI already showing.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CheckoutFunnel.NoShow",
                        NotShownReason.CONCURRENT_REQUESTS));

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
    }
}
