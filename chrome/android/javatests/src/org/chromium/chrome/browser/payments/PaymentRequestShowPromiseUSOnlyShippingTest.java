// Copyright 2019 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the show promise with restricted shipping rules.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowPromiseUSOnlyShippingTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("show_promise/us_only_shipping.html", this);

    @Override
    public void onMainActivityStarted() {}

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotShipWithFastApp() throws TimeoutException {
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.IMMEDIATE_RESPONSE);
        runCannotShipTest();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotShipWithSlowApp() throws TimeoutException {
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.DELAYED_RESPONSE, PaymentRequestTestRule.DELAYED_CREATION);
        runCannotShipTest();
    }

    private void runCannotShipTest() throws TimeoutException {
        AutofillTestHelper autofillTestHelper = new AutofillTestHelper();
        autofillTestHelper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Jon Doe", "Google", "51 Breithaupt St", "ON", "Kitchener", "", "N2H 5G5", "", "CA",
                "555-222-2222", "", "en-CA"));
        mRule.triggerUIAndWait(mRule.getReadyForInput());
        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        Assert.assertEquals("To see shipping methods and requirements, select an address",
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
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.IMMEDIATE_RESPONSE);
        runCanShipTest();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanShipWithSlowApp() throws TimeoutException {
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.DELAYED_RESPONSE, PaymentRequestTestRule.DELAYED_CREATION);
        runCanShipTest();
    }

    private void runCanShipTest() throws TimeoutException {
        AutofillTestHelper autofillTestHelper = new AutofillTestHelper();
        autofillTestHelper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Jane Smith", "Google", "340 Main St", "California", "Los Angeles", "", "90291", "",
                "US", "555-111-1111", "", "en-US"));
        mRule.triggerUIAndWait(mRule.getReadyForInput());
        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        Assert.assertEquals("To see shipping methods and requirements, select an address",
                mRule.getShippingAddressDescriptionLabel());
        Assert.assertEquals(null, mRule.getShippingAddressWarningLabel());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(0, mRule.getReadyToPay());
        Assert.assertEquals(null, mRule.getShippingAddressDescriptionLabel());
        Assert.assertEquals(null, mRule.getShippingAddressWarningLabel());
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }
}
