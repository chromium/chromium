// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.Event;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that does not require shipping address.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNoShippingTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_no_shipping_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(
                new AutofillProfile("", "https://example.com", true, "" /* honorific prefix */,
                        "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                        "US", "650-253-0000", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    /** Click [X] to cancel payment. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testCloseDialog() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
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
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
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
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
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
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
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
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
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

    /** Quickly pressing on [X] and then "add card" should not crash. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickCloseAndAddCardShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press on [X] and then "add card."
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.close_button)
                    .performClick();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getPaymentMethodSectionForTest()
                    .findViewById(R.id.payments_add_option_button)
                    .performClick();
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Quickly pressing on "cancel" and then "add card" should not crash. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickCancelAndAddCardShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press on "cancel" and then "add card."
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.button_secondary)
                    .performClick();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getPaymentMethodSectionForTest()
                    .findViewById(R.id.payments_add_option_button)
                    .performClick();
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * "pay" should not crash.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickDismissAndPayShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        // Quickly dismiss and then press on "pay."
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI().getDialogForTest().onBackPressed();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.button_primary)
                    .performClick();
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /**
     * Quickly dismissing the dialog (via Android's back button, for example) and then pressing on
     * [X] should not crash.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickDismissAndCloseShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

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

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /**
     * Quickly pressing on [X] and then dismissing the dialog (via Android's back button, for
     * example) should not crash.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickCloseAndDismissShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

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

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
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
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
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
