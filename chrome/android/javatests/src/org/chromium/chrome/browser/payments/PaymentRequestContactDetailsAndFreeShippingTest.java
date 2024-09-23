// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.Event2;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests a payer name, an email address and a
 * phone number and provides free shipping regardless of address.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestContactDetailsAndFreeShippingTest {
    // A fake payment method.
    private static final String BOBPAY_TEST = "https://bobpay.test";

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule(
                    "payment_request_contact_details_and_free_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address with a valid email address and a valid phone number on
        // disk.
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("555-555-5555")
                        .setEmailAddress("jon.doe@google.com")
                        .setLanguageCode("en-US")
                        .build());

        mPaymentRequestTestRule.addPaymentAppFactory(
                BOBPAY_TEST, AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /**
     * Submit the payer name, email address, phone number and shipping address to the merchant when
     * the user clicks "Pay."
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "Jon Doe",
                    "jon.doe@google.com",
                    "+15555555555",
                    "Google",
                    "340 Main St",
                    "CA",
                    "Los Angeles",
                    "90291",
                    "US",
                    "en",
                    "freeShippingOption"
                });
    }

    /**
     * Test that ending a payment request that requires an email address, a phone number, a name and
     * a shipping address results in the appropriate metric being logged in PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_PAYER_DATA
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_OTHER;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("PaymentRequest.Events2", expectedSample)
                        .build();

        // Start and complete the Payment Request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "Jon Doe",
                    "jon.doe@google.com",
                    "+15555555555",
                    "Google",
                    "340 Main St",
                    "CA",
                    "Los Angeles",
                    "90291",
                    "US",
                    "en",
                    "freeShippingOption"
                });
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }
}
