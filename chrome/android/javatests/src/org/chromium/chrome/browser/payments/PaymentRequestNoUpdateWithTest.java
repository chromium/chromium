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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that either does not listen to update callbacks, does
 * not call updateWith(), or does not uses promises. These behaviors are all OK and should not cause
 * timeouts.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNoUpdateWithTest {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("payment_request_no_update_with_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Lisa Simpson")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("555 123-4567")
                        .setEmailAddress("lisa@simpson.com")
                        .build());
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Maggie Simpson")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("Uzbekistan")
                        .setPhoneNumber("555 123-4567")
                        .setEmailAddress("maggie@simpson.com")
                        .build());
        mRule.addPaymentAppFactory(AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /**
     * A merchant that does not listen to shipping address update events will not cause timeouts in
     * UI.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoEventListener() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "buyWithoutListenersWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mRule.getReadyToPay());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /**
     * A merchant that listens to shipping address update events, but does not call updateWith() on
     * the event, will not cause timeouts in UI.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoUpdateWith() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "buyWithoutCallingUpdateWithWithMethods([{supportedMethods:"
                        + " 'https://bobpay.test'}]);",
                mRule.getReadyToPay());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() without using promises will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoPromises() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "buyWithoutPromisesWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        Assert.assertEquals("USD $10.00", mRule.getOrderSummaryTotal());
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"updatedShipping"});
    }
}
