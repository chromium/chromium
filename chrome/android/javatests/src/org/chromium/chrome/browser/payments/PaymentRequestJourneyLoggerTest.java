// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DECEMBER;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.FIRST_BILLING_ADDRESS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NEXT_YEAR;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NO_INSTRUMENTS;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestJourneyLoggerTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_metrics_test.html", this);

    @Override
    public void onMainActivityStarted() {}

    private void createTestData() throws TimeoutException {
        AutofillTestHelper mHelper = new AutofillTestHelper();
        // The user has a shipping address and a credit card associated with that address on disk.
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "jondoe@email.com", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));
        // The user also has an incomplete address and an incomplete card saved.
        String mIncompleteAddressId = mHelper.setProfile(new AutofillProfile("",
                "https://example.com", true, "In Complete", "Google", "344 Main St", "CA", "", "",
                "90291", "", "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "",
                "4111111111111111", "1111", "18", "2075", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mIncompleteAddressId, "" /* serverId */));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @RetryOnFailure
    public void testNumberOfSuggestionsShown_ShippingAddress_Completed() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.Completed"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.Completed.Shown"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.Completed.Shown.BasicCard"));

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_AbortedByUser()
            throws InterruptedException, TimeoutException {
        createTestData();

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.UserAborted"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "PaymentRequest.TimeToCheckout.UserAborted.Shown"));

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.UserAborted", 0));
    }

    /**
     * Expect that the NumberOfSelectionEdits histogram gets logged properly for shipping addresses.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionEdits_ShippingAddress_Completed() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Select the incomplete address and edit it.
        mPaymentRequestTestRule.clickOnShippingAddressSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"In Complete", "Google", "344 Main St", "CA", "Los Angeles"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the edit was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 1));

        // Since the edit was not for the default selection a change should be logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 1));

        // Make sure no add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the NumberOfSelectionAdds histogram gets logged properly for shipping addresses.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionAdds_ShippingAddress_Completed() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Add a new shipping address.
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setSpinnerSelectionInEditorAndWait(
                0 /* Afghanistan */, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                        "Alice", "Supreme Court", "Airport Road", "Kabul", "1043", "020-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Complete the transaction.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 1));

        // Make sure no edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));
    }

    /**
     * Expect that the number of payment method suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_Completed() throws TimeoutException {
        // Add two credit cards.
        createTestData();

        // Add a complete payment app.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Complete a Payment Request with the payment app.
        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed", 3));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.PaymentMethod.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.PaymentMethod.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.PaymentMethod.Completed", 0));
    }

    /**
     * Expect that the number of payment method suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_AbortedByUser()
            throws InterruptedException, TimeoutException {
        // Add two credit cards.
        createTestData();

        // Add a complete payment app.
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted", 3));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.PaymentMethod.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.PaymentMethod.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.PaymentMethod.UserAborted", 0));
    }

    /**
     * Expect that an incomplete payment app is not suggested to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_InvalidPaymentApp()
            throws InterruptedException, TimeoutException {
        // Add two credit cards.
        createTestData();

        // Add an incomplete payment app.
        mPaymentRequestTestRule.installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        Thread.sleep(200);

        // Make sure only the two credit card suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted", 2));
    }

    /**
     * Expect that the NumberOfSelectionAdds histogram gets logged properly for payment methods.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionAdds_PaymentMethod_Completed() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());

        // Add a new credit card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
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

        // Make sure the add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.PaymentMethod.Completed", 1));

        // Make sure no edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.PaymentMethod.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.PaymentMethod.Completed", 0));
    }

    /**
     * Expect that the number of contact info suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ContactInfo_Completed() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ContactInfo.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ContactInfo.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ContactInfo.Completed", 0));
    }

    /**
     * Expect that the number of contact info suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ContactInfo_AbortedByUser()
            throws InterruptedException, TimeoutException {
        createTestData();

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.UserAborted", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ContactInfo.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ContactInfo.UserAborted", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ContactInfo.UserAborted", 0));
    }

    /**
     * Expect that the NumberOfSelectionEdits histogram gets logged properly for contact info.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionEdits_ContactInfo_Completed() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Select the incomplete contact info and edit it.
        mPaymentRequestTestRule.clickOnContactInfoSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"In Complete", "514-123-1234", "test@email.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the edit was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ContactInfo.Completed", 1));

        // Since the edit was not for the default selection a change should be logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ContactInfo.Completed", 1));

        // Make sure no add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ContactInfo.Completed", 0));
    }

    /**
     * Expect that the NumberOfSelectionAdds histogram gets logged properly for contact info.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSelectionAdds_ContactInfo_Completed() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Add a new shipping address.
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Alice", "020-253-0000", "test@email.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Complete the transaction.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the add was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ContactInfo.Completed", 1));

        // Make sure no edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ContactInfo.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ContactInfo.Completed", 0));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_ShippingAndPayment() throws TimeoutException {
        // Add two addresses and two cards.
        createTestData();

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserDidNotHaveCompleteSuggestions_ShippingAndPayment_IncompleteShipping()
            throws TimeoutException {
        // Add a card and an incomplete address (no region).
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", /*region=*/"", "Los Angeles", "", "90291",
                "", "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly. Since the added credit card is using the same
        // incomplete profile for billing address, NEEDS_COMPLETION_PAYMENT is also set.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_BASIC_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_PAYMENT
                | Event.NEEDS_COMPLETION_SHIPPING;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserDidNotHaveCompleteSuggestions_ShippingAndPayment_IncompleteCard()
            throws TimeoutException {
        // Add an incomplete card (no exp date) and an complete address.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true,
                /*cardholderName=*/"", "4111111111111111", "1111", "10", "2021", "visa",
                R.drawable.visa_card, CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_BASIC_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testUserDidNotHaveCompleteSuggestions_ShippingAndPayment_UnsupportedCard()
            throws TimeoutException {
        // Add an unsupported card (mastercard) and an complete address.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "5187654321098765", "8765", "10", "2021", "mastercard", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testUserDidNotHaveCompleteSuggestions_ShippingAndPayment_OnlyPaymentApp()
            throws TimeoutException {
        // Add a complete address and a working payment app.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        mHelper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000", "",
                "en-US"));
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testUserDidNotHaveCompleteSuggestions_PaymentApp_NoInstruments()
            throws TimeoutException {
        // Add an address and a payment app without instruments on file.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        mHelper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000", "",
                "en-US"));
        mPaymentRequestTestRule.installPaymentApp(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.REQUEST_METHOD_OTHER
                | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_PaymentApp_HasValidPaymentApp()
            throws TimeoutException {
        // Add an address and a payment app on file.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        mHelper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000", "",
                "en-US"));
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.REQUEST_METHOD_OTHER
                | Event.AVAILABLE_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_ShippingAndPaymentApp_HasInvalidShipping()
            throws TimeoutException {
        // Add a card and an incomplete address (no region).
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", /*region=*/"", "Los Angeles", "", "90291",
                "", "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly. Since the added credit card is using the same
        // incomplete profile, NEEDS_COMPLETION_PAYMENT is also set.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.NEEDS_COMPLETION_PAYMENT
                | Event.AVAILABLE_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_SHIPPING;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the UserHadCompleteSuggestions histogram gets logged properly when the user has
     * at least one credit card on file.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_AcceptsCardsAndApps_UserHasOnlyCard()
            throws TimeoutException {
        // Add an address and a credit card on file.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));

        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyToPay());

        // The user cancels the Payment Request (trigger the logs).
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.REQUEST_METHOD_OTHER
                | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the UserHadCompleteSuggestions histogram gets logged properly when the user has
     * at least one payment app on file.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_AcceptsCardsAndApps_UserHasOnlyPaymentApp()
            throws TimeoutException {
        // Add an address and a payment app on file.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        mHelper.setProfile(new AutofillProfile("", "https://example.com", true, "Jon Doe", "Google",
                "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000", "",
                "en-US"));
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyToPay());

        // The user cancels the Payment Request (trigger the logs).
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.REQUEST_METHOD_OTHER
                | Event.AVAILABLE_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the UserHadCompleteSuggestions histogram gets logged properly when the user has
     * at both a card and a payment app on file.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_AcceptsCardsAndApps_UserHasCardAndPaymentApp()
            throws TimeoutException {
        // Add an address, a credit card and a payment app on file.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "", "en-US"));
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);

        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyToPay());

        // The user cancels the Payment Request (trigger the logs).
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.REQUEST_METHOD_OTHER
                | Event.AVAILABLE_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the UserDidNotHaveInitialFormOfPayment histogram gets logged properly when the
     * user has no form of payment on file.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @CommandLineFlags.Add("disable-features=NoCreditCardAbort")
    public void testUserDidNotHaveCompleteSuggestions_AcceptsCardsAndApps_NoCardOrPaymentApp()
            throws TimeoutException {
        // Add an address on file.
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://example.com", true,
                "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "650-253-0000", "", "en-US"));

        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyForInput());

        // The user cancels the Payment Request (trigger the logs).
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.REQUEST_METHOD_OTHER
                | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that no metric for contact info has been logged.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoContactInfoHistogram() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure nothing was logged for contact info.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ContactInfo.Completed", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ContactInfo.Completed", 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ContactInfo.Completed", 0));
    }

    /**
     * Expect that that the journey metrics are logged correctly on a second consecutive payment
     * request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testTwoTimes() throws TimeoutException {
        createTestData();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));

        // Complete a second Payment Request with a credit card.
        mPaymentRequestTestRule.reTriggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));

        // Make sure no adds, edits or changes were logged.
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionAdds.ShippingAddress.Completed", 0));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionChanges.ShippingAddress.Completed", 0));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSelectionEdits.ShippingAddress.Completed", 0));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.COMPLETED | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.PAY_CLICKED | Event.AVAILABLE_METHOD_BASIC_CARD
                | Event.SELECTED_CREDIT_CARD;
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that only some journey metrics are logged if the payment request was not shown to the
     * user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoShow() throws TimeoutException {
        // Android Pay is supported but no instruments are present.
        mPaymentRequestTestRule.installPaymentApp(
                "https://android.com/pay", NO_INSTRUMENTS, DELAYED_RESPONSE);
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "androidPayBuy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Payment method not supported"});

        // Make sure that no journey metrics were logged.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.OtherAborted", 2));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));
    }
}
