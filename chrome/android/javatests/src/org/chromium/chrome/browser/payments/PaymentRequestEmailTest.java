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
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.Event2;

import java.util.concurrent.TimeoutException;

/** A payment integration test for a merchant that requests email address. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestEmailTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_email_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a valid email address on disk.
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
                        .setEmailAddress("jon.doe@google.com")
                        .setLanguageCode("en-US")
                        .build());

        // Add the same profile but with a different address.
        helper.setProfile(
                AutofillProfile.builder()
                        .setCompanyName("Google")
                        .setStreetAddress("999 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("555-555-5555")
                        .setEmailAddress("jon.doe@google.com")
                        .setLanguageCode("en-US")
                        .build());

        // Add the same profile but without a phone number.
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber(/* phone_number= */ "")
                        .setEmailAddress("jon.doe@google.com")
                        .setLanguageCode("en-US")
                        .build());

        // Add the same profile but without an email.
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
                        .setEmailAddress(/* emailAddress= */ "")
                        .setLanguageCode("en-US")
                        .build());

        // Add the same profile but without a name.
        helper.setProfile(
                AutofillProfile.builder()
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("555-555-5555")
                        .setEmailAddress("jon.doe@google.com")
                        .setLanguageCode("en-US")
                        .build());

        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Provide the existing valid email address to the merchant. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"jon.doe@google.com"});
    }

    /** Attempt to add an invalid email address and cancel the transaction. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidEmailAndCancel() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"jane.jones"}, mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Add a new email address and provide that to the merchant. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddEmailAndPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"jane.jones@google.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"jane.jones@google.com"});
    }

    /**
     * Makes sure that suggestions that are equal to or subsets of other suggestions are not shown
     * to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuggestionsDeduped() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
    }

    /**
     * Test that ending a payment request that requires only the shipping address results in the
     * appropriate metric being logged in PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        // Start and complete the Payment Request.
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"jon.doe@google.com"});

        int expectedSample =
                Event2.SHOWN
                        | Event2.COMPLETED
                        | Event2.PAY_CLICKED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_PAYER_DATA
                        | Event2.REQUEST_METHOD_BASIC_CARD
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events2", expectedSample));
    }
}
