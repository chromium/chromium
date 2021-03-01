// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import android.widget.Button;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay or basic-card with
 * modifiers.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPaymentAppAndBasicCardWithModifiersTest {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_bobpay_and_basic_card_with_modifiers_test.html");

    private AutofillTestHelper mHelper;
    private String mBillingAddressId;

    @Before
    public void setUp() throws Throwable {
        mHelper = new AutofillTestHelper();
        mBillingAddressId = mHelper.setProfile(new AutofillProfile("", "https://example.com", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "310-310-6000", "jon.doe@gmail.com", "en-US"));
    }

    protected String getPrimaryButtonLabel() {
        Button primary = (Button) mPaymentRequestTestRule.getPaymentRequestUI()
                                 .getDialogForTest()
                                 .findViewById(R.id.button_primary);
        return primary.getText().toString();
    }

    /**
     * Verify modifier for Bobpay is only applied for Bobpay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @FlakyTest(message = "https://crbug.com/1182584")
    public void testUpdateTotalAndInstrumentLabelWithBobPayModifiers() throws TimeoutException {
        // Mastercard card with complete set of information and unknown type.
        mHelper.setCreditCard(new CreditCard(/*guid=*/"", "https://example.com", true /* isLocal */,
                true /* isCached */, "Jon Doe", "5555555555554444", "" /* obfuscatedNumber */, "12",
                "2050", "mastercard", R.drawable.mc_card, mBillingAddressId, "" /* serverId */));
        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_bobpay_discount", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());
        assertEquals(mPaymentRequestTestRule.getActivity().getResources().getString(
                             R.string.payments_continue_button),
                getPrimaryButtonLabel());

        // select other payment method and verify modifier for bobpay is not applied
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertFalse(mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
        assertEquals(mPaymentRequestTestRule.getActivity().getResources().getString(
                             R.string.payments_pay_button),
                getPrimaryButtonLabel());
    }

    /**
     * Verify modifier for visa card is only applied for visa card.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithVisaModifiers() throws TimeoutException {
        // Credit visa card with complete set of information.
        String guid1 = mHelper.setCreditCard(new CreditCard(/*guid=*/"", "https://example.com",
                true /* isLocal */, true /* isCached */, "Jon Doe", "4111111111111111",
                "" /* obfuscatedNumber */, "12", "2050", "visa", R.drawable.visa_card,
                mBillingAddressId, "server-id-1"));
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(guid1, /*count=*/100);
        // Credit mastercard with complete set of information.
        String guid2 = mHelper.setCreditCard(new CreditCard(/*guid=*/"", "https://example.com",
                true /* isLocal */, true /* isCached */, "Jon Doe", "5200828282828210",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                mBillingAddressId, "server-id-2"));
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(guid2, /*count=*/1);
        mPaymentRequestTestRule.triggerUIAndWait(
                "visa_supported_network", mPaymentRequestTestRule.getReadyToPay());

        assertTrue("\"" + mPaymentRequestTestRule.getSelectedPaymentAppLabel()
                        + "\" should start with \"Visa\".",
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith("Visa"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select the other credit mastercard and verify modifier is not applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertTrue("\"" + mPaymentRequestTestRule.getSelectedPaymentAppLabel()
                        + "\" should start with \"Mastercard\".",
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith("Mastercard"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify modifier for mastercard is applied for mastercard.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithMastercardModifiers() throws TimeoutException {
        // 1st Mastercard card with complete set of information.
        String guid = mHelper.setCreditCard(new CreditCard(/*guid=*/"", "https://example.com",
                true /* isLocal */, true /* isCached */, "Jon Doe", "5555555555554444",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                mBillingAddressId, "" /* serverId */));
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(guid, /*count=*/1000);

        // 2nd Mastercard with complete set of information.
        String guid1 = mHelper.setCreditCard(new CreditCard(/*guid=*/"", "https://example.com",
                true /* isLocal */, true /* isCached */, "Jon Doe", "5200828282828210",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                mBillingAddressId, "server-id-1"));
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(guid1, /*count=*/100);

        // Visa card with complete set of information and unknown type.
        String guid2 = mHelper.setCreditCard(new CreditCard(/*guid=*/"", "https://example.com",
                true /* isLocal */, true /* isCached */, "Jon Doe", "4111111111111111",
                "" /* obfuscatedNumber */, "12", "2050", "visa", R.drawable.visa_card,
                mBillingAddressId, "server-id-2"));
        PaymentPreferencesUtil.setPaymentAppUseCountForTest(guid2, /*count=*/1);

        // The most frequently used Mastercard is selected by default.
        mPaymentRequestTestRule.triggerUIAndWait(
                "mastercard_any_supported_type", mPaymentRequestTestRule.getReadyToPay());
        assertTrue("\"" + mPaymentRequestTestRule.getSelectedPaymentAppLabel()
                        + "\" should start with \"Mastercard\".",
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith("Mastercard"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // Select the other credit Mastercard and verify modifier is applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertTrue("\"" + mPaymentRequestTestRule.getSelectedPaymentAppLabel()
                        + "\" should start with \"Mastercard\".",
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith("Mastercard"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // Select the visa card and verify modifier is not applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                2, mPaymentRequestTestRule.getReadyForInput());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith("Visa"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify native app can pay as expected when modifier is applied.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppCanPayWithModifiers() throws TimeoutException {
        // Add a credit card to force showing payment sheet UI.
        String billingAddressId = mHelper.setProfile(
                new AutofillProfile("", "https://example.com", true, "" /* honorific prefix */,
                        "John Smith", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                        "US", "310-310-6000", "john.smith@gmail.com", "en-US"));
        mHelper.setCreditCard(new CreditCard(/*guid=*/"", "https://example.com", true, true,
                "Jon Doe", "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));

        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.SLOW_FACTORY);
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_bobpay_discount", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentAppLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }
}
