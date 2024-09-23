// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppSpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.payments.Event2;
import org.chromium.components.payments.PaymentFeatureList;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay when performing the
 * single payment app UI skip optimization.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Speed up the test by not looking up actual apps installed on the device.
    "disable-features=" + PaymentFeatureList.SERVICE_WORKER_PAYMENT_APPS
})
public class PaymentRequestPaymentAppUiSkipTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_bobpay_ui_skip_test.html");

    /** If the transaction fails, the browser shows an error message. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testFail() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyFail", mPaymentRequestTestRule.getResultReady());

        mPaymentRequestTestRule.expectResultContains(new String[] {"Transaction failed"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome immediately.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome after a slight delay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, but responds immediately to
     * getInstruments.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.FAST_FACTORY,
                AppSpeed.SLOW_APP);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, and responds slowly to
     * getInstruments.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedSlowBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.SLOW_FACTORY,
                AppSpeed.SLOW_APP);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2",
                        Event2.REQUEST_METHOD_OTHER
                                | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                                | Event2.SKIPPED_SHOW
                                | Event2.SELECTED_OTHER
                                | Event2.PAY_CLICKED
                                | Event2.COMPLETED));
    }

    /** Two payments apps with the same payment method name should not skip payments UI. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testTwoPaymentsAppsWithTheSamePaymentMethodName() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.FAST_FACTORY,
                AppSpeed.FAST_APP);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.FAST_FACTORY,
                AppSpeed.FAST_APP);
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2",
                        Event2.REQUEST_METHOD_OTHER
                                | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                                | Event2.SHOWN
                                | Event2.SELECTED_OTHER
                                | Event2.PAY_CLICKED
                                | Event2.COMPLETED));
    }
}
