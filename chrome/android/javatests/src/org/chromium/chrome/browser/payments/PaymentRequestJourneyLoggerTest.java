// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.chromium.base.test.util.DoNotBatch;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
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
import org.chromium.components.payments.Event;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
@DoNotBatch(reason="Histogram values are not reset between runs.")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestJourneyLoggerTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_metrics_test.html");

    @Before
    public void setUp() throws Exception {
        AutofillTestHelper autofillTestHelper = new AutofillTestHelper();
        // The user has a shipping address.
        autofillTestHelper.setProfile(AutofillProfile.builder()
                                              .setFullName("Jon Doe")
                                              .setCompanyName("Google")
                                              .setStreetAddress("340 Main St")
                                              .setRegion("CA")
                                              .setLocality("Los Angeles")
                                              .setPostalCode("90291")
                                              .setCountryCode("US")
                                              .setPhoneNumber("650-253-0000")
                                              .setEmailAddress("jondoe@email.com")
                                              .setLanguageCode("en-US")
                                              .build());
        // The user also has an incomplete address.
        autofillTestHelper.setProfile(AutofillProfile.builder()
                                              .setFullName("In Complete")
                                              .setCompanyName("Google")
                                              .setStreetAddress("344 Main St")
                                              .setRegion("CA")
                                              .setPostalCode("90291")
                                              .setCountryCode("US")
                                              .setPhoneNumber("650-253-0000")
                                              .setLanguageCode("en-US")
                                              .build());
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    @After
    public void tearDown() throws Exception {
        var autofillTestHelper = new AutofillTestHelper();
        autofillTestHelper.clearAllDataForTesting();
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_Completed() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed",
                                2)
                        .build();

        // Complete a Payment Request with the fake url payment method.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_AbortedByUser() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted",
                                2)
                        .build();

        // Cancel the payment request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that the number of payment method suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_Completed() throws Exception {
        // Add a second payment app.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed",
                                2)
                        .build();

        // Complete a Payment Request with the payment app.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'},"
                        + " {supportedMethods: 'https://kylepay.test/webpay'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        // Matches either "https://bobpay.test" or "https://kylepay.test/webpay"
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://", "\"transaction\"", "1337"});

        // Make sure the right number of suggestions were logged.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that the number of payment method suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_AbortedByUser() throws Exception {
        // Add a second payment app.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted",
                                2)
                        .build();

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that an incomplete payment app is not suggested to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_InvalidPaymentApp() throws Exception {
        // Add a second incomplete payment app.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.NO_APPS, FactorySpeed.FAST_FACTORY);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted",
                                1)
                        .build();

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Make sure only the one payment app suggestions was logged.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that the number of contact info suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ContactInfo_Completed() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2)
                        .build();

        // Complete a Payment Request with the fake url payment method.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that the number of contact info suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ContactInfo_AbortedByUser() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.UserAborted",
                                2)
                        .build();

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_Shipping() throws TimeoutException {
        // Ensure Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS is present.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_OTHER | Event.AVAILABLE_METHOD_OTHER;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("PaymentRequest.Events", expectedSample)
                        .build();

        // Cancel the payment request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUserDidNotHaveCompleteSuggestions_IncompleteShipping() throws Exception {
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_OTHER | Event.AVAILABLE_METHOD_OTHER
                | Event.NEEDS_COMPLETION_SHIPPING;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("PaymentRequest.Events", expectedSample)
                        .build();
        // Set only an incomplete address (no region).
        var autofillTestHelper = new AutofillTestHelper();
        autofillTestHelper.clearAllDataForTesting();
        autofillTestHelper.setProfile(AutofillProfile.builder()
                                              .setFullName("Jon Doe")
                                              .setCompanyName("Google")
                                              .setStreetAddress("340 Main St")
                                              .setLocality("Los Angeles")
                                              .setPostalCode("90291")
                                              .setCountryCode("US")
                                              .setPhoneNumber("650-253-0000")
                                              .setLanguageCode("en-US")
                                              .build());

        // Cancel the payment request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly, particularly NEEDS_COMPLETION_SHIPPING is
        // set.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
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
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_OTHER | Event.AVAILABLE_METHOD_OTHER;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("PaymentRequest.Events", expectedSample)
                        .build();
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
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
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_OTHER | Event.AVAILABLE_METHOD_OTHER
                | Event.NEEDS_COMPLETION_SHIPPING;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("PaymentRequest.Events", expectedSample)
                        .build();
        // Add a card and an incomplete address (no region).
        AutofillTestHelper autofillTestHelper = new AutofillTestHelper();
        autofillTestHelper.clearAllDataForTesting();
        autofillTestHelper.setProfile(AutofillProfile.builder()
                                              .setFullName("Jon Doe")
                                              .setCompanyName("Google")
                                              .setStreetAddress("340 Main St")
                                              .setLocality("Los Angeles")
                                              .setPostalCode("90291")
                                              .setCountryCode("US")
                                              .setPhoneNumber("650-253-0000")
                                              .setLanguageCode("en-US")
                                              .build());

        // Cancel the payment request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that no metric for contact info has been logged.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoContactInfoHistogram() throws TimeoutException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed")
                        .build();

        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure nothing was logged for contact info.
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Expect that that the journey metrics are logged correctly on a second consecutive payment
     * request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testTwoTimes() throws TimeoutException {
        // Add a second payment app.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Complete a Payment Request with payment apps.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed", 2));

        // Complete a second Payment Request with payment apps.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed", 2));

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.COMPLETED | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_OTHER | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.RECEIVED_INSTRUMENT_DETAILS
                | Event.PAY_CLICKED | Event.AVAILABLE_METHOD_OTHER | Event.SELECTED_OTHER;
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
        // Android Pay has a factory but it does not return an app.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://android.com/pay", AppPresence.NO_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.clickNodeAndWait(
                "androidPayBuy", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"The payment method", "not supported"});

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
