// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DECEMBER;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.FIRST_BILLING_ADDRESS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NEXT_YEAR;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.Event;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the correct log of the CanMakePayment metrics.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestCanMakePaymentMetricsTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_can_make_payment_metrics_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
    }

    public void addCreditCard() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(
                new AutofillProfile("", "https://example.com", true, "" /* honorific prefix */,
                        "John Smith", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                        "US", "310-310-6000", "john.smith@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving no as a response, still showing the Payment Request and the user aborts
     * the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=StrictHasEnrolledAutofillInstrument")
    public void testCannotMakePayment_UserAbort() throws TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPaymentRequestTestRule.getPaymentRequestUI()
                                   .getDialogForTest()
                                   .onBackPressed());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_FALSE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving no as a response, still showing the Payment Request and the user
     * completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=StrictHasEnrolledAutofillInstrument")
    public void testCannotMakePayment_Complete() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Add a new credit card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4111111111111111", "Jon Doe"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Complete the transaction.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_FALSE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_CREDIT_CARD
                | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yes as a response, showing the Payment Request and the merchant aborts
     * the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_MerchantAbort() throws TimeoutException {
        // Install the app so CanMakePayment returns true.
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Add credit card so that autofill payment app is also available and payment sheet UI gets
        // shown.
        addCreditCard();

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());

        // Simulate an abort by the merchant.
        mPaymentRequestTestRule.clickNodeAndWait("abort", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Abort"});

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.OTHER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_TRUE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.AVAILABLE_METHOD_OTHER
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving yes as a response, showing the Payment Request and the user completes
     * the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePayment_Complete() throws TimeoutException {
        // Install the app so CanMakePayment returns true.
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Add credit card so that autofill payment app is available and payment sheet UI gets
        // shown.
        addCreditCard();

        // Initiate an complete a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.CAN_MAKE_PAYMENT_TRUE
                | Event.HAS_ENROLLED_INSTRUMENT_TRUE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_OTHER | Event.AVAILABLE_METHOD_OTHER
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * calling it, receiving false as a response because can make payment is disabled, showing the
     * Payment Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanMakePaymentDisabled_Complete() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefs = UserPrefs.get(Profile.fromWebContents(
                    mPaymentRequestTestRule.getActivity().getCurrentWebContents()));
            prefs.setBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED, false);
        });

        // Install the app so CanMakePayment returns true if it is enabled.
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Add credit card so that autofill payment app is available and payment sheet UI gets
        // shown.
        addCreditCard();

        // Initiate an complete a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "queryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the canMakePayment events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.CAN_MAKE_PAYMENT_FALSE
                | Event.HAS_ENROLLED_INSTRUMENT_FALSE | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_OTHER | Event.AVAILABLE_METHOD_OTHER
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user aborts the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=StrictHasEnrolledAutofillInstrument")
    public void testNoQuery_UserAbort() throws TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShow", mPaymentRequestTestRule.getReadyForInput());

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPaymentRequestTestRule.getPaymentRequestUI()
                                   .getDialogForTest()
                                   .onBackPressed());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure no canMakePayment events were logged.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Tests that the CanMakePayment metrics are correctly logged for the case of a merchant
     * not calling it but still showing the Payment Request and the user completes the flow.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoQuery_Complete() throws TimeoutException {
        // Install the app so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Add credit card so that autofill payment app is available and payment sheet UI gets
        // shown.
        addCreditCard();

        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "noQueryShow", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure no canMakePayment events were logged.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.SELECTED_OTHER | Event.AVAILABLE_METHOD_OTHER
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
