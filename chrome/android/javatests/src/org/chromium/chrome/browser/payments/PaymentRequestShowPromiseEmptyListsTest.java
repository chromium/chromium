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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the show promise that resolves with empty lists of display
 * items, modifiers, and shipping options, which clears out the Payment Request data.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowPromiseEmptyListsTest {
    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("show_promise/resolve_with_empty_lists.html");

    @Before
    public void setUp() throws TimeoutException {
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "650-253-0000", "", "en-US"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testResolveWithEmptyLists() throws TimeoutException {
        mRule.addPaymentAppFactory(
                "https://example.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mRule.triggerUIAndWait("buy", mRule.getReadyForInput());

        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());

        mRule.clickInOrderSummaryAndWait(mRule.getReadyForInput());

        Assert.assertEquals(0, mRule.getNumberOfLineItems());

        mRule.clickInShippingAddressAndWait(R.id.payments_section, mRule.getReadyForInput());

        Assert.assertEquals("To see shipping methods and requirements, select an address",
                mRule.getShippingAddressDescriptionLabel());
    }
}
