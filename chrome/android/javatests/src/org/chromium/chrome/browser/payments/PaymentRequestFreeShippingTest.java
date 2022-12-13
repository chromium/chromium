// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.payments.Event;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that provides free shipping regardless of address.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestFreeShippingTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_free_shipping_test.html", /*delayStartActivity=*/true);

    private static final int RENDER_TEST_REVISION = 1;
    private static final String RENDER_TEST_REVISION_DESCRIPTION =
            "Updated EditText hint color for a11y";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_REVISION_DESCRIPTION)
                    .setBugComponent(RenderTestRule.Component.BLINK_PAYMENTS)
                    .build();

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address on disk.
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.test",
                true, "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA",
                "Los Angeles", "", "90291", "", "US", "650-253-0000", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.test", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "amex", R.drawable.amex_card,
                billingAddressId, "" /* serverId */));
    }

    /** Submit the shipping address to the merchant when the user clicks "Pay." */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testPayWithRender(boolean nightModeEnabled) throws Throwable {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mRenderTestRule.render(mPaymentRequestTestRule.getPaymentRequestView(), "free_shipping");
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mRenderTestRule.render(mPaymentRequestTestRule.getCardUnmaskView(), "unmask");
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Jon Doe", "4111111111111111",
                "12", "2050", "basic-card", "123", "Google", "340 Main St", "CA", "Los Angeles",
                "90291", "US", "en", "freeShippingOption"});
    }

    /** Attempt to add an invalid address and cancel the transaction. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/673371")
    @Feature({"Payments"})
    public void testAddInvalidAddressAndCancel() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Add a valid address and complete the transaction. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testAddAddressAndPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Bob", "Google", "1600 Amphitheatre Pkwy", "Mountain View", "CA",
                        "94043", "650-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"Bob", "Google",
                "1600 Amphitheatre Pkwy", "Mountain View", "CA", "94043", "+16502530000"});
    }

    /** Change the country in the spinner, add a valid address, and complete the transaction. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testChangeCountryAddAddressAndPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setSpinnerSelectionInEditorAndWait(
                0 /* Afghanistan */, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                        "Alice", "Supreme Court", "Airport Road", "Kabul", "1043", "020-253-0000"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogProperties.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {
                "Alice", "Supreme Court", "Airport Road", "Kabul", "1043", "+93202530000"});
    }

    /** Quickly pressing on "add address" and then [X] should not crash. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickAddAddressAndCloseShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press on "add address" and then [X].
        int callCount = mPaymentRequestTestRule.getReadyToEdit().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getShippingAddressSectionForTest()
                    .findViewById(R.id.payments_add_option_button)
                    .performClick();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.close_button)
                    .performClick();
        });
        mPaymentRequestTestRule.getReadyToEdit().waitForCallback(callCount);

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Quickly pressing on [X] and then "add address" should not crash. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickCloseAndAddAddressShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press on [X] and then "add address."
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.close_button)
                    .performClick();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getShippingAddressSectionForTest()
                    .findViewById(R.id.payments_add_option_button)
                    .performClick();
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Quickly pressing on "add address" and then "cancel" should not crash. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/673371")
    @Feature({"Payments"})
    public void testQuickAddAddressAndCancelShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press on "add address" and then "cancel."
        int callCount = mPaymentRequestTestRule.getReadyToEdit().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getShippingAddressSectionForTest()
                    .findViewById(R.id.payments_add_option_button)
                    .performClick();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.button_secondary)
                    .performClick();
        });
        mPaymentRequestTestRule.getReadyToEdit().waitForCallback(callCount);

        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Quickly pressing on "cancel" and then "add address" should not crash. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testQuickCancelAndAddAddressShouldNotCrash() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Quickly press on "cancel" and then "add address."
        int callCount = mPaymentRequestTestRule.getDismissed().getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getDialogForTest()
                    .findViewById(R.id.button_secondary)
                    .performClick();
            mPaymentRequestTestRule.getPaymentRequestUI()
                    .getShippingAddressSectionForTest()
                    .findViewById(R.id.payments_add_option_button)
                    .performClick();
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(callCount);

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
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
        // Start and abort the Payment Request.
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_SHIPPING
                | Event.REQUEST_METHOD_BASIC_CARD | Event.AVAILABLE_METHOD_BASIC_CARD;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
