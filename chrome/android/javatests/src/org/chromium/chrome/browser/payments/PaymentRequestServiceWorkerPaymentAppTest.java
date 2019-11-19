// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.payments.mojom.BasicCardType;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for service worker based payment apps.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        // For all the tests in this file, we expect abort exception when there is no supported
        // payment instruments instead of showing payment request UI.
        "enable-features=" + ChromeFeatureList.NO_CREDIT_CARD_ABORT})
public class PaymentRequestServiceWorkerPaymentAppTest {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule = new PaymentRequestTestRule(
            "payment_request_bobpay_and_basic_card_with_modifier_optional_data_test.html");

    /**
     * Installs a mock service worker based payment app with given supported delegations for
     * testing.
     *
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param capabilities         The capabilities of the mocked payment app.
     * @param name                 The name of the mocked payment app.
     * @param withIcon             Whether provide payment app icon.
     * @param supportedDelegations The supported delegations of the mock payment app.
     */
    private void installMockServiceWorkerPaymentApp(final String[] supportedMethodNames,
            final ServiceWorkerPaymentApp.Capabilities[] capabilities, final String name,
            final boolean withIcon,
            ServiceWorkerPaymentApp.SupportedDelegations supportedDelegations) {
        PaymentAppFactory.getInstance().addAdditionalFactory(
                (webContents, methodNames, mayCrawlUnused, callback) -> {
                    ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
                    BitmapDrawable icon = withIcon
                            ? new BitmapDrawable(activity.getResources(),
                                      Bitmap.createBitmap(new int[] {Color.RED}, 1 /* width */,
                                              1 /* height */, Bitmap.Config.ARGB_8888))
                            : null;
                    callback.onPaymentAppCreated(new ServiceWorkerPaymentApp(webContents,
                            0 /* registrationId */,
                            UriUtils.parseUriFromString("https://bobpay.com") /* scope */, name,
                            "test@bobpay.com" /* userHint */, "https://bobpay.com" /* origin */,
                            icon /* icon */, supportedMethodNames /* methodNames */,
                            true /* explicitlyVerified */, capabilities /* capabilities */,
                            new String[0] /* preferredRelatedApplicationIds */,
                            supportedDelegations));
                    callback.onAllPaymentAppsCreated();
                });
    }

