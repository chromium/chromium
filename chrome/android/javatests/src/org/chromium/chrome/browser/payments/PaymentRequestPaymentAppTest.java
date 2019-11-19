// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_CREATION;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NO_INSTRUMENTS;

import android.support.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.TestPay;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPaymentAppTest {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_bobpay_test.html");

    /** If no payment methods are supported, reject the show() promise. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods() throws TimeoutException {
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "Payment method not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome immediately.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "Payment method not supported"});
    }

    /**
     * If Bob Pay does not have any instruments, reject the show() promise. Here Bob Pay responds to
     * Chrome after a slight delay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInSlowBobPay() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(NO_INSTRUMENTS, DELAYED_RESPONSE);
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "Payment method not supported"});
    }

    /**
     * If the payment app responds with more instruments after the UI has been dismissed, don't
     * crash.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentWithInstrumentsAppResponseAfterDismissShouldNotCrash()
            throws TimeoutException {
        final TestPay app = new TestPay("https://bobpay.com", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        PaymentAppFactory.getInstance().addAdditionalFactory(
                (webContents, methodNames, mayCrawlUnused, callback) -> {
                    callback.onPaymentAppCreated(app);
                    callback.onAllPaymentAppsCreated();
                });
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        TestThreadUtils.runOnUiThreadBlocking(() -> app.respond());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "User closed the Payment Request UI."});
    }

    /**
     * If the payment app responds with no instruments after the UI has been dismissed, don't crash.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/769851")
    @Feature({"Payments"})
    public void testPaymentAppNoInstrumentsResponseAfterDismissShouldNotCrash()
            throws TimeoutException {
        final TestPay app = new TestPay("https://bobpay.com", NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
        PaymentAppFactory.getInstance().addAdditionalFactory(
                (webContents, methodNames, mayCrawlUnused, callback) -> {
                    callback.onPaymentAppCreated(app);
                    callback.onAllPaymentAppsCreated();
                });
        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        TestThreadUtils.runOnUiThreadBlocking(() -> app.respond());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "Payment method not supported"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome immediately.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * If Bob Pay is supported and installed, user should be able to pay with it. Here Bob Pay
     * responds to Chrome after a slight delay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaSlowBobPay() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, but responds immediately
     * to getInstruments.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedFastBobPay() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(
                "https://bobpay.com", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE, DELAYED_CREATION);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * Test payment with a Bob Pay that is created with a delay, and responds slowly to
     * getInstruments.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayViaDelayedSlowBobPay() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(
                "https://bobpay.com", HAVE_INSTRUMENTS, DELAYED_RESPONSE, DELAYED_CREATION);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }
}
