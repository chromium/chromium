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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.browser.payments.ui.PaymentRequestSection;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping
 * and user that has a single address stored in autofill settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestDynamicShippingSingleAddressTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_dynamic_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        helper.setProfile(
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
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** The shipping address should not be selected in UI by default. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddressNotSelected() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(
                PaymentRequestSection.EDIT_BUTTON_CHOOSE,
                mPaymentRequestTestRule.getShippingAddressSectionButtonState());
    }

    /** Expand the shipping address section, select an address, and click "Pay." */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressAndPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "Jon Doe",
                    "\"methodName\": \"https://bobpay.test\"",
                    "\"transaction\": 1337",
                    "Google",
                    "340 Main St",
                    "CA",
                    "Los Angeles",
                    "90291",
                    "+16502530000",
                    "US",
                    "en",
                    "californiaShippingOption"
                });
    }

    /** Expand the shipping address section, select an address, edit it and click "Pay." */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressEditItAndPay() throws TimeoutException, InterruptedException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe"}, mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "Jane Doe",
                    "\"methodName\": \"https://bobpay.test\"",
                    "\"transaction\": 1337",
                    "Google",
                    "340 Main St",
                    "CA",
                    "Los Angeles",
                    "90291",
                    "+16502530000",
                    "US",
                    "en",
                    "californiaShippingOption"
                });
    }

    /** Expand the shipping address section, select address, edit but cancel editing, and "Pay". */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSelectValidAddressEditItAndCancelAndPay()
            throws TimeoutException, InterruptedException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe"}, mPaymentRequestTestRule.getEditorTextUpdate());
        // Cancel the edit.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.expectShippingAddressRowIsSelected(0);

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "Jon Doe",
                    "\"methodName\": \"https://bobpay.test\"",
                    "\"transaction\": 1337",
                    "Google",
                    "340 Main St",
                    "CA",
                    "Los Angeles",
                    "90291",
                    "+16502530000",
                    "US",
                    "en",
                    "californiaShippingOption"
                });
    }

    /** Attempt to add an invalid address and cancel the transaction. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidAddressAndCancel() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Shipping Address).
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyForInput());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /**
     * Add a valid address and complete the
     * transaction. @MediumTest @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE) //
     * crbug.com/626289
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddAddressAndPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                    "Bob",
                    "Google",
                    "1600 Amphitheatre Pkwy",
                    "Mountain View",
                    "CA",
                    "94043",
                    "650-253-0000"
                },
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "Bob",
                    "\"methodName\": \"https://bobpay.test\"",
                    "\"transaction\": 1337",
                    "Google",
                    "1600 Amphitheatre Pkwy",
                    "Mountain View",
                    "CA",
                    "94043",
                    "+16502530000"
                });
    }

    /** Quickly pressing "add address" and then [X] should not crash. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCloseShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press "add address" and then [X].
        int callCount = mPaymentRequestTestRule.getReadyToEdit().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getShippingAddressSectionForTest()
                            .findViewById(R.id.payments_add_option_button)
                            .performClick();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.close_button)
                            .performClick();
                });
        mPaymentRequestTestRule.getReadyToEdit().waitForCallback(callCount);

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Quickly pressing [X] and then "add address" should not crash. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCloseAndAddAddressShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press [X] and then "add address."
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.close_button)
                            .performClick();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getShippingAddressSectionForTest()
                            .findViewById(R.id.payments_add_option_button)
                            .performClick();
                });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Quickly pressing "add address" and then "cancel" should not crash. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickAddAddressAndCancelShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press "add address" and then "cancel."
        int callCount = mPaymentRequestTestRule.getReadyToEdit().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getShippingAddressSectionForTest()
                            .findViewById(R.id.payments_add_option_button)
                            .performClick();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.button_secondary)
                            .performClick();
                });
        mPaymentRequestTestRule.getReadyToEdit().waitForCallback(callCount);

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Quickly pressing on "cancel" and then "add address" should not crash. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testQuickCancelAndAddAddressShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press on "cancel" and then "add address."
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getDialogForTest()
                            .findViewById(R.id.button_secondary)
                            .performClick();
                    mPaymentRequestTestRule
                            .getPaymentRequestUI()
                            .getShippingAddressSectionForTest()
                            .findViewById(R.id.payments_add_option_button)
                            .performClick();
                });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }
}
