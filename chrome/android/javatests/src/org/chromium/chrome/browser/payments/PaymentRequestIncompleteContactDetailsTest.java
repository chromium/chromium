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
 * A payment integration test for a merchant that requests contact details from a user that has
 * incomplete contact details stored on disk.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestIncompleteContactDetailsTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_contact_details_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has an invalid email address on disk.
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("333-333-3333")
                        .setEmailAddress("jon.doe" /* invalid email address */)
                        .setLanguageCode("en-US")
                        .build());

        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Attempt to update the contact information with invalid data and cancel the transaction. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteContactAndCancel() throws TimeoutException {
        // Not ready to pay since Contact email is invalid.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buy();", mPaymentRequestTestRule.getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        // Updating contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"", "---", "jane.jones"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
        // The section collapses and the [CHOOSE] button is active.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(
                PaymentRequestSection.EDIT_BUTTON_CHOOSE,
                mPaymentRequestTestRule.getContactDetailsButtonState());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Attempt to add invalid contact info alongside the already invalid info, and cancel. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddIncompleteContactAndCancel() throws TimeoutException, InterruptedException {
        // Not ready to pay since Contact email is invalid.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buy();", mPaymentRequestTestRule.getReadyForInput());
        // Check that there is a selected payment method (makes sure we are not ready to pay because
        // of the Contact Details).
        mPaymentRequestTestRule.expectPaymentMethodRowIsSelected(0);
        // Adding contact with an invalid value and cancelling means we're still not
        // ready to pay (the value is still the original value).
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"", "---", "jane.jones"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
        // The section collapses and the [CHOOSE] button is active.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(
                PaymentRequestSection.EDIT_BUTTON_CHOOSE,
                mPaymentRequestTestRule.getContactDetailsButtonState());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Update the contact information with valid data and provide that to the merchant. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteContactAndPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buy();", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jon Doe", "555-555-5555", "jon.doe@google.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jon Doe", "+15555555555", "jon.doe@google.com"});
    }
}
