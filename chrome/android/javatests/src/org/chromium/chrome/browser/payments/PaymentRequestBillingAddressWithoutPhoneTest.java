// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DECEMBER;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.FIRST_BILLING_ADDRESS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NEXT_YEAR;

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
 * A payment integration test for biling addresses without a phone.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestBillingAddressWithoutPhoneTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_free_shipping_test.html", this);

    /*
     * The index at which the option to add a billing address is located in the billing address
     * selection dropdown.
     */
    private static final int ADD_BILLING_ADDRESS = 3;

    /** The index of the billing address dropdown in the card editor. */
    private static final int BILLING_ADDRESS_DROPDOWN_INDEX = 2;

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String address_without_phone = helper.setProfile(new AutofillProfile("",
                "https://example.com", true, "Jon NoPhone", "Google", "340 Main St", "CA",
                "Los Angeles", "", "90291", "", "US", "", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "amex", R.drawable.amex_card,
                CardType.UNKNOWN, address_without_phone, "" /* serverId */));
        String address_with_phone = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Rob Phone", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));

        // Assign use stats so that the address without a phone number has a higher frecency score.
        helper.setProfileUseStatsForTesting(address_without_phone, 10, 10);
        helper.setProfileUseStatsForTesting(address_with_phone, 5, 5);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanPayWithBillingNoPhone() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon NoPhone"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanSelectBillingAddressWithoutPhone() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        // Go edit the credit card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionEditIconAndWait(
                0, mPaymentRequestTestRule.getReadyToEdit());

        // Make sure that the currently selected address is valid and can be selected (does not
        // include error messages).
        Assert.assertTrue(
                mPaymentRequestTestRule
                        .getSpinnerSelectionTextInCardEditor(BILLING_ADDRESS_DROPDOWN_INDEX)
                        .equals("Jon NoPhone, 340 Main St, Los Angeles, CA 90291"));

        // Even though the current billing address is valid, the one with a phone number should be
        // suggested first (right after the select hint) if the user wants to change it.
        Assert.assertTrue(mPaymentRequestTestRule
                                  .getSpinnerTextAtPositionInCardEditor(
                                          BILLING_ADDRESS_DROPDOWN_INDEX, FIRST_BILLING_ADDRESS)
                                  .equals("Rob Phone, 340 Main St, Los Angeles, CA 90291"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCantSelectShippingAddressWithoutPhone() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // The first suggestion should be the address with a phone.
        Assert.assertTrue(
                mPaymentRequestTestRule.getShippingAddressSuggestionLabel(0).contains("Rob Phone"));
        Assert.assertFalse(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(0).endsWith(
                "Phone number required"));

        // The address without a phone should be suggested after with a message indicating that the
        // phone number is required.
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(1).contains(
                "Jon NoPhone"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(1).endsWith(
                "Phone number required"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCantAddNewBillingAddressWithoutPhone() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // Add a new billing address without a phone.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS},
                mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles", "CA", "90291", ""},
                mPaymentRequestTestRule.getEditorTextUpdate());
        // Trying to add the address without a phone number should fail.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
    }
}
