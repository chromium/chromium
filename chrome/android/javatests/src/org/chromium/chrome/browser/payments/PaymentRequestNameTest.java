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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.Event;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payer name.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNameTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_name_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a valid payer name on disk.
        String billingAddressId = helper.setProfile(
                new AutofillProfile("", "https://example.test", true, "" /* honorific prefix */,
                        "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                        "US", "555-555-5555", "jon.doe@google.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.test", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));

        // Add the same profile but with a different address.
        helper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "", "Google", "999 Main St", "CA", "Los Angeles", "",
                "90291", "", "US", "555-555-5555", "jon.doe@google.com", "en-US"));

        // Add the same profile but without a phone number.
        helper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "" /* phone_number */, "jon.doe@google.com", "en-US"));

        // Add the same profile but without an email.
        helper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "555-555-5555", "" /* emailAddress */, "en-US"));

        // Add the same profile but without a name.
        helper.setProfile(new AutofillProfile("" /* name */, "https://example.test", true,
                "" /* honorific prefix */, "", "Google", "340 Main St", "CA", "Los Angeles", "",
                "90291", "", "US", "555-555-5555", "jon.doe@google.com", "en-US"));
    }

    /** Provide the existing valid payer name to the merchant. */
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
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon Doe"});
    }

    /** Attempt to add an invalid payer name and cancel the transaction. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testAddInvalidNameAndCancel() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {""}, mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Add a new payer name and provide that to the merchant. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testAddNameAndPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Jones"}, mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jane Jones"});
    }

    /**
     * Makes sure that suggestions that are equal to or subsets of other suggestions are not shown
     * to the user.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testSuggestionsDeduped() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
    }

    /**
     * Test that ending a payment request that requires only the user's payer name results in
     * the appropriate metric being logged in PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        // Start and complete the Payment Request.
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon Doe"});

        int expectedSample = Event.SHOWN | Event.PAY_CLICKED | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.COMPLETED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_PAYER_NAME
                | Event.REQUEST_METHOD_BASIC_CARD | Event.SELECTED_CREDIT_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
