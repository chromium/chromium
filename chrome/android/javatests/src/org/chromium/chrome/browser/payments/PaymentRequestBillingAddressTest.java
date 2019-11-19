// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.endsWith;

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

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for biling addresses.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestBillingAddressTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_free_shipping_test.html", this);

    /**
     * The index at which the option to add a billing address is located in the billing address
     * selection dropdown.
     */
    private static final int ADD_BILLING_ADDRESS = 8;

    /** The index of the billing address dropdown in the card editor. */
    private static final int BILLING_ADDRESS_DROPDOWN_INDEX = 2;

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String profile1 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "650-253-0000", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "amex", R.drawable.amex_card,
                CardType.UNKNOWN, profile1, "" /* serverId */));
        String profile2 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Rob Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "650-253-0000", "jon.doe@gmail.com", "en-US"));
        String profile3 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Tom Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "650-253-0000", "jon.doe@gmail.com", "en-US"));

        // Incomplete profile (invalid address).
        String profile4 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Bart Doe", "Google", "340 Main St", "CA", "", "", "90291", "", "US",
                "650-253-0000", "jon.doe@gmail.com", "en-US"));

        // Incomplete profile (missing phone number)
        String profile5 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Lisa Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "",
                "jon.doe@gmail.com", "en-US"));

        // Incomplete profile (missing recipient name).
        String profile6 = helper.setProfile(new AutofillProfile("", "https://example.com", true, "",
                "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000",
                "jon.doe@gmail.com", "en-US"));

        // Incomplete profile (need more information).
        String profile7 = helper.setProfile(new AutofillProfile("", "https://example.com", true, "",
                "Google", "340 Main St", "CA", "", "", "90291", "", "US", "", "", "en-US"));

        // Profile with empty street address (should not be presented to user).
        String profile8 = helper.setProfile(new AutofillProfile("", "https://example.com", true,
                "Jerry Doe", "Google", "" /* streetAddress */, "CA", "Los Angeles", "", "90291", "",
                "US", "650-253-0000", "jerry.doe@gmail.com", "en-US"));

        // This card has no billing address selected.
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jane Doe",
                "4242424242424242", "1111", "12", "2050", "amex", R.drawable.amex_card,
                CardType.UNKNOWN, profile6, "" /* serverId */));

        // Assign use stats so that incomplete profiles have the highest frecency, profile2 has the
        // highest frecency and profile3 has the lowest among the complete profiles, and profile8
        // has the highest frecency and profile4 has the lowest among the incomplete profiles.
        helper.setProfileUseStatsForTesting(profile1, 5, 5);
        helper.setProfileUseStatsForTesting(profile2, 10, 10);
        helper.setProfileUseStatsForTesting(profile3, 1, 1);
        helper.setProfileUseStatsForTesting(profile4, 15, 15);
        helper.setProfileUseStatsForTesting(profile5, 30, 30);
        helper.setProfileUseStatsForTesting(profile6, 25, 25);
        helper.setProfileUseStatsForTesting(profile7, 20, 20);
        helper.setProfileUseStatsForTesting(profile8, 40, 40);
    }

    /** Verifies the format of the billing address suggestions when adding a new credit card. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNewCardBillingAddressFormat() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"5454-5454-5454-5454", "Bob"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        // The billing address suggestions should include only the name, address, city, state and
        // zip code of the profile.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Rob Doe, 340 Main St, Los Angeles, CA 90291");
    }

    /**
     * Verifies that the correct number of billing address suggestions are shown when adding a new
     * credit card.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfBillingAddressSuggestions() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // There should only be 9 suggestions, the 7 saved addresses, the select hint and the
        // option to add a new address.
        Assert.assertEquals(9,
                mPaymentRequestTestRule.getSpinnerItemCountInCardEditor(
                        BILLING_ADDRESS_DROPDOWN_INDEX));
    }

    /**
     * Verifies that the correct number of billing address suggestions are shown when adding a new
     * credit card, even after cancelling out of adding a new billing address.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfBillingAddressSuggestions_AfterCancellingNewBillingAddress()
            throws TimeoutException {
        // Add a payment method and add a new billing address.
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // Select the "+ ADD ADDRESS" option for the billing address.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS},
                mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the creation of a new billing address.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToEdit());
        // There should still only be 9 suggestions, the 7 saved addresses, the select hint and
        // the option to add a new address.
        Assert.assertEquals(9,
                mPaymentRequestTestRule.getSpinnerItemCountInCardEditor(
                        BILLING_ADDRESS_DROPDOWN_INDEX));
    }

    /**
     * Tests that for a card that already has a billing address, adding a new one and cancelling
     * maintains the previous selection. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddBillingAddressOnCardAndCancel_MaintainsPreviousSelection()
            throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        // Edit the only card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());

        // Jon Doe is selected as the billing address.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Jon Doe, 340 Main St, Los Angeles, CA 90291");

        // Select the "+ ADD ADDRESS" option for the billing address.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS},
                mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the creation of a new billing address.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToEdit());

        // Jon Doe is STILL selected as the billing address.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Jon Doe, 340 Main St, Los Angeles, CA 90291");
    }

    /**
     * Tests that adding a billing address for a card that has none, and cancelling then returns
     * to the proper selection (Select...).
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddBillingAddressOnCardWithNoBillingAndCancel_MaintainsPreviousSelection()
            throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        // Edit the second card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyToEdit());

        // Now in Card Editor to add a billing address. "Select" is selected in the dropdown.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Select");

        // Select the "+ ADD ADDRESS" option for the billing address.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS},
                mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the creation of a new billing address.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToEdit());

        // "Select" is STILL selected as the billing address.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Select");
    }

    /**
     * Verifies that the billing address suggestions are ordered by frecency.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testBillingAddressSortedByFrecency() throws TimeoutException {
        // Add a payment method.
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // There should be 9 suggestions, the 7 saved addresses, the select hint and the option to
        // add a new address.
        Assert.assertEquals(9,
                mPaymentRequestTestRule.getSpinnerItemCountInCardEditor(
                        BILLING_ADDRESS_DROPDOWN_INDEX));

        // The billing address suggestions should be ordered by frecency.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 0),
                "Select");
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 1),
                "Rob Doe, 340 Main St, Los Angeles, CA 90291");
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 2),
                "Jon Doe, 340 Main St, Los Angeles, CA 90291");
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 3),
                "Tom Doe, 340 Main St, Los Angeles, CA 90291");
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 8),
                "Add address");
    }

    /**
     * Verifies that the billing address suggestions are ordered by frecency, except for a newly
     * created address which should be suggested first.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testBillingAddressSortedByFrecency_AddNewAddress() throws TimeoutException {
        // Add a payment method.
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // Add a new billing address.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, ADD_BILLING_ADDRESS},
                mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Seb Doe", "Google", "340 Main St", "Los Angeles", "CA", "90291",
                        "650-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToEdit());

        // There should be 10 suggestions, the 7 initial addresses, the newly added address, the
        // select hint and the option to add a new address.
        Assert.assertEquals(10,
                mPaymentRequestTestRule.getSpinnerItemCountInCardEditor(
                        BILLING_ADDRESS_DROPDOWN_INDEX));

        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 0),
                "Select");
        // The fist address suggestion should be the newly added address.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 1),
                "Seb Doe, 340 Main St, Los Angeles, CA 90291");

        // The rest of the billing address suggestions should be ordered by frecency.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 2),
                "Rob Doe, 340 Main St, Los Angeles, CA 90291");
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 3),
                "Jon Doe, 340 Main St, Los Angeles, CA 90291");
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 4),
                "Tom Doe, 340 Main St, Los Angeles, CA 90291");
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, 9),
                "Add address");
    }

    /**
     * Verifies that a newly created shipping address is offered as the first billing address
     * suggestion.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNewShippingAddressSuggestedFirst() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());

        // Add a shipping address.
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

        // Navigate to the card editor UI.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // There should be 10 suggestions, the 7 initial addresses, the newly added address, the
        // select hint and the option to add a new address.
        Assert.assertEquals(10,
                mPaymentRequestTestRule.getSpinnerItemCountInCardEditor(
                        BILLING_ADDRESS_DROPDOWN_INDEX));

        // The new address must be put at the top of the dropdown list, right after the
        // select hint.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, FIRST_BILLING_ADDRESS),
                "Seb Doe, 340 Main St, Los Angeles, CA 90291");
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSelectIncompleteBillingAddress_EditComplete() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        // Edit the second card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyToEdit());

        // Now "Select" is selected in the dropdown.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Select");

        // The incomplete addresses in the dropdown contain edit required messages.
        assertThat(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                           BILLING_ADDRESS_DROPDOWN_INDEX, 5),
                endsWith("Name required"));
        assertThat(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                           BILLING_ADDRESS_DROPDOWN_INDEX, 6),
                endsWith("More information required"));
        assertThat(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                           BILLING_ADDRESS_DROPDOWN_INDEX, 7),
                endsWith("Enter a valid address"));

        // Selects the fourth billing address (the 5th option on the dropdown list) that misses
        // recipient name brings up the address editor.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, 5}, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Lisa Doh", "Google", "340 Main St", "Los Angeles", "CA", "90291",
                        "650-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToEdit());

        // The newly completed address must be selected and put at the top of the dropdown list,
        // right after the select hint.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Lisa Doh, 340 Main St, Los Angeles, CA 90291");

        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX, FIRST_BILLING_ADDRESS),
                "Lisa Doh, 340 Main St, Los Angeles, CA 90291");
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSelectIncompleteBillingAddress_EditCancel() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        // Edit the only complete card.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());

        // Jon Doe is selected as the billing address.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Jon Doe, 340 Main St, Los Angeles, CA 90291");

        // The incomplete addresses in the dropdown contain edit required messages.
        assertThat(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                           BILLING_ADDRESS_DROPDOWN_INDEX, 5),
                endsWith("Name required"));
        assertThat(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                           BILLING_ADDRESS_DROPDOWN_INDEX, 6),
                endsWith("More information required"));
        assertThat(mPaymentRequestTestRule.getSpinnerTextAtPositionInCardEditor(
                           BILLING_ADDRESS_DROPDOWN_INDEX, 7),
                endsWith("Enter a valid address"));

        // Selects the forth billing address (the 5th option on the dropdown list) that misses
        // recipient name brings up the address editor.
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, 5}, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToEdit());

        // The previous selected address should be selected after canceling out from edit.
        Assert.assertEquals(mPaymentRequestTestRule.getSpinnerSelectionTextInCardEditor(
                                    BILLING_ADDRESS_DROPDOWN_INDEX),
                "Jon Doe, 340 Main St, Los Angeles, CA 90291");
    }
}
