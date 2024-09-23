// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

import java.util.concurrent.TimeoutException;

/** A payment integration test for a merchant that requests payment via Bob Pay. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class PaymentRequestInputProtectionTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_bobpay_test.html");

    /** Test that UI interactions are ignored before the input protector threshold. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testInputProtection() throws TimeoutException {
        // Install two payment apps, so that the PaymentRequest UI is shown rather than skipped.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://alicepay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.setAutoAdvanceInputProtectorClock(false);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getShowCalled());
        Assert.assertFalse(mPaymentRequestTestRule.getPaymentRequestUI().isAcceptingUserInput());

        // Interacting with the UI does nothing.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.button_primary)
                            .performClick();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.close_button)
                            .performClick();
                });
        Assert.assertTrue(
                mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest().isShowing());

        // Advance the clock and then close the UI.
        mPaymentRequestTestRule.advanceInputProtectorClock();
        mPaymentRequestTestRule.getReadyForInput().waitForOnly();
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentRequestUI().isAcceptingUserInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
    }
}
