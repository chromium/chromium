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
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the show promise that resolves with an empty dictionary.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowPromiseEmptyTest {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("show_promise/resolve_with_empty_dictionary.html");

    @Before
    public void setUp() throws TimeoutException {
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "650-253-0000", "", "en-US"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testResolveWithEmptyDictionary() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mRule.clickNodeAndWait("buy", mRule.getReadyToPay());

        Assert.assertEquals("USD $3.00", mRule.getOrderSummaryTotal());

        mRule.clickInOrderSummaryAndWait(mRule.getReadyToPay());

        Assert.assertEquals(2, mRule.getNumberOfLineItems());
        Assert.assertEquals("$1.00", mRule.getLineItemAmount(0));
        Assert.assertEquals("$1.00", mRule.getLineItemAmount(1));

        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyToPay());

        Assert.assertEquals(null, mRule.getShippingAddressDescriptionLabel());

        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());

        mRule.expectResultContains(new String[] {"3.00", "shipping-option-identifier"});
    }
}
