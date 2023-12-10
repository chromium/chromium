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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

import java.util.concurrent.TimeoutException;

/** A payment integration test for shipping address labels. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShippingAddressAndOptionTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_free_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address associated with a credit card.
        String firstAddressId =
                helper.setProfile(
                        AutofillProfile.builder()
                                .setFullName("Jon Doe")
                                .setCompanyName("Google")
                                .setStreetAddress("340 Main St")
                                .setRegion("CA")
                                .setLocality("Los Angeles")
                                .setPostalCode("90291")
                                .setCountryCode("US")
                                .setPhoneNumber("555-555-5555")
                                .setLanguageCode("en-US")
                                .build());

        // The user has a second address.
        String secondAddressId =
                helper.setProfile(
                        AutofillProfile.builder()
                                .setFullName("Fred Doe")
                                .setCompanyName("Google")
                                .setStreetAddress("340 Main St")
                                .setRegion("CA")
                                .setLocality("Los Angeles")
                                .setPostalCode("90291")
                                .setCountryCode("US")
                                .setPhoneNumber("555-555-5555")
                                .setEmailAddress("en-US")
                                .build());

        // Set the fist profile to have a better frecency score that the second one.
        helper.setProfileUseStatsForTesting(firstAddressId, 10, 0);
        helper.setProfileUseStatsForTesting(secondAddressId, 0, 10);
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Verifies that the shipping address format in bottomsheet mode is as expected. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_BottomSheet() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Make sure that the shipping label on the bottomsheet does not include the country.
        Assert.assertEquals(
                "Jon Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n555-555-5555",
                mPaymentRequestTestRule.getShippingAddressSummaryLabel());

        // Make sure shipping option summary on bottom sheet is displayed in a single line
        // as expected.
        Assert.assertEquals(
                "Free global shipping", mPaymentRequestTestRule.getShippingOptionSummaryLabel());

        // Make sure shipping cost is displayed in the right summary text view.
        Assert.assertEquals(
                "$0.00", mPaymentRequestTestRule.getShippingOptionCostSummaryLabelOnBottomSheet());
    }

    /** Verifies that the shipping address format in fullsheet mode is as expected. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_FullSheet() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        // Focus on a section other that shipping addresses to enter fullsheet mode.
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Make sure that the shipping label on the fullsheet does not include the country.
        Assert.assertEquals(
                "Jon Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n555-555-5555",
                mPaymentRequestTestRule
                        .getShippingAddressOptionRowAtIndex(0)
                        .getLabelText()
                        .toString());

        // Make sure shipping option summary on the full sheet is displayed on multiple lines
        // as expected.
        Assert.assertEquals(
                "Free global shipping\n$0.00",
                mPaymentRequestTestRule.getShippingOptionSummaryLabel());
    }

    /** Verifies that the shipping address format in expanded mode is as expected. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_Expanded() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Focus on the shipping addresses section to enter expanded mode.
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Make sure that the shipping label in expanded mode includes the country.
        Assert.assertTrue(
                mPaymentRequestTestRule
                        .getShippingAddressOptionRowAtIndex(0)
                        .getLabelText()
                        .toString()
                        .equals(
                                "Jon Doe\n"
                                        + "Google, 340 Main St, Los Angeles, CA 90291, United States\n"
                                        + "555-555-5555"));

        // Make sure that the second profile's shipping label also includes the country.
        Assert.assertTrue(
                mPaymentRequestTestRule
                        .getShippingAddressOptionRowAtIndex(1)
                        .getLabelText()
                        .toString()
                        .equals(
                                "Fred Doe\n"
                                        + "Google, 340 Main St, Los Angeles, CA 90291, United States\n"
                                        + "555-555-5555"));
    }

    /** Verifies that the shipping address format of a new address is as expected. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testShippingAddressFormat_NewAddress() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        // Add a shipping address.
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

        // Make sure that the shipping label does not include the country.
        Assert.assertEquals(
                mPaymentRequestTestRule
                        .getShippingAddressOptionRowAtIndex(0)
                        .getLabelText()
                        .toString(),
                "Seb Doe\nGoogle, 340 Main St, Los Angeles, CA 90291\n+1 650-253-0000");
    }

    /**
     * Test that going into the editor and clicking 'CANCEL' button to cancel editor will leave the
     * row checked.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditShippingAddressAndCancelEditorShouldKeepAddressSelected()
            throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the editor by clicking 'CANCEL' button in the editor view.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());

        // Expect the row to still be selected in the Shipping Address section.
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
    }

    /**
     * Test that going into the editor and clicking Android back button to cancel editor will leave
     * the row checked.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testEditShippingAddressAndClickAndroidBackButtonShouldKeepAddressSelected()
            throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the editor by clicking Android back button.
        mPaymentRequestTestRule.clickAndroidBackButtonInEditorAndWait(
                mPaymentRequestTestRule.getReadyToPay());

        // Expect the row to still be selected in the Shipping Address section.
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
    }

    /**
     * Test that going into the "add" flow and clicking 'CANCEL' button to cancel editor will leave
     * the existing row checked.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddShippingAddressAndCancelEditorShouldKeepAddressSelected()
            throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the editor by clicking 'CANCEL' button in the editor view.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());

        // Expect the existing row to still be selected in the Shipping Address section.
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
    }

    /**
     * Test that going into the "add" flow and clicking Android back button to cancel editor will
     * leave the existing row checked.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testAddShippingAddressAndClickAndroidBackButtonShouldKeepAddressSelected()
            throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());

        // Cancel the editor by clicking Android back button.
        mPaymentRequestTestRule.clickAndroidBackButtonInEditorAndWait(
                mPaymentRequestTestRule.getReadyToPay());

        // Expect the existing row to still be selected in the Shipping Address section.
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
    }
}
