// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppSpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.TestFactory;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.TestPay;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/** A payment integration test for a merchant that requests payment via Bob Pay. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPaymentAppTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_bobpay_test.html");

    /** If no payment methods are supported, reject the show() promise. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods() throws TimeoutException {
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"The payment method", "not supported"});
    }

    /**
     * If Bob Pay factory does not have any apps, reject the show() promise. Here Bob Pay factory
     * responds to Chrome immediately.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoAppsInFastBobPayFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.NO_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"The payment method", "not supported"});
    }

    /**
     * If Bob Pay factory does not have any apps, reject the show() promise. Here Bob Pay factory
     * responds to Chrome after a slight delay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoAppsInSlowBobPayFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.NO_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"The payment method", "not supported"});
    }

    /** If the factory creates more payment apps after the UI has been dismissed, don't crash. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAppsCreatedAfterDismissShouldNotCrash() throws TimeoutException {
        TestFactory factory =
                mPaymentRequestTestRule.addPaymentAppFactory(
                        AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    factory.getDelegateForTest()
                            .onPaymentAppCreated(
                                    new TestPay("https://bobpay.test", AppSpeed.FAST_APP));
                    factory.getDelegateForTest()
                            .onPaymentAppCreated(
                                    new TestPay("https://alicepay.test", AppSpeed.FAST_APP));
                });
        mPaymentRequestTestRule.expectResultContains(new String[] {"\"transaction\": 1337"});
    }

    /** If the factory calls into delegate after the UI has been dismissed, don't crash. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testFactoryActivityAfterDismissShouldNotCrash() throws TimeoutException {
        TestFactory factory =
                mPaymentRequestTestRule.addPaymentAppFactory(
                        AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    factory.getDelegateForTest().onCanMakePaymentCalculated(true);
                    factory.getDelegateForTest().onDoneCreatingPaymentApps(factory);
                });
        mPaymentRequestTestRule.expectResultContains(new String[] {"\"transaction\": 1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * factory responds to Chrome immediately.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPayFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * factory responds to Chrome after a slight delay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPayFactory() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, but responds immediately to
     * invokePaymentApp().
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test",
                AppPresence.HAVE_APPS,
                FactorySpeed.FAST_FACTORY,
                AppSpeed.FAST_APP);
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.test", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, and responds slowly to
     * invokePaymentApp().
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
    }
}
