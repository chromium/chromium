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

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillProfile;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.payments.Event;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that does not require shipping address.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNoShippingTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_no_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(AutofillProfile.builder()
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
    }

    /** Click [X] to cancel payment. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testCloseDialog() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Click [EDIT] to expand the dialog, then click [X] to cancel payment. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testEditAndCloseDialog() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Click [EDIT] to expand the dialog, then click [CANCEL] to cancel payment. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testEditAndCancelDialog() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_secondary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Click [PAY] and dismiss the card unmask dialog. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jon Doe", "4111111111111111", "12", "2050", "basic-card", "123"});
    }

    /** Click [PAY], type in "123" into the CVC dialog, then submit the payment. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testCancelUnmaskAndRetry() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.NEGATIVE, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jon Doe", "4111111111111111", "12", "2050", "basic-card", "123"});
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * "pay" should not crash.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickDismissAndPayShouldNotCrash() throws TimeoutException {
        // Install two payment apps, so that the PaymentRequest UI is shown rather than skipped.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://alicepay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Quickly dismiss and then press on "Continue"
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest().onBackPressed();
            mPaymentRequestTestRule.getPaymentRequestUI()
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
        // TODO(crbug.com/1375286): Avoid ending up in this state.
        Assert.assertEquals("\"Failed to execute 'complete' on 'PaymentResponse': "
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
        // Install two payment apps, so that the PaymentRequest UI is shown rather than skipped.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://alicepay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Quickly dismiss and then press on [X].
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest().onBackPressed();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.close_button)
                    .performClick();
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        Assert.assertEquals("\"User closed the Payment Request UI.\"",
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
        // Install two payment apps, so that the PaymentRequest UI is shown rather than skipped.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://alicepay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "triggerPaymentRequest([{supportedMethods:'https://bobpay.test'}, "
                        + "{supportedMethods:'https://alicepay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Quickly press on [X] and then dismiss.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.close_button)
                    .performClick();
            mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest().onBackPressed();
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        Assert.assertEquals("\"User closed the Payment Request UI.\"",
                mPaymentRequestTestRule.runJavaScriptAndWaitForPromise("getResult()"));
    }

    /**
     * Test that ending a payment request that requires user information except for the payment
     * results in the appropriate metric being logged in PaymentRequest.Events.
     * histogram.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        // Start and cancel the Payment Request.
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_METHOD_BASIC_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
