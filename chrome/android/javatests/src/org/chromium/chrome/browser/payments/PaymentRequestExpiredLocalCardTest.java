// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.FIRST_BILLING_ADDRESS;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
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

import java.util.Calendar;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a user that pays with an expired local credit card.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestExpiredLocalCardTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("payment_request_free_shipping_test.html", this);

    AutofillTestHelper mHelper;
    String mCreditCardId;

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        mHelper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        String billingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        // Create an expired credit card
        mCreditCardId = mHelper.setCreditCard(new CreditCard("", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "1", "2016", "amex", R.drawable.amex_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    /**
     * Tests that the credit card unmask prompt includes inputs for a new expiration date if the
     * credit card is expired. Also tests that the user can pay once they have entered a new valid
     * expiration date and that merchant receives the updated data.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithExpiredCard_ValidExpiration() throws TimeoutException {
        mRule.triggerUIAndWait(mRule.getReadyToPay());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"11", "26", "123"}, mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());
        mRule.expectResultContains(new String[] {"Jon Doe", "4111111111111111", "11", "2026",
                "basic-card", "123", "Google", "340 Main St", "CA", "Los Angeles", "90291", "US",
                "en", "freeShippingOption"});
    }

    /**
     * Tests that the new expiration date entered in the unmasking prompt for an expired card
     * overwrites that card's old expiration date.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithExpiredCard_NewExpirationSaved() throws TimeoutException {
        mRule.triggerUIAndWait(mRule.getReadyToPay());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());
        mRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"11", "26", "123"}, mRule.getReadyToUnmask());
        mRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mRule.getDismissed());

        // Make sure the new expiration date was saved.
        CreditCard storedCard = mHelper.getCreditCard(mCreditCardId);
        Assert.assertEquals("11", storedCard.getMonth());
        Assert.assertEquals("2026", storedCard.getYear());
    }

    /**
     * Tests that it is not possible to add an expired card in a payment request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotAddExpiredCard() throws TimeoutException {
        // If the current date is in January skip this test. It is not possible to select an expired
        // date in the card editor in January.
        Calendar now = Calendar.getInstance();
        if (now.get(Calendar.MONTH) == 0) return;

        mRule.triggerUIAndWait(mRule.getReadyToPay());

        // Try to add an expired card.
        mRule.clickInPaymentMethodAndWait(R.id.payments_section, mRule.getReadyForInput());
        mRule.clickInPaymentMethodAndWait(R.id.payments_add_option_button, mRule.getReadyToEdit());
        // Set the expiration date to past month of the current year.
        mRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {now.get(Calendar.MONTH) - 1, 0, FIRST_BILLING_ADDRESS},
                mRule.getBillingAddressChangeProcessed());
        mRule.setTextInCardEditorAndWait(
                new String[] {"4111111111111111", "Jon Doe"}, mRule.getEditorTextUpdate());
        mRule.clickInCardEditorAndWait(
                R.id.editor_dialog_done_button, mRule.getEditorValidationError());

        // Set the expiration date to the current month of the current year.
        mRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {now.get(Calendar.MONTH), 0, FIRST_BILLING_ADDRESS},
                mRule.getExpirationMonthChange());

        mRule.clickInCardEditorAndWait(R.id.editor_dialog_done_button, mRule.getReadyToPay());
    }

    /**
     * Tests the different card unmask error messages for an expired card.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPromptErrorMessages() throws TimeoutException {
        // Click pay to get to the card unmask prompt.
        mRule.triggerUIAndWait(mRule.getReadyToPay());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());

        // Set valid arguments.
        mRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"10", "26", "123"}, mRule.getUnmaskValidationDone());
        Assert.assertTrue(mRule.getUnmaskPromptErrorMessage().equals(""));

        // Set an invalid expiration month.
        mRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"99", "25", "123"}, mRule.getUnmaskValidationDone());
        Assert.assertTrue(mRule.getUnmaskPromptErrorMessage().equals(
                "Check your expiration month and try again"));

        // Set an invalid expiration year.
        mRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"10", "14", "123"}, mRule.getUnmaskValidationDone());
        Assert.assertTrue(mRule.getUnmaskPromptErrorMessage().equals(
                "Check your expiration year and try again"));

        // If the current date is in January skip this test. It is not possible to select an expired
        // date in January.
        Calendar now = Calendar.getInstance();
        if (now.get(Calendar.MONTH) != 0) {
            String twoDigitsYear = Integer.toString(now.get(Calendar.YEAR)).substring(2);

            // Set an invalid expiration date. The year is current, but the month is previous.
            // now.get(Calendar.MONTH) returns 0-indexed values (January is 0), but the unmask
            // dialog expects 1-indexed values (January is 1). Therefore, using
            // now.get(Calendar.MONTH) directly will result in using the previous month and no
            // subtraction is needed here.
            mRule.setTextInExpiredCardUnmaskDialogAndWait(
                    new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                    new String[] {Integer.toString(now.get(Calendar.MONTH)), twoDigitsYear, "123"},
                    mRule.getUnmaskValidationDone());
            Assert.assertTrue(mRule.getUnmaskPromptErrorMessage().equals(
                    "Check your expiration date and try again"));
        }

        // Set valid arguments again.
        mRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"10", "26", "123"}, mRule.getUnmaskValidationDone());
        Assert.assertTrue(mRule.getUnmaskPromptErrorMessage().equals(""));
    }

    /**
     * Tests that hitting "submit" on the software keyboard in the CVC number field will submit the
     * CVC unmask dialog.
     */
    @MediumTest
    @Feature({"Payments"})
    @Test
    public void testSoftwareKeyboardSubmitInCvcNumberField() throws TimeoutException {
        mRule.triggerUIAndWait(mRule.getReadyToPay());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());

        // Set valid arguments.
        mRule.setTextInExpiredCardUnmaskDialogAndWait(
                new int[] {R.id.expiration_month, R.id.expiration_year, R.id.card_unmask_input},
                new String[] {"10", "26", "123"}, mRule.getReadyToUnmask());

        mRule.hitSoftwareKeyboardSubmitButtonAndWait(R.id.card_unmask_input, mRule.getDismissed());
    }

    /**
     * Tests that hitting "submit" on the software keyboard in the CVC number field with no CVC set
     * will not submit the CVC unmask dialog.
     */
    @MediumTest
    @Feature({"Payments"})
    @Test
    public void testNoSoftwareKeyboardSubmitInCvcNumberFieldIfInvalid() throws TimeoutException {
        mRule.triggerUIAndWait(mRule.getReadyToPay());
        mRule.clickAndWait(R.id.button_primary, mRule.getReadyForUnmaskInput());

        mRule.hitSoftwareKeyboardSubmitButtonAndWait(
                R.id.card_unmask_input, mRule.getSubmitRejected());
    }
}
