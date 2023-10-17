// Copyright 2019 The Chromium Authors
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that retries payment request with payment validation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    PaymentRequestTestRule.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES
})
public class PaymentRequestRetryTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_retry.html");

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.BLINK_PAYMENTS)
                    .build();

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
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
                        .setEmailAddress("jon.doe@gmail.com")
                        .setLanguageCode("en-US")
                        .build());

        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Tests that only the initially selected payment app is available during retry(). */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDoNotAllowPaymentAppChange() throws TimeoutException {
        // Note that the bobpay app has been added in onMainActivityStarted(), so we will have two
        // payment apps in total.
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://kylepay.test/webpay", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyToPay());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        // Confirm that only one payment app is available for retry().
        mPaymentRequestTestRule.retryPaymentRequest("{}", mPaymentRequestTestRule.getReadyToPay());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentApps());
    }

    /** Test for retry() with default error message */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryWithDefaultError() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{}", mPaymentRequestTestRule.getReadyToPay());

        Assert.assertEquals(
                mPaymentRequestTestRule.getActivity().getString(R.string.payments_error_message),
                mPaymentRequestTestRule.getRetryErrorMessage());
    }

    /** Test for retry() with custom error message. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryWithCustomError() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest(
                "{" + "  error: 'ERROR'" + "}", mPaymentRequestTestRule.getReadyToPay());

        Assert.assertEquals("ERROR", mPaymentRequestTestRule.getRetryErrorMessage());
    }

    /** Test for retry() with shipping address errors. */
    @Test
    @MediumTest
    @Feature({"Payments", "RenderTest"})
    public void testRetryWithShippingAddressErrors() throws Throwable {
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest(
                "{"
                        + "  shippingAddress: {"
                        + "    country: 'COUNTRY ERROR',"
                        + "    recipient: 'RECIPIENT ERROR',"
                        + "    organization: 'ORGANIZATION ERROR',"
                        + "    addressLine: 'ADDRESS LINE ERROR',"
                        + "    city: 'CITY ERROR',"
                        + "    region: 'REGION ERROR',"
                        + "    postalCode: 'POSTAL CODE ERROR',"
                        + "    phone: 'PHONE ERROR'"
                        + "  }"
                        + "}",
                mPaymentRequestTestRule.getEditorValidationError());

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToEdit());

        mPaymentRequestTestRule
                .getKeyboardDelegate()
                .hideKeyboard(mPaymentRequestTestRule.getEditorDialogView());

        ChromeRenderTestRule.sanitize(mPaymentRequestTestRule.getEditorDialogView());
        mRenderTestRule.render(
                mPaymentRequestTestRule.getEditorDialogView(),
                "retry_with_shipping_address_errors");

        mPaymentRequestTestRule.setSpinnerSelectionInEditorAndWait(
                0 /* Afghanistan */, mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                    "Alice", "Supreme Court", "Airport Road", "Kabul", "1043", "020-253-0000"
                },
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
    }

    /** Test for retry() with payer errors. */
    @Test
    @MediumTest
    @Feature({"Payments", "RenderTest"})
    public void testRetryWithPayerErrors() throws Throwable {
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest(
                "{"
                        + "  payer: {"
                        + "    email: 'EMAIL ERROR',"
                        + "    name: 'NAME ERROR',"
                        + "    phone: 'PHONE ERROR'"
                        + "  }"
                        + "}",
                mPaymentRequestTestRule.getEditorValidationError());

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToEdit());

        mPaymentRequestTestRule
                .getKeyboardDelegate()
                .hideKeyboard(mPaymentRequestTestRule.getEditorDialogView());

        ChromeRenderTestRule.sanitize(mPaymentRequestTestRule.getEditorDialogView());
        mRenderTestRule.render(
                mPaymentRequestTestRule.getEditorDialogView(), "retry_with_payer_errors");

        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe", "650-253-0000", "jane.doe@gmail.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
    }

    /** Test for retry() with shipping address errors and payer errors. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryWithShippingAddressErrorsAndPayerErrors() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest(
                "{"
                        + "  shippingAddress: {"
                        + "    addressLine: 'ADDRESS LINE ERROR',"
                        + "    city: 'CITY ERROR'"
                        + "  },"
                        + "  payer: {"
                        + "    email: 'EMAIL ERROR',"
                        + "    name: 'NAME ERROR',"
                        + "    phone: 'PHONE ERROR'"
                        + "  }"
                        + "}",
                mPaymentRequestTestRule.getEditorValidationError());

        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe", "Edge Corp.", "111 Wall St.", "New York", "NY"},
                mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());

        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe", "650-253-0000", "jon.doe@gmail.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
    }

    /** Test for onpayerdetailchange event after retry(). */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryAndPayerDetailChangeEvent() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{}", mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_open_editor_pencil_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe", "650-253-0000", "jane.doe@gmail.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jane Doe", "6502530000", "jane.doe@gmail.com"});
    }

    /** Test for reselecting contact detail after retry(). */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryAndReselectContactDetail() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(
                "buyWithUrlMethod", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{}", mPaymentRequestTestRule.getReadyToPay());

        // Add new contact detail
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe", "650-253-0000", "jane.doe@gmail.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        // Reselect new contact detail
        mPaymentRequestTestRule.expectContactDetailsRowIsSelected(0);
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickOnContactInfoSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.expectContactDetailsRowIsSelected(1);

        // Click 'Pay'; This logic should be executed successfully.
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
    }
}
