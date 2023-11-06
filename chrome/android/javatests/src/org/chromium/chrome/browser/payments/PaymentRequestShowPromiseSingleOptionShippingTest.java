// Copyright 2019 The Chromium Authors
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
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppSpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the show promise with a single pre-selected shipping option and no
 * shipping address change handler.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowPromiseSingleOptionShippingTest {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("show_promise/single_option_shipping.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper autofillTestHelper = new AutofillTestHelper();
        autofillTestHelper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("555-222-2222")
                        .setLanguageCode("en-US")
                        .build());
        autofillTestHelper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jane Smith")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("555-111-1111")
                        .setLanguageCode("en-US")
                        .build());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testFastApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mRule.triggerUIAndWait("buy", mRule.getReadyToPay());
        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$0.00", mRule.getShippingOptionCostSummaryOnBottomSheet());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSlowApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.SLOW_FACTORY,
                AppSpeed.SLOW_APP);
        mRule.triggerUIAndWait("buy", mRule.getReadyToPay());
        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$0.00", mRule.getShippingOptionCostSummaryOnBottomSheet());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }
}
