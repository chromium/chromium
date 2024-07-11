// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.payments.Event2;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/** A payment integration test for the correct log of the CanMakePayment metrics. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestCanMakePaymentMetricsTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_can_make_payment_metrics_test.html");

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant calling
     * it, receiving no as a response, still showing the Payment Request and the user aborts the
     * flow.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=PaymentRequestBasicCard"})
    public void testCannotMakePayment_UserAbort() throws TimeoutException {
        // Install the apps so CanMakePayment returns true.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShowWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPaymentRequestTestRule
                                .getPaymentRequestUI()
                                .getDialogForTest()
                                .onBackPressed());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the canMakePayment events were logged correctly.
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

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant calling
     * it, receiving no as a response, still showing the Payment Request and the user completes the
     * flow.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    @CommandLineFlags.Add({"disable-features=PaymentRequestBasicCard"})
    public void testCannotMakePayment_Complete() throws TimeoutException {
        // Install the apps so CanMakePayment returns true.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShowWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());

        // Add a new credit card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyToEdit());
        // TODO(crbug.com/40182225): This test will also need migrated away from basic-card before
        // being re-enabled.
        // mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
        //         new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
        //         mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        // mPaymentRequestTestRule.setTextInCardEditorAndWait(
        //         new String[] {"4111111111111111", "Jon Doe"},
        //         mPaymentRequestTestRule.getEditorTextUpdate());
        // mPaymentRequestTestRule.clickInCardEditorAndWait(
        //         R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Complete the transaction.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_CREDIT_CARD;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant calling
     * it, receiving yes as a response, showing the Payment Request and the merchant aborts the
     * flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_MerchantAbort() throws TimeoutException {
        // Install the apps so CanMakePayment returns true.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShowWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());

        // Simulate an abort by the merchant.
        mPaymentRequestTestRule.clickNodeAndWait("abort", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Abort"});

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.OTHER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant calling
     * it, receiving yes as a response, showing the Payment Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_Complete() throws TimeoutException {
        // Install the apps so CanMakePayment returns true.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Initiate and complete a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShowWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant calling
     * it, receiving false as a response because can make payment is disabled, showing the Payment
     * Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePaymentDisabled_Complete() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefs =
                            UserPrefs.get(
                                    Profile.fromWebContents(
                                            mPaymentRequestTestRule
                                                    .getActivity()
                                                    .getCurrentWebContents()));
                    prefs.setBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED, false);
                });

        // Install the apps so CanMakePayment returns true if it is enabled.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Initiate an complete a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShowWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant not
     * calling it but still showing the Payment Request and the user aborts the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_UserAbort() throws TimeoutException {
        // Install the apps so CanMakePayment returns true.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShowWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPaymentRequestTestRule
                                .getPaymentRequestUI()
                                .getDialogForTest()
                                .onBackPressed());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure no canMakePayment events were logged.
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

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant not
     * calling it but still showing the Payment Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_Complete() throws TimeoutException {
        // Install the apps so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShowWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure no canMakePayment events were logged.
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }
}
