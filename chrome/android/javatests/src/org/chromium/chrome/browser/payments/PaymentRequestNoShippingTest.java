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

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.Event2;

import java.util.concurrent.TimeoutException;

/** A payment integration test for a merchant that does not require shipping address. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNoShippingTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_no_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("650-253-0000")
                        .setEmailAddress("jon.doe@gmail.com")
                        .setLanguageCode("en-US")
                        .build());

        // This test uses two payment apps, so that the PaymentRequest UI is shown rather than
        // skipped.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://alicepay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Click [X] to cancel payment. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCloseDialog() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        Assert.assertEquals(
                "\"User closed the Payment Request UI.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));
    }

    /** Click [EDIT] to expand the dialog, then click [X] to cancel payment. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditAndCloseDialog() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        Assert.assertEquals(
                "\"User closed the Payment Request UI.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));
    }

    /** Click [EDIT] to expand the dialog, then click [CANCEL] to cancel payment. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditAndCancelDialog() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getDismissed());
        Assert.assertEquals(
                "\"User closed the Payment Request UI.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * "pay" should not crash.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickDismissAndPayShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Quickly dismiss and then press on "Continue"
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .onBackPressed();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.button_primary)
                            .performClick();
                });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        // Currently, the above calls for the back button and pay button result in the
        // PaymentRequest being in a bad state. The back button call is handled asynchronously by
        // Android, and so the pay click happens first. The show() promise resolves, kicking off the
        // must-call-complete timer, however the back button cancellation then tears down the
        // PaymentRequest state - including setting the must-call-complete timer to failed.
        //
        // TODO(crbug.com/40872814): Avoid ending up in this state.
        Assert.assertEquals(
                "\"Failed to execute 'complete' on 'PaymentResponse': "
                        + "Timed out after 60 seconds, complete() called too late\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * [X] should not crash.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickDismissAndCloseShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Quickly dismiss and then press on [X].
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .onBackPressed();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.close_button)
                            .performClick();
                });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        Assert.assertEquals(
                "\"User closed the Payment Request UI.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));
    }

    /**
     * Quickly pressing on [X] and then dismissing the dialog (via Android's back button, for
     * example) should not crash.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndDismissShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Quickly press on [X] and then dismiss.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.close_button)
                            .performClick();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .onBackPressed();
                });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        Assert.assertEquals(
                "\"User closed the Payment Request UI.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));
    }

    /**
     * Test that ending a payment request that requires user information except for the payment
     * results in the appropriate metric being logged in PaymentRequest.Events. histogram.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        // Start and cancel the Payment Request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        Assert.assertEquals(
                "\"User closed the Payment Request UI.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));

        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }
}
