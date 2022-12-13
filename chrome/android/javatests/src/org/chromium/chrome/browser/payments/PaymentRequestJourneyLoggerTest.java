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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.Event;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test to validate the logging of Payment Request metrics.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestJourneyLoggerTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_metrics_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper mHelper = new AutofillTestHelper();
        // The user has a shipping address.
        String mBillingAddressId = mHelper.setProfile(
                new AutofillProfile("", "https://example.test", true, "" /* honorific prefix */,
                        "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                        "US", "650-253-0000", "jondoe@email.com", "en-US"));
        // The user also has an incomplete address.
        String mIncompleteAddressId = mHelper.setProfile(new AutofillProfile("",
                "https://example.test", true, "" /* honorific prefix */, "In Complete", "Google",
                "344 Main St", "CA", "", "", "90291", "", "US", "650-253-0000", "", "en-US"));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_Completed() throws TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.Completed", 2));
    }

    /**
     * Expect that the number of shipping address suggestions was logged properly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ShippingAddress_AbortedByUser()
            throws InterruptedException, TimeoutException {
        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ShippingAddress.UserAborted", 2));
    }

    /**
     * Expect that the number of payment method suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_Completed() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Complete a Payment Request with the payment app.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        // Matches either "https://bobpay.test" or "https://kylepay.test/webpay"
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://", "\"transaction\"", "1337"});

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.Completed", 2));
    }

    /**
     * Expect that the number of payment method suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_AbortedByUser()
            throws InterruptedException, TimeoutException {
        // Add two payment apps
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted", 2));
    }

    /**
     * Expect that an incomplete payment app is not suggested to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_PaymentMethod_InvalidPaymentApp()
            throws InterruptedException, TimeoutException {
        // Add an incomplete payment app.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.NO_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethods", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        Thread.sleep(200);

        // Make sure only the one payment app suggestions was logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.PaymentMethod.UserAborted", 1));
    }

    /**
     * Expect that the number of contact info suggestions was logged properly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testNumberOfSuggestionsShown_ContactInfo_Completed() throws TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2));
    }

    /**
     * Expect that the number of contact info suggestions was logged properly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @DisabledTest(message = "https://crbug.com/1197578")
    public void testNumberOfSuggestionsShown_ContactInfo_AbortedByUser()
            throws InterruptedException, TimeoutException {
        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "contactInfoBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());

        // Wait for the histograms to be logged.
        Thread.sleep(200);

        // Make sure the right number of suggestions were logged.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.UserAborted", 2));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_ShippingAndPayment() throws TimeoutException {
        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testUserDidNotHaveCompleteSuggestions_ShippingAndPayment_IncompleteShipping()
            throws TimeoutException {
        // Add a card and an incomplete address (no region).
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(
                new AutofillProfile("", "https://example.test", true, "" /* honorific prefix */,
                        "Jon Doe", "Google", "340 Main St", /*region=*/"", "Los Angeles", "",
                        "90291", "", "US", "650-253-0000", "", "en-US"));

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly. Since the added credit card is using the same
        // incomplete profile for billing address, NEEDS_COMPLETION_PAYMENT is also set.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_BASIC_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_PAYMENT
                | Event.NEEDS_COMPLETION_SHIPPING;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testUserDidNotHaveCompleteSuggestions_ShippingAndPayment_IncompleteCard()
            throws TimeoutException {
        // Add an incomplete card (no exp date) and an complete address.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId =
                mHelper.setProfile(new AutofillProfile("", "https://example.test", true,
                        "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA",
                        "Los Angeles", "", "90291", "", "US", "650-253-0000", "", "en-US"));

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_BASIC_CARD
                | Event.AVAILABLE_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testUserDidNotHaveCompleteSuggestions_ShippingAndPayment_OnlyPaymentApp()
            throws TimeoutException {
        // Add a complete address and a working payment app.
        AutofillTestHelper mHelper = new AutofillTestHelper();
        mHelper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "650-253-0000", "", "en-US"));
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "ccBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_PAYMENT;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
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
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
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
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_OTHER | Event.AVAILABLE_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that the metric that records whether the user had complete suggestions for the
     * requested information is logged correctly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testUserHadCompleteSuggestions_ShippingAndPaymentApp_HasInvalidShipping()
            throws TimeoutException {
        // Add a card and an incomplete address (no region).
        AutofillTestHelper mHelper = new AutofillTestHelper();
        String mBillingAddressId = mHelper.setProfile(
                new AutofillProfile("", "https://example.test", true, "" /* honorific prefix */,
                        "Jon Doe", "Google", "340 Main St", /*region=*/"", "Los Angeles", "",
                        "90291", "", "US", "650-253-0000", "", "en-US"));

        // Cancel the payment request.
        mPaymentRequestTestRule.triggerUIAndWait(
                "cardsAndBobPayBuy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        // Make sure the events were logged correctly. Since the added credit card is using the same
        // incomplete profile, NEEDS_COMPLETION_PAYMENT is also set.
        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.REQUEST_SHIPPING | Event.REQUEST_METHOD_BASIC_CARD
                | Event.REQUEST_METHOD_OTHER | Event.NEEDS_COMPLETION_PAYMENT
                | Event.AVAILABLE_METHOD_BASIC_CARD | Event.NEEDS_COMPLETION_SHIPPING;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }

    /**
     * Expect that no metric for contact info has been logged.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testNoContactInfoHistogram() throws TimeoutException {
        // Complete a Payment Request with a credit card.
        mPaymentRequestTestRule.triggerUIAndWait("ccBuy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        // Make sure nothing was logged for contact info.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.NumberOfSuggestionsShown.ContactInfo.Completed", 2));
    }

    /**
     * Expect that that the journey metrics are logged correctly on a second consecutive payment
     * request.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testTwoTimes() throws TimeoutException {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
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
