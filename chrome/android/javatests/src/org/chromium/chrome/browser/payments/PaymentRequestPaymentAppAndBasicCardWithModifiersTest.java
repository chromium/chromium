// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DELAYED_RESPONSE;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.ENABLE_WEB_PAYMENTS_MODIFIERS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.HAVE_INSTRUMENTS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.IMMEDIATE_RESPONSE;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.payments.mojom.BasicCardType;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests payment via Bob Pay or basic-card with
 * modifiers.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES, ENABLE_WEB_PAYMENTS_MODIFIERS,
        "enable-features=" + ChromeFeatureList.WEB_PAYMENTS_RETURN_GOOGLE_PAY_IN_BASIC_CARD})
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
                "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "", "US",
                "310-310-6000", "jon.doe@gmail.com", "en-US"));
    }

    /**
     * Verify modifier for Bobpay is only applied for Bobpay.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithBobPayModifiers() throws TimeoutException {
        // Mastercard card with complete set of information and unknown type.
        mHelper.setCreditCard(new CreditCard("", "https://example.com", true /* isLocal */,
                true /* isCached */, "Jon Doe", "5555555555554444", "" /* obfuscatedNumber */, "12",
                "2050", "mastercard", R.drawable.mc_card, CardType.UNKNOWN, mBillingAddressId,
                "" /* serverId */));
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, IMMEDIATE_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_bobpay_discount", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select other payment method and verify modifier for bobpay is not applied
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertFalse(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify modifier for credit card is only applied for credit card.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithCreditModifiers() throws TimeoutException {
        // Credit mastercard card with complete set of information.
        mHelper.addServerCreditCard(new CreditCard("guid_1", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "5454545454545454",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.CREDIT, mBillingAddressId, "server-id-1"));
        mHelper.setCreditCardUseStatsForTesting("guid_1", 100, 5000);
        // Mastercard card with complete set of information and unknown type.
        mHelper.addServerCreditCard(new CreditCard("guid_2", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "5555555555554444",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.UNKNOWN, mBillingAddressId, "server-id-2"));
        mHelper.setCreditCardUseStatsForTesting("guid_2", 1, 5000);
        mPaymentRequestTestRule.triggerUIAndWait(
                "credit_supported_type", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select the master card with unknown type and verify modifier is not applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify modifier for debit card is only applied for debit card.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithDebitModifiers() throws TimeoutException {
        // Debit mastercard card with complete set of information.
        mHelper.addServerCreditCard(new CreditCard("guid_1", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "5200828282828210",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.DEBIT, mBillingAddressId, "server-id-1"));
        mHelper.setCreditCardUseStatsForTesting("guid_1", 100, 5000);
        // Credit mastercard card with complete set of information.
        mHelper.addServerCreditCard(new CreditCard("guid_2", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "5454545454545454",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.CREDIT, mBillingAddressId, "server-id-2"));
        mHelper.setCreditCardUseStatsForTesting("guid_2", 1, 5000);
        mPaymentRequestTestRule.triggerUIAndWait(
                "debit_supported_type", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select the credit mastercard and verify modifier is not applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify modifier for credit visa card is only applied for credit visa card.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithCreditVisaModifiers() throws TimeoutException {
        // Credit visa card with complete set of information.
        mHelper.addServerCreditCard(new CreditCard("guid_1", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "4111111111111111",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.CREDIT, mBillingAddressId, "server-id-1"));
        mHelper.setCreditCardUseStatsForTesting("guid_1", 100, 5000);
        // Credit mastercard with complete set of information.
        mHelper.addServerCreditCard(new CreditCard("guid_2", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "5200828282828210",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.CREDIT, mBillingAddressId, "server-id-2"));
        mHelper.setCreditCardUseStatsForTesting("guid_2", 1, 5000);
        mPaymentRequestTestRule.triggerUIAndWait(
                "visa_supported_network", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith("Visa"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select the other credit mastercard and verify modifier is not applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify modifier for credit mastercard is only applied for credit mastercard.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithMasterCreditModifiers()
            throws TimeoutException {
        // Credit mastercard with complete set of information.
        mHelper.addServerCreditCard(new CreditCard("guid_1", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "5200828282828210",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.CREDIT, mBillingAddressId, "server-id-1"));
        mHelper.setCreditCardUseStatsForTesting("guid_1", 100, 5000);
        // Visa card with complete set of information and unknown type.
        mHelper.addServerCreditCard(new CreditCard("guid_2", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "4111111111111111",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.UNKNOWN, mBillingAddressId, "server-id-2"));
        mHelper.setCreditCardUseStatsForTesting("guid_2", 1, 5000);
        mPaymentRequestTestRule.triggerUIAndWait(
                "mastercard_supported_network", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select the other visa card with unknown type and verify modifier is not applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith("Visa"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify modifier for mastercard (any card type) is applied for mastercard.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalAndInstrumentLabelWithMastercardModifiers() throws TimeoutException {
        // Mastercard card with complete set of information and unknown type.
        String guid = mHelper.setCreditCard(new CreditCard("", "https://example.com",
                true /* isLocal */, true /* isCached */, "Jon Doe", "5555555555554444",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.UNKNOWN, mBillingAddressId, "" /* serverId */));
        mHelper.setCreditCardUseStatsForTesting(guid, 1000, 5000);

        // Credit mastercard with complete set of information.
        mHelper.addServerCreditCard(new CreditCard("guid_1", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "5200828282828210",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.CREDIT, mBillingAddressId, "server-id-1"));
        mHelper.setCreditCardUseStatsForTesting("guid_1", 100, 5000);

        // Visa card with complete set of information and unknown type.
        mHelper.addServerCreditCard(new CreditCard("guid_2", "https://example.com",
                false /* isLocal */, true /* isCached */, "Jon Doe", "4111111111111111",
                "" /* obfuscatedNumber */, "12", "2050", "mastercard", R.drawable.mc_card,
                CardType.UNKNOWN, mBillingAddressId, "server-id-2"));
        mHelper.setCreditCardUseStatsForTesting("guid_2", 1, 5000);

        mPaymentRequestTestRule.triggerUIAndWait(
                "mastercard_any_supported_type", mPaymentRequestTestRule.getReadyToPay());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select the other credit Mastercard and verify modifier is applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                1, mPaymentRequestTestRule.getReadyForInput());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "Mastercard"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        // select the other visa card with unknown type and verify modifier is not applied.
        mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                2, mPaymentRequestTestRule.getReadyForInput());
        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith("Visa"));
        assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    /**
     * Verify native app can pay as expected when modifier is applied.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppCanPayWithModifiers() throws TimeoutException {
        mPaymentRequestTestRule.installPaymentApp(HAVE_INSTRUMENTS, DELAYED_RESPONSE);
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_bobpay_discount", mPaymentRequestTestRule.getReadyToPay());

        assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith(
                "https://bobpay.com"));
        assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());

        mPaymentRequestTestRule.expectResultContains(
                new String[] {"https://bobpay.com", "\"transaction\"", "1337"});
    }

    /**
     * Verify modifier for credit visa card is only applied for service worker payment app with
     * credit visa card capabilities.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testUpdateTotalWithCreditVisaModifiersForServiceWorkerPaymentApp()
            throws TimeoutException {
        String[] bobpayMethodNames = {"https://bobpay.com", "basic-card"};
        int[] bobpayNetworks = {BasicCardNetwork.VISA};
        int[] bobPayTypes = {BasicCardType.CREDIT};
        ServiceWorkerPaymentApp.Capabilities[] bobpayCapabilities = {
                new ServiceWorkerPaymentApp.Capabilities(bobpayNetworks, bobPayTypes)};

        String[] alicepayMethodNames = {"https://alicepay.com", "basic-card"};
        int[] alicepayNetworks = {BasicCardNetwork.MASTERCARD};
        int[] alicepayTypes = {BasicCardType.CREDIT};
        ServiceWorkerPaymentApp.Capabilities[] alicepayCapabilities = {
                new ServiceWorkerPaymentApp.Capabilities(alicepayNetworks, alicepayTypes)};

        PaymentAppFactory.getInstance().addAdditionalFactory((webContents, methodNames,
                                                                     mayCrawlUnused, callback) -> {
            ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
            BitmapDrawable icon = new BitmapDrawable(activity.getResources(),
                    Bitmap.createBitmap(new int[] {Color.RED}, 1 /* width */, 1 /* height */,
                            Bitmap.Config.ARGB_8888));

            ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);
            callback.onPaymentAppCreated(
                    new ServiceWorkerPaymentApp(webContents, 0 /* registrationId */,
                            UriUtils.parseUriFromString("https://bobpay.com") /* scope */,
                            "BobPay" /* label */, "https://bobpay.com" /* sublabel*/,
                            "https://bobpay.com" /* tertiarylabel */, icon /* icon */,
                            bobpayMethodNames /* methodNames */, true /* explicitlyVerified */,
                            bobpayCapabilities /* capabilities */,
                            new String[0] /* preferredRelatedApplicationIds */,
                            new ServiceWorkerPaymentApp.SupportedDelegations()));
            callback.onPaymentAppCreated(
                    new ServiceWorkerPaymentApp(webContents, 0 /* registrationId */,
                            UriUtils.parseUriFromString("https://alicepay.com") /* scope */,
                            "AlicePay" /* label */, "https://bobpay.com" /* sublabel*/,
                            "https://alicepay.com" /* tertiarylabel */, icon /* icon */,
                            alicepayMethodNames /* methodNames */, true /* explicitlyVerified */,
                            alicepayCapabilities /* capabilities */,
                            new String[0] /* preferredRelatedApplicationIds */,
                            new ServiceWorkerPaymentApp.SupportedDelegations()));
            callback.onAllPaymentAppsCreated();
        });
        mPaymentRequestTestRule.triggerUIAndWait(
                "visa_supported_network", mPaymentRequestTestRule.getReadyToPay());

        if (mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().startsWith("BobPay")) {
            assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

            // select the alice pay and verify modifier is not applied.
            mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                    1, mPaymentRequestTestRule.getReadyForInput());
            assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
        } else {
            assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

            // select the bob pay and verify modifier is applied.
            mPaymentRequestTestRule.clickOnPaymentMethodSuggestionOptionAndWait(
                    1, mPaymentRequestTestRule.getReadyForInput());
            assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());
        }
    }
}
