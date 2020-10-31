// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** An integration test for shipping address and payer's contact information delegation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MediumTest
public class PaymentHandlerEnableDelegationsTest {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

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
                mServer.getURL("/components/test/data/payments/payment_handler.html"));

        // Find the web contents where JavaScript will be executed and instrument the browser
        // payment sheet.
        mRule.openPage();
    }

    private void installPaymentHandlerWithDelegations(String delegations) throws Throwable {
        Assert.assertEquals("\"success\"",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        mRule.getActivity().getCurrentWebContents(),
                        "install().then(result => {domAutomationController.send(result);});"));
        Assert.assertEquals("\"success\"",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        mRule.getActivity().getCurrentWebContents(),
                        "enableDelegations(" + delegations
                                + ").then(result => {domAutomationController.send(result);});"));
    }

    @After
    public void tearDown() {
        mServer.stopAndDestroyServer();
    }

    private void createPaymentRequestAndWaitFor(String paymentOptions, CallbackHelper helper)
            throws Throwable {
        int callCount = helper.getCallCount();
        Assert.assertEquals("\"success\"",
                mRule.runJavaScriptCodeInCurrentTab(
                        "paymentRequestWithOptions(" + paymentOptions + ");"));
        helper.waitForCallback(callCount);
    }

    @Test
    @Feature({"Payments"})
    @MediumTest
    public void testShippingDelegation() throws Throwable {
        installPaymentHandlerWithDelegations("['shippingAddress']");
        // The pay button should be enabled when shipping address is requested and the selected
        // payment instrument can provide it.
        createPaymentRequestAndWaitFor("{requestShipping: true}", mRule.getReadyToPay());

        // Click the pay button and wait for success.
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
    }

    @Test
    @Feature({"Payments"})
    @MediumTest
    public void testContactDelegation() throws Throwable {
        installPaymentHandlerWithDelegations("['payerName', 'payerEmail', 'payerPhone']");
        // The pay button should be enabled when payer's contact information is requested and the
        // selected payment instrument can provide it.
        createPaymentRequestAndWaitFor(
                "{requestPayerName: true, requestPayerEmail: true, requestPayerPhone: true}",
                mRule.getReadyToPay());

        // Click the pay button and wait for success.
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
    }

    @Test
    @Feature({"Payments"})
    @MediumTest
    @DisabledTest(message = "crbug.com/1131674")
    public void testShippingAndContactInfoDelegation() throws Throwable {
        installPaymentHandlerWithDelegations(
                "['shippingAddress', 'payerName', 'payerEmail', 'payerPhone']");
        // The pay button should be enabled when shipping address and payer's contact information
        // are requested and the selected payment instrument can provide them.
        createPaymentRequestAndWaitFor(
                "{requestShipping: true, requestPayerName: true, requestPayerEmail: true,"
                        + " requestPayerPhone: true}",
                mRule.getReadyToPay());

        // Click the pay button and wait for success.
        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());
    }

    @Test
    @Feature({"Payments"})
    @MediumTest
    public void testPartialDelegationShippingNotSupported() throws Throwable {
        installPaymentHandlerWithDelegations("['payerName', 'payerEmail', 'payerPhone']");
        createPaymentRequestAndWaitFor(
                "{requestShipping: true, requestPayerName: true, requestPayerEmail: true}",
                mRule.getReadyForInput());
        // Shipping section must exist in payment sheet since shipping address is requested and
        // won't be provided by the selected payment handler.
        Assert.assertEquals(View.VISIBLE,
                mRule.getPaymentRequestUI().getShippingAddressSectionForTest().getVisibility());
    }

    @Test
    @Feature({"Payments"})
    @MediumTest
    public void testPartialDelegationContactInfoNotSupported() throws Throwable {
        installPaymentHandlerWithDelegations("['shippingAddress']");
        createPaymentRequestAndWaitFor(
                "{requestShipping: true, requestPayerName: true, requestPayerEmail: true}",
                mRule.getReadyForInput());
        // Contact section must exist in payment sheet since payer's name and email are requested
        // and won't be provided by the selected payment handler.
        Assert.assertEquals(View.VISIBLE,
                mRule.getPaymentRequestUI().getContactDetailsSectionForTest().getVisibility());
    }
}
