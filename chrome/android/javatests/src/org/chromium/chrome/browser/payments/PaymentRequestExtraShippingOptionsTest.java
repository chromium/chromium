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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that provides shipping options, but does not request a
 * shipping address.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestExtraShippingOptionsTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_extra_shipping_options_test.html");

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
        helper.setCreditCard(new CreditCard("", "https://example.test", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    /**
     * Submit the payment information without shipping address or shipping options to the merchant
     * when the user clicks "Pay."
     */
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
                new String[] {"Jon Doe", "4111111111111111", "12", "2050", "basic-card", "123",
                        "Google", "340 Main St", "CA", "Los Angeles", "90291", "US", "en"});
    }
}
