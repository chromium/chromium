// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NO_INSTRUMENTS;

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

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay or cards.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPaymentAppAndCardsTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_bobpay_and_cards_test.html", this);

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
        // Mastercard card without a billing address.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "5454545454545454", "", "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.UNKNOWN, "" /* billingAddressId */, "" /* serverId */));
        // Visa card with complete set of information.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    /**
     * If Bob Pay does not have any instruments, show [visa, mastercard]. Here the payment app
     * responds quickly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInFastBobPay() throws TimeoutException {
        runTest(NO_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /**
     * If Bob Pay does not have any instruments, show [visa, mastercard]. Here the payment app
     * responds slowly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoInstrumentsInSlowBobPay() throws TimeoutException {
        runTest(NO_INSTRUMENTS, DELAYED_RESPONSE);
    }

    /**
     * If Bob Pay has instruments, show [bobpay, visa, mastercard]. Here the payment app responds
     * quickly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHaveInstrumentsInFastBobPay() throws TimeoutException {
        runTest(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /**
     * If Bob Pay has instruments, show [bobpay, visa, mastercard]. Here the payment app responds
     * slowly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHaveInstrumentsInSlowBobPay() throws TimeoutException {
        runTest(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
    }

    /** Test that going into the editor and cancelling will leave the row checked. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditPaymentMethodAndCancelEditorShouldKeepCardSelected()
            throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the editor.
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());

        // Expect the existing row to still be selected in the Shipping Address section.
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
    }

    /** Test that going into "add" flow editor and cancelling will leave existing row checked. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddPaymentMethodAndCancelEditorShouldKeepExistingCardSelected()
            throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the editor.
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());

        // Expect the row to still be selected in the Shipping Address section.
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
    }

    private void runTest(int instrumentPresence, int responseSpeed) throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(instrumentPresence, responseSpeed);
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Check the number of instruments.
        Assert.assertEquals(instrumentPresence == HAVE_INSTRUMENTS ? 3 : 2,
                mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        // Check the labels of the instruments.
        int i = 0;
        if (instrumentPresence == HAVE_INSTRUMENTS) {
            Assert.assertEquals(
                    "https://bobpay.com", mPaymentRequestTestRule.getPaymentInstrumentLabel(i++));
        }
        // \u0020\...\u2060 is four dots ellipsis, \u202A is the Left-To-Right Embedding (LTE) mark,
        // \u202C is the Pop Directional Formatting (PDF) mark. Expected string with form
        // 'Visa  <LRE>****1111<PDF>\nJoe Doe'.

        Assert.assertEquals(
                "Visa\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060\u2022"
                        + "\u2060\u2006\u2060\u2022\u2060\u2006\u20601111\u202C\nJon Doe",
                mPaymentRequestTestRule.getPaymentInstrumentLabel(i++));
        // Expected string with form
        // 'Visa  <LRE>****5454<PDF>\nJoe Doe\nBilling address required'.
        Assert.assertEquals(
                "Mastercard\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060"
                        + "\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u20605454\u202C\nJon Doe\n"
                        + "Billing address required",
                mPaymentRequestTestRule.getPaymentInstrumentLabel(i++));

        // Check the output of the selected instrument.
        if (instrumentPresence == HAVE_INSTRUMENTS) {
            mPaymentRequestTestRule.clickAndWait(
                    R.id.button_primary, mPaymentRequestTestRule.getDismissed());
            mPaymentRequestTestRule.expectResultContains(
                    new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
        } else {
            mPaymentRequestTestRule.clickAndWait(
                    R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
            mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                    R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
            mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                    ModalDialogProperties.ButtonType.POSITIVE,
                    mPaymentRequestTestRule.getDismissed());
            mPaymentRequestTestRule.expectResultContains(new String[] {
                    "Jon Doe", "4111111111111111", "12", "2050", "basic-card", "123"});
        }
    }
}
