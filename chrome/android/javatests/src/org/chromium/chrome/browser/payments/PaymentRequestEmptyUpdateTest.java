// Copyright 2017 The Chromium Authors
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
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address, but always rejects all
 * shipping addresses via updateWith({}), which is an "empty update."
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestEmptyUpdateTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_empty_update_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk.
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
                                .setPhoneNumber("650-253-0000")
                                .setEmailAddress("en-US")
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

    /** Expand the shipping address section and select a valid address. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testSelectValidAddress() throws Throwable {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyForInput());
    }
}
