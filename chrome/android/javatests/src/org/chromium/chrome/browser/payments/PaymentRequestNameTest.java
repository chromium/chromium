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

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.Event2;

import java.util.concurrent.TimeoutException;

/** A payment integration test for a merchant that requests payer name. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestNameTest {
    /** A fake payment method. */
    private static final String BOBPAY_TEST = "https://bobpay.test";

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_name_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a valid payer name on disk.
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
                BOBPAY_TEST, AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Provide the existing valid payer name to the merchant. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon Doe"});
    }

    /** Attempt to add an invalid payer name and cancel the transaction. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidNameAndCancel() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {""}, mPaymentRequestTestRule.getEditorTextUpdate());

        // Expect an editor validation error when clicking done.
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
    @Feature({"Payments"})
    public void testAddNameAndPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Jones"}, mPaymentRequestTestRule.getEditorTextUpdate());

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jane Jones"});
    }

    /**
     * Makes sure that suggestions that are equal to or subsets of other suggestions are not shown
     * to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuggestionsDeduped() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
    }

    /**
     * Test that ending a payment request that requires only the user's payer name results in the
     * appropriate metric being logged in PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        int expectedSample =
                Event2.SHOWN
                        | Event2.PAY_CLICKED
                        | Event2.COMPLETED
                        | Event2.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event2.REQUEST_PAYER_DATA
                        | Event2.REQUEST_METHOD_OTHER
                        | Event2.SELECTED_OTHER;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("PaymentRequest.Events2", expectedSample)
                        .build();

        // Start and complete the Payment Request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon Doe"});
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }
}
