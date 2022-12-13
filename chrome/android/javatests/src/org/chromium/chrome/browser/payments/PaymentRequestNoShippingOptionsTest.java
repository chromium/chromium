// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for when shipping is requested but no shipping
 * options are provided by the merchant. See crbug.com/1082630
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNoShippingOptionsTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_no_shipping_options_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address associated with a credit card.
        String firstAddressId = helper.setProfile(new AutofillProfile("", "https://example.test",
                true, "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA",
                "Los Angeles", "", "90291", "", "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.test", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                firstAddressId, /*serverId=*/""));

        // The user has a second address.
        String secondAddressId = helper.setProfile(new AutofillProfile("", "https://example.test",
                true, "" /* honorific prefix */, "Fred Doe", "Google", "340 Main St", "CA",
                "Los Angeles", "", "90291", "", "US", "555-555-5555", "", "en-US"));

        // Set the fist profile to have a better frecency score that the second one.
        helper.setProfileUseStatsForTesting(firstAddressId, 10, 10);
        helper.setProfileUseStatsForTesting(secondAddressId, 0, 0);
    }

    /**
     * Verifies that clicking any shipping address is invalid because there are no shipping
     * options.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testAllShippingAddressesInvalid() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());

        // Focus on the shipping addresses section to enter expanded mode.
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Expect 2 shipping options and no error yet.
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfShippingAddressSuggestions());
        Assert.assertEquals(null, mPaymentRequestTestRule.getShippingAddressWarningLabel());

        mPaymentRequestTestRule.clickOnShippingAddressSuggestionOptionAndWait(
                0, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals("Can’t ship to this address. Select a different address.",
                mPaymentRequestTestRule.getShippingAddressWarningLabel());

        mPaymentRequestTestRule.clickOnShippingAddressSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals("Can’t ship to this address. Select a different address.",
                mPaymentRequestTestRule.getShippingAddressWarningLabel());
    }
}
