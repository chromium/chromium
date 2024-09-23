// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.Event2;
import org.chromium.components.payments.PaymentFeatureList;

import java.util.concurrent.TimeoutException;

/** A payment integration test to validate the logging of Payment Request metrics. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestMetricsTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_metrics_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        String mBillingAddressId =
                mHelper.setProfile(
                        AutofillProfile.builder()
                                .setFullName("Jon Doe")
                                .setCompanyName("Google")
                                .setStreetAddress("340 Main St")
                                .setRegion("CA")
                                .setLocality("Los Angeles")
                                .setPostalCode("90291")
                                .setCountryCode("US")
                                .setPhoneNumber("650-253-0000")
                                .setLanguageCode("en-US")
                                .build());
        mHelper.setCreditCard(
                new CreditCard(
                        "",
                        "https://example.test",
                        true,
                        "Jon Doe",
                        "4111111111111111",
                        "1111",
                        "12",
                        "2050",
                        "visa",
                        R.drawable.visa_card,
                        mBillingAddressId,
                        /* serverId= */ ""));
    }

    /**
     * Expect that the successful checkout funnel metrics are logged during a successful checkout.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuccessCheckoutFunnel() throws TimeoutException {
        // Install the apps so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());

        // Click the pay button.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user cancels a
     * Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAbortMetrics_AbortedByUser_CancelButton() throws TimeoutException {
        // Install the apps so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());

        // Cancel the Payment Request.
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable)
                        () -> {
                            // Click "Edit" to expand the UI.
                            mPaymentRequestTestRule
                                    .getPaymentRequestUI()
                                    .getDialogForTest()
                                    .findViewById(R.id.button_secondary)
                                    .performClick();
                            ((Runnable)
                                            () ->
                                                    // Click "Cancel" to dismiss the UI.
                                                    mPaymentRequestTestRule
                                                            .getPaymentRequestUI()
                                                            .getDialogForTest()
                                                            .findViewById(R.id.button_secondary)
                                                            .performClick())
                                    .run();
                        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_METHOD_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user presses on
     * the [X] button in the Payment Request dialog.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAbortMetrics_AbortedByUser_XButton() throws TimeoutException {
        // Install the apps so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());

        // Press the [X] button.
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_METHOD_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user presses on
     * the back button on their phone during a Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAbortMetrics_AbortedByUser_BackButton() throws TimeoutException {
        // Install the apps so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());

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

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_METHOD_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_USER enum value gets logged when a user closes the
     * tab during a Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAbortMetrics_UserAborted_TabClosed() throws TimeoutException {
        // Install the apps so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(),
                mPaymentRequestTestRule.getActivity());

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_METHOD_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect only the ABORT_REASON_ABORTED_BY_MERCHANT enum value gets logged when a Payment
     * Request gets cancelled by the merchant.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAbortMetrics_AbortedByMerchant() throws TimeoutException {
        // Install the apps so the user can complete the Payment Request.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());

        // Simulate an abort by the merchant.
        mPaymentRequestTestRule.clickNodeAndWait("abort", mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Abort"});

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.OTHER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_SHIPPING
                        | Event2.REQUEST_METHOD_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
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
    public void testMetrics_NoMatchingPaymentMethod() throws TimeoutException {
        // Android Pay has a factory, but it returns no apps.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://android.com/pay", AppPresence.NO_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait(
                "androidPayBuy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"The payment method", "not supported"});

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.REQUEST_SHIPPING | Event2.REQUEST_METHOD_GOOGLE | Event2.COULD_NOT_SHOW;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect only the Event.SELECTED_GOOGLE enum value to be logged for the events histogram when
     * completing a Payment Request with Android Pay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSelectedPaymentMethod_AndroidPay() throws TimeoutException {
        // Complete a Payment Request with Android Pay.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://android.com/pay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.triggerUIAndWait(
                "androidPayBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_GOOGLE
                        | Event2.SELECTED_GOOGLE
                        | Event2.REQUEST_SHIPPING;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect that the SkippedShow metric is logged when the UI directly goes to the payment app UI
     * during a Payment Request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testMetrics_SkippedShow() throws TimeoutException {
        // Complete a Payment Request with Android Pay.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://android.com/pay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.triggerUIAndWait(
                "androidPaySkipUiBuy", mPaymentRequestTestRule.getResultReady());

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SKIPPED_SHOW
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_GOOGLE
                        | Event2.SELECTED_GOOGLE;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect that the PaymentRequest UI is shown even if all the requirements are met to skip, if
     * the skip feature is disabled.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add({
        "disable-features=" + PaymentFeatureList.WEB_PAYMENTS_SINGLE_APP_UI_SKIP
    })
    public void testMetrics_SkippedShow_Disabled() throws TimeoutException {
        // Complete a Payment Request with Android Pay.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://android.com/pay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.triggerUIAndWait(
                "androidPaySkipUiBuy", mPaymentRequestTestRule.getReadyToPay());

        // Close the payment Request.
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_GOOGLE;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }

    /**
     * Expect that the "Shown" event is recorded only once.
     *
     * <p>TODO(crbug.com/40182225): Will need ported away from basic-card before being enabled
     * again.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @DisabledTest(
            message =
                    "crbug.com/1260121 - The test deterministically fails in local run. "
                            + "Efforts are needed to fix the test or the implementation code.")
    public void testShownLoggedOnlyOnce() throws TimeoutException {
        // Initiate a payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());

        // Add a shipping address, which triggers a second "Show".
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                    "Seb Doe", "Google", "340 Main St", "Los Angeles", "CA", "90291", "650-253-0000"
                },
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Close the payment Request.
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample =
                Event2.SHOWN
                        | Event2.USER_ABORTED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_METHOD_BASIC_CARD
                        | Event2.SELECTED_CREDIT_CARD;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }
}