    /**
     * Installs a mock service worker based payment app with no supported delegations for testing.
     *
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param capabilities         The capabilities of the mocked payment app.
     * @param withName             Whether provide payment app name.
     * @param withIcon             Whether provide payment app icon.
     */
    private void installMockServiceWorkerPaymentApp(final String[] supportedMethodNames,
            final ServiceWorkerPaymentApp.Capabilities[] capabilities, final boolean withName,
            final boolean withIcon) {
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities,
                withName ? "BobPay" : null, withIcon,
                new ServiceWorkerPaymentApp.SupportedDelegations());
    }

    /**
     * Installs a mock service worker based payment app for bobpay with given supported delegations
     * for testing.
     *
     * @param shippingAddress   Whether or not the mock payment app provides shipping address.
     * @param payerName         Whether or not the mock payment app provides payer's name.
     * @param payerPhone        Whether or not the mock payment app provides payer's phone number.
     * @param payerEmail        Whether or not the mock payment app provides payer's email address.
     * @param name              The name of the mocked payment app.
     */
    private void installMockServiceWorkerPaymentAppWithDelegations(final boolean shippingAddress,
            final boolean payerName, final boolean payerPhone, final boolean payerEmail,
            final String name) {
        String[] supportedMethodNames = {"https://bobpay.xyz"};
        installMockServiceWorkerPaymentApp(supportedMethodNames,
                new ServiceWorkerPaymentApp.Capabilities[0], name, true /*withIcon*/,
                new ServiceWorkerPaymentApp.SupportedDelegations(
                        shippingAddress, payerName, payerPhone, payerEmail));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods() throws TimeoutException {
        installMockServiceWorkerPaymentApp(
                new String[0], new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "Payment method not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasSupportedPaymentMethods() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoCapabilities() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {};
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities, true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The Bob Pay modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasVisaCreditCapabilities() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.VISA};
        int[] types = {BasicCardType.CREDIT};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks, types)};
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities, true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasMastercardCreditCapabilities() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.MASTERCARD};
        int[] types = {BasicCardType.CREDIT};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks, types)};
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities, true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should not apply.
        Assert.assertEquals("USD $5.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasVisaCreditAndDebitCapabilities() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        int[] networks = {BasicCardNetwork.VISA};
        int[] types = {BasicCardType.CREDIT, BasicCardType.DEBIT};
        ServiceWorkerPaymentApp.Capabilities[] capabilities = {
                new ServiceWorkerPaymentApp.Capabilities(networks, types)};
        installMockServiceWorkerPaymentApp(supportedMethodNames, capabilities, true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_all_cards_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_debit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_credit_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());

        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_visa_modifier", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
        // The modifier should apply.
        Assert.assertEquals("USD $4.00", mPaymentRequestTestRule.getOrderSummaryTotal());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDoNotCallCanMakePayment() throws TimeoutException {
        String[] supportedMethodNames = {"basic-card"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        // Sets setCanMakePaymentForTesting(false) to return false for CanMakePayment since there is
        // no real sw payment app, so if CanMakePayment is called then no payment instruments will
        // be available, otherwise CanMakePayment is not called.
        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(false);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfPaymentInstruments());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCallCanMakePayment() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com", "basic-card"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        // Sets setCanMakePaymentForTesting(false) to return false for CanMakePayment since there is
        // no real sw payment app, so if CanMakePayment is called then no payment instruments will
        // be available, otherwise CanMakePayment is not called.
        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(false);

        mPaymentRequestTestRule.openPageAndClickBuyAndWait(mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "Payment method not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanPreselect() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], true, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNotNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutName() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], false, true);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutIcon() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], true, false);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutNameAndIcon() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.com"};
        installMockServiceWorkerPaymentApp(
                supportedMethodNames, new ServiceWorkerPaymentApp.Capabilities[0], false, false);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingShippingComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(false /*shippingAddress*/,
                false /*payerName*/, false /*payerPhone*/, false /*payerEmail*/,
                "noSupportedDelegation" /*name*/);
        installMockServiceWorkerPaymentAppWithDelegations(true /*shippingAddress*/,
                false /*payerName*/, false /*payerPhone*/, false /*payerEmail*/,
                "shippingSupported" /*name */);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_shipping_requested", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        // The payment app which provides shipping address must be preselected.
        Assert.assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().contains(
                "shippingSupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingContactComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(false /*shippingAddress*/,
                false /*payerName*/, false /*payerPhone*/, false /*payerEmail*/,
                "noSupportedDelegation" /*name*/);
        installMockServiceWorkerPaymentAppWithDelegations(false /*shippingAddress*/,
                true /*payerName*/, true /*payerPhone*/, true /*payerEmail*/,
                "contactSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations(false /*shippingAddress*/,
                false /*payerName*/, false /*payerPhone*/, true /*payerEmail*/,
                "emailOnlySupported" /*name */);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_contact_requested", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(3, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        // The payment app which provides full contact details must be preselected.
        Assert.assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().contains(
                "contactSupported"));
        // The payment app which partially provides the required contact details comes before the
        // one that provides no contact information.
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(1).contains(
                "emailOnlySupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingAllRequiredInfoComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(true /*shippingAddress*/,
                false /*payerName*/, false /*payerPhone*/, false /*payerEmail*/,
                "shippingSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations(false /*shippingAddress*/,
                true /*payerName*/, true /*payerPhone*/, true /*payerEmail*/,
                "contactSupported" /*name */);
        installMockServiceWorkerPaymentAppWithDelegations(true /*shippingAddress*/,
                true /*payerName*/, true /*payerPhone*/, true /*payerEmail*/,
                "shippingAndContactSupported" /*name*/);

        ServiceWorkerPaymentAppBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait("buy_with_shipping_and_contact_requested",
                mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(3, mPaymentRequestTestRule.getNumberOfPaymentInstruments());

        // The payment app which provides all required information must be preselected.
        Assert.assertTrue(mPaymentRequestTestRule.getSelectedPaymentInstrumentLabel().contains(
                "shippingAndContactSupported"));
        // The payment app which provides shipping comes before the one which provides contact
        // details when both required by merchant.
        Assert.assertTrue(mPaymentRequestTestRule.getPaymentMethodSuggestionLabel(1).contains(
                "shippingSupported"));
    }
}
