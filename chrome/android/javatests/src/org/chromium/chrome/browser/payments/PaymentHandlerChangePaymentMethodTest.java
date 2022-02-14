// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.UiDisableIf;

/** An integration test for PaymentRequestEvent.changePaymentMethod(). */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-blink-features=PaymentMethodChangeEvent,PaymentHandlerChangePaymentMethod"})
@MediumTest
public class PaymentHandlerChangePaymentMethodTest {

    // Open a tab on the blank page first to initiate the native bindings required by the test
    // server.
    @Rule
    public PaymentRequestTestRule mRule = new PaymentRequestTestRule("about:blank", null, true);

    // Host the tests on https://127.0.0.1, because file:// URLs cannot have service workers.
    private EmbeddedTestServer mServer;

    @Before
    public void setUp() throws Throwable {
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getContext(), ServerCertificate.CERT_OK);
        mRule.startMainActivityWithURL(
                mServer.getURL("/components/test/data/payments/change_payment_method.html"));

        // Find the web contents where JavaScript will be executed and instrument the browser
        // payment sheet.
        mRule.openPage();
    }

    private void installPaymentHandler() throws Throwable {
        mRule.runJavaScriptCodeInCurrentTab("install();");
        mRule.expectResultContains(new String[] {"instruments.set(): Payment handler installed."});
    }

    @After
    public void tearDown() {
        mServer.stopAndDestroyServer();
    }

    /**
     * Verify that absence of the "paymentmethodchange" event handler in the merchant will cause
     * PaymentRequestEvent.changePaymentMethod() to resolve with null.
     */
    @Test
    @Feature({"Payments"})
    public void testNoEventHandler() throws Throwable {
        installPaymentHandler();
        mRule.clickNodeAndWait("testNoHandler", mRule.getDismissed());
        mRule.expectResultContains(
                new String[] {"PaymentRequest.show(): changePaymentMethod() returned: null"});
    }

    /**
     * Verify that rejecting the promise passed into PaymentMethodChangeEvent.updateWith() will
     * cause PaymentRequest.show() to reject and thus abort the transaction.
     */
    @Test
    @Feature({"Payments"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // See https://crbug.com/1136100.
    public void testReject() throws Throwable {
        installPaymentHandler();
        mRule.clickNodeAndWait("testReject", mRule.getDismissed());
        mRule.expectResultContains(
                new String[] {"PaymentRequest.show() rejected with: Error for test"});
    }

    /**
     * Verify that a JavaScript exception in the "paymentmethodchange" event handler will cause
     * PaymentRequest.show() to reject and thus abort the transaction.
     */
    @Test
    @Feature({"Payments"})
    public void testThrow() throws Throwable {
        installPaymentHandler();
        mRule.clickNodeAndWait("testThrow", mRule.getDismissed());
        mRule.expectResultContains(
                new String[] {"PaymentRequest.show() rejected with: Error: Error for test"});
    }

    /**
     * Verify that the payment handler receives a subset of the payment details passed into
     * PaymentMethodChangeEvent.updateWith() with URL-based payment method identifier.
     */
    @Test
    @Feature({"Payments"})
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // See https://crbug.com/1136100.
    public void testDetails() throws Throwable {
        installPaymentHandler();
        mRule.clickNodeAndWait("testDetails", mRule.getDismissed());
        // Look for the this exact return value to ensure that the browser redacts some details
        // before forwarding them to the payment handler.
        mRule.expectResultContains(
                new String[] {"PaymentRequest.show(): changePaymentMethod() returned: "
                                + "{\"error\":\"Error for test\",\"modifiers\":"
                                + "[{\"data\":{\"soup\":\"potato\"},"
                                + "\"supportedMethods\":\"https://127.0.0.1:",
                        // Port changes every time, so don't hardcode it here.
                        "/pay\",\"total\":{\"amount\":{\"currency\":\"EUR\",\"value\":\"0.03\"},"
                                + "\"label\":\"\",\"pending\":false}}],"
                                + "\"paymentMethodErrors\":{\"country\":\"Unsupported country\"},"
                                + "\"total\":{\"currency\":\"GBP\",\"value\":\"0.02\"}}"});
    }
}
