// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
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

/** A payment integration test for the show promise with restricted shipping rules. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowPromiseUSOnlyShippingTest {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("show_promise/us_only_shipping.html");

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotShipWithFastApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        runCannotShipTest();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotShipWithSlowApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.SLOW_FACTORY,
                AppSpeed.SLOW_APP);
        runCannotShipTest();
    }

    private void runCannotShipTest() throws TimeoutException {
        AutofillTestHelper autofillTestHelper = new AutofillTestHelper();
        autofillTestHelper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("51 Breithaupt St")
                        .setRegion("ON")
                        .setLocality("Kitchener")
                        .setPostalCode("N2H 5G5")
                        .setCountryCode("CA")
                        .setPhoneNumber("555-222-2222")
                        .setLanguageCode("en-CA")
                        .build());
        mRule.triggerUIAndWait("buy", mRule.getReadyForInput());
        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        Assert.assertEquals(
                "To see shipping methods and requirements, select an address",
                mRule.getShippingAddressDescriptionLabel());
        Assert.assertEquals(null, mRule.getShippingAddressWarningLabel());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(0, mRule.getReadyForInput());
        Assert.assertEquals(null, mRule.getShippingAddressDescriptionLabel());
        Assert.assertEquals("Cannot ship outside of US.", mRule.getShippingAddressWarningLabel());
        mRule.clickAndWait(R.id.button_secondary, mRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanShipWithFastApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        runCanShipTest();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanShipWithSlowApp() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.SLOW_FACTORY,
                AppSpeed.SLOW_APP);
        runCanShipTest();
    }

    private void runCanShipTest() throws TimeoutException {
        AutofillTestHelper autofillTestHelper = new AutofillTestHelper();
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
        mRule.triggerUIAndWait("buy", mRule.getReadyForInput());
        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        Assert.assertEquals(
                "To see shipping methods and requirements, select an address",
                mRule.getShippingAddressDescriptionLabel());
        Assert.assertEquals(null, mRule.getShippingAddressWarningLabel());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(0, mRule.getReadyToPay());
        Assert.assertEquals(null, mRule.getShippingAddressDescriptionLabel());
        Assert.assertEquals(null, mRule.getShippingAddressWarningLabel());
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }
}
