// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NO_INSTRUMENTS;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestMetricsTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_metrics_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));
    }

    // Transaction amount from metrics.js is 5$ which falls into the regular transaction category.
    private static final int sRegularTransaction = 2;

    /**
     * Expect that the successful checkout funnel metrics are logged during a succesful checkout.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testSuccessCheckoutFunnel() throws TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered", sRegularTransaction));

        // Click the pay button.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());

        // Unmask the credit card,
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed", sRegularTransaction));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.SELECTED_CREDIT_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user cancels a
     * Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testAbortMetrics_AbortedByUser_CancelButton() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered", sRegularTransaction));

        // Cancel the Payment Request.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> mPaymentRequestTestRule.getPaymentRequestUI()
                                   .getDialogForTest()
                                   .findViewById(R.id.button_secondary)
                                   .performClick());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(AbortReason.ABORTED_BY_USER);

        // Make sure no PaymentRequest.TransactionAmount.Completed is logged since transaction
        // got aborted.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed"));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user presses on
     * the [X] button in the Payment Request dialog.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testAbortMetrics_AbortedByUser_XButton() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered", sRegularTransaction));

        // Press the [X] button.
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(AbortReason.ABORTED_BY_USER);

        // Make sure no PaymentRequest.TransactionAmount.Completed is logged since transaction
        // got aborted.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed"));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user presses on
     * the back button on their phone during a Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testAbortMetrics_AbortedByUser_BackButton() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered", sRegularTransaction));

        // Press the back button.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPaymentRequestTestRule.getPaymentRequestUI()
                                   .getDialogForTest()
                                   .onBackPressed());
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(AbortReason.ABORTED_BY_USER);

        // Make sure no PaymentRequest.TransactionAmount.Completed is logged since transaction
        // got aborted.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed"));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_MOJO_RENDERER_CLOSING enum value gets logged when a user closes
     * the tab during a Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    @DisabledTest(message = "Test is flaky, see crbug.com/968797")
    public void testAbortMetrics_OtherAborted_TabClosed() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered", sRegularTransaction));

        // Press the back button.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(),
                mPaymentRequestTestRule.getActivity());

        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(
                AbortReason.MOJO_RENDERER_CLOSING);

        // Make sure no PaymentRequest.TransactionAmount.Completed is logged since transaction
        // got aborted.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed"));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.OTHER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_MERCHANT enum value gets logged when a Payment
     * Request gets cancelled by the merchant.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testAbortMetrics_AbortedByMerchant() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered", sRegularTransaction));

        // Simulate an abort by the merchant.
        mPaymentRequestTestRule.clickNodeAndWait("abort", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Abort"});

        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(
                AbortReason.ABORTED_BY_MERCHANT);

        // Make sure no PaymentRequest.TransactionAmount.Completed is logged since transaction
        // got aborted.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed"));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.OTHER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect no abort metrics to be logged even if there are no matching payment methods because
     * the Payment Request was not shown to the user (a Payment Request gets cancelled because the
     * user does not have any of the payment methods accepted by the merchant and the merchant does
     * not accept credit cards). It should instead be logged as a reason why the Payment Request was
     * not shown to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testMetrics_NoMatchingPaymentMethod() throws TimeoutException {
        // Android Pay is supported but no instruments are present.
        mPaymentRequestTestRule.installPaymentApp(
                "https://android.com/pay", NO_INSTRUMENTS, DELAYED_RESPONSE);
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "androidPayBuy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Payment method not supported"});

        // Make sure that it is not logged as an abort.
        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(-1 /* none */);
        // Make sure that it was logged as a reason why the Payment Request was not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CheckoutFunnel.NoShow",
                        NotShownReason.NO_MATCHING_PAYMENT_METHOD));

        // Make sure no PaymentRequest.TransactionAmount.[Completed|Triggered] is logged since
        // we could not trigger the payment sheet.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed"));

        // Make sure the events were logged correctly.
        int expectedSample =
                Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_GOOGLE | Event.COULD_NOT_SHOW;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect no abort metrics to be logged even if there are no matching payment methods because
     * the Payment Request was not shown to the user (a Payment Request gets cancelled because the
     * merchant only accepts payment methods we don't support. It should instead be logged as a
     * reason why the Payment Request was not shown to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testMetrics_NoSupportedPaymentMethod() throws TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "noSupported", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Payment method not supported"});

        // Make sure that it is not logged as an abort.
        mPaymentRequestTestRule.assertOnlySpecificAbortMetricLogged(-1 /* none */);
        // Make sure that it was logged as a reason why the Payment Request was not shown.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.CheckoutFunnel.NoShow",
                        NotShownReason.NO_SUPPORTED_PAYMENT_METHOD));

        // Make sure no PaymentRequest.TransactionAmount.[Completed|Triggered] is logged since
        // we could not trigger the payment sheet.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Triggered"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TransactionAmount.Completed"));

        // Make sure the events were logged correctly.
        int expectedSample =
                Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_OTHER | Event.COULD_NOT_SHOW;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect only the Event.SELECTED_CREDIT_CARD enum value to be logged for the events histogram
     * when completing a Payment Request with a credit card.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testSelectedPaymentMethod_CreditCard() throws TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.SELECTED_CREDIT_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect only the Event.SELECTED_GOOGLE enum value to be logged for the events histogram
     * when completing a Payment Request with Android Pay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testSelectedPaymentMethod_AndroidPay() throws TimeoutException {
        // Complete a Payment Request with Android Pay.
        mPaymentRequestTestRule.installPaymentApp(
                "https://android.com/pay", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(
                "androidPayBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_METHOD_GOOGLE
                | Event.SELECTED_GOOGLE | Event.REQUEST_SHIPPING | Event.AVAILABLE_METHOD_GOOGLE;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the SkippedShow metric is logged when the UI directly goes
     * to the payment app UI during a Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testMetrics_SkippedShow() throws TimeoutException {
        // Complete a Payment Request with Android Pay.
        mPaymentRequestTestRule.installPaymentApp(
                "https://android.com/pay", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(
                "androidPaySkipUiBuy", mPaymentRequestTestRule.getResultReady());

        // Make sure the events were logged correctly.
        int expectedSample = Event.SKIPPED_SHOW | Event.PAY_CLICKED
                | Event.RECEIVED_INSTRUMENT_DETAILS | Event.COMPLETED
                | Event.HAD_INITIAL_FORM_OF_PAYMENT | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS
                | Event.REQUEST_METHOD_GOOGLE | Event.SELECTED_GOOGLE
                | Event.AVAILABLE_METHOD_GOOGLE;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.Completed"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.Completed.SkippedShow"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.Completed.SkippedShow.Google"));
    }

    /**
     * Expect that the PaymentRequest UI is shown even if all the requirements are met to skip, if
     * the skip feature is disabled.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    @CommandLineFlags.Add({"disable-features=" + ChromeFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP})
    public void testMetrics_SkippedShow_Disabled() throws TimeoutException {
        // Complete a Payment Request with Android Pay.
        mPaymentRequestTestRule.installPaymentApp(
                "https://android.com/pay", HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(
                "androidPaySkipUiBuy", mPaymentRequestTestRule.getReadyToPay());

        // Close the payment Request.
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_METHOD_GOOGLE
                | Event.AVAILABLE_METHOD_GOOGLE;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the "Shown" event is recorded only once.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @DisabledTest(message = "Flaky. See crbug.com/727558")
    public void testShownLoggedOnlyOnce() throws TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());

        // Add a shipping address, which triggers a second "Show".
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles", "CA", "90291",
                        "650-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Close the payment Request.
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_METHOD_BASIC_CARD
                | Event.SELECTED_CREDIT_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
