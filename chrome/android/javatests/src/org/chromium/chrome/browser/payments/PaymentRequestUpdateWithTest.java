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
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

import java.util.concurrent.TimeoutException;

/** A payment integration test for updateWith(). */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestUpdateWithTest {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("payment_request_update_with_test.html");

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

        mRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** A merchant that calls updateWith() with {} will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithEmpty() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "updateWithEmpty('https://bobpay.test');", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with total will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithTotal() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "updateWithTotal('https://bobpay.test');", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $10.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with displayItems will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithDisplayItems() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "updateWithDisplayItems('https://bobpay.test');", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /** A merchant that calls updateWith() with shipping options will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithShippingOptions() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "updateWithShippingOptions('https://bobpay.test');", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"updatedShipping"});
    }

    /** A merchant that calls updateWith() with modifiers will not cause timeouts in UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithModifiers() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "updateWithModifiers('https://bobpay.test');", mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $5.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("$0.00", mRule.getLineItemAmount(2));
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyToPay());
        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());
        Assert.assertEquals("USD $4.00", mRule.getOrderSummaryTotal());
        Assert.assertEquals("$2.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$3.00", mRule.getLineItemAmount(1));
        Assert.assertEquals("-$1.00", mRule.getLineItemAmount(2));
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"freeShipping"});
    }

    /**
     * Show the shipping address validation error message even if the merchant provided some
     * shipping options.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateWithError() throws Throwable {
        mRule.runJavaScriptAndWaitForUIEvent(
                "updateWithError('https://bobpay.test');", mRule.getReadyToPay());
        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());
        mRule.clickOnShippingAddressSuggestionOptionAndWait(1, mRule.getReadyForInput());
        CharSequence actualString = mRule.getShippingAddressOptionRowAtIndex(0).getLabelText();
        Assert.assertEquals("This is an error for a browsertest", actualString);
    }
}
