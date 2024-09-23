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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.Event2;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests a payer name and provides free shipping
 * regardless of address.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNameAndFreeShippingTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_name_and_free_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address with a valid payer name on disk.
        String billingAddressId =
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
                                .setLanguageCode("en-US")
                                .build());
        helper.setCreditCard(
                new CreditCard(
                        "",
                        "https://example.test",
                        true,
                        "Jon Doe",
                        "4111111111111111",
                        "1111",
                        "12",
                        "2050",
                        "visa",
                        R.drawable.visa_card,
                        billingAddressId,
                        /* serverId= */ ""));
    }

    /** Submit the payer name and shipping address to the merchant when the user clicks "Pay." */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "Jon Doe",
                    "Jon Doe",
                    "4111111111111111",
                    "12",
                    "2050",
                    "basic-card",
                    "123",
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
     * Test that ending a payment request that requires a payer name and a shipping address results
     * in the appropriate metric being logged in PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        // Start and cancel the Payment Request.
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_PAYER_DATA
                        | Event2.REQUEST_METHOD_BASIC_CARD;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }
}
