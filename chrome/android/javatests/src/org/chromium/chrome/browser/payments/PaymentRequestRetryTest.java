// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;

import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
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
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that retries payment request with payment validation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        PaymentRequestTestRule.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES})
public class PaymentRequestRetryTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_retry.html", this);

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("components/test/data/payments/render_tests");

    @Override
    public void onMainActivityStarted() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();

        String billing_address_id = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "333-333-3333", "jon.doe@gmail.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true /* isLocal */,
                true /* isCached */, "Jon Doe", "5555555555554444", "" /* obfuscatedNumber */, "12",
                "2050", "mastercard", R.drawable.mc_card, CardType.UNKNOWN, billing_address_id,
                "" /* serverId */));

        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
    }

    /**
     * Test for retry() with default error message
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryWithDefaultError() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE,
                mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{}", mPaymentRequestTestRule.getReadyToPay());

        Assert.assertEquals(
                mPaymentRequestTestRule.getActivity().getString(R.string.payments_error_message),
                mPaymentRequestTestRule.getRetryErrorMessage());
    }

    /**
     * Test for retry() with custom error message.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryWithCustomError() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE,
                mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{"
                        + "  error: 'ERROR'"
                        + "}",
                mPaymentRequestTestRule.getReadyToPay());

        Assert.assertEquals("ERROR", mPaymentRequestTestRule.getRetryErrorMessage());
    }

    /**
     * Test for retry() with shipping address errors.
     */
    @Test
    @MediumTest
    @Feature({"Payments", "RenderTest"})
    @DisabledTest(message = "crbug.com/980276")
    public void testRetryWithShippingAddressErrors() throws Throwable {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE,
                mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{"
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

        mPaymentRequestTestRule.getKeyboardDelegate().hideKeyboard(
                mPaymentRequestTestRule.getEditorDialogView());

        RenderTestRule.sanitize(mPaymentRequestTestRule.getEditorDialogView());
        mRenderTestRule.render(mPaymentRequestTestRule.getEditorDialogView(),
                "retry_with_shipping_address_errors");

        mPaymentRequestTestRule.setSpinnerSelectionInEditorAndWait(
                0 /* Afghanistan */, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                        "Alice", "Supreme Court", "Airport Road", "Kabul", "1043", "020-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
    }

    /**
     * Test for retry() with payer errors.
     */
    @Test
    @MediumTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "https://crbug.com/979159")
    @Feature({"Payments", "RenderTest"})
    public void testRetryWithPayerErrors() throws Throwable {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE,
                mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{"
                        + "  payer: {"
                        + "    email: 'EMAIL ERROR',"
                        + "    name: 'NAME ERROR',"
                        + "    phone: 'PHONE ERROR'"
                        + "  }"
                        + "}",
                mPaymentRequestTestRule.getEditorValidationError());

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToEdit());

        mPaymentRequestTestRule.getKeyboardDelegate().hideKeyboard(
                mPaymentRequestTestRule.getEditorDialogView());

        RenderTestRule.sanitize(mPaymentRequestTestRule.getEditorDialogView());
        mRenderTestRule.render(
                mPaymentRequestTestRule.getEditorDialogView(), "retry_with_payer_errors");

        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jane Doe", "650-253-0000", "jane.doe@gmail.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
    }

    /**
     * Test for retry() with shipping address errors and payer errors.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryWithShippingAddressErrorsAndPayerErrors() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE,
                mPaymentRequestTestRule.getPaymentResponseReady());

        mPaymentRequestTestRule.retryPaymentRequest("{"
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
                mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
    }

    /**
     * Test for onpayerdetailchange event after retry().
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryAndPayerDetailChangeEvent() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE,
                mPaymentRequestTestRule.getPaymentResponseReady());

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
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jane Doe", "6502530000", "jane.doe@gmail.com"});
    }

    /**
     * Test for reselecting contact detail after retry().
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testRetryAndReselectContactDetail() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE,
                mPaymentRequestTestRule.getPaymentResponseReady());

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
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
    }
}
