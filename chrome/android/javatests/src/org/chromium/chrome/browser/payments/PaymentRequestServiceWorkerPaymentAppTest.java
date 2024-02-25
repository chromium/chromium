// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppService;
import org.chromium.components.payments.PaymentAppServiceBridge;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.SupportedDelegations;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/** A payment integration test for service worker based payment apps. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Prevent crawling the web for real payment apps.
    "disable-features=" + PaymentFeatureList.SERVICE_WORKER_PAYMENT_APPS
})
public class PaymentRequestServiceWorkerPaymentAppTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule(
                    "payment_request_bobpay_and_basic_card_with_modifier_optional_data_test.html");

    /**
     * Installs a mock service worker based payment app with given supported delegations for
     * testing.
     *
     * @param scope Service worker scope that identifies the payment app. Must be unique.
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param name The name of the mocked payment app.
     * @param withIcon Whether provide payment app icon.
     * @param supportedDelegations The supported delegations of the mock payment app.
     */
    private void installMockServiceWorkerPaymentApp(
            String scope,
            String[] supportedMethodNames,
            String name,
            boolean withIcon,
            SupportedDelegations supportedDelegations) {
        PaymentAppService.getInstance()
                .addFactory(
                        new PaymentAppFactoryInterface() {
                            @Override
                            public void create(PaymentAppFactoryDelegate delegate) {
                                WebContents webContents = delegate.getParams().getWebContents();
                                Activity activity =
                                        ActivityUtils.getActivityFromWebContents(webContents);
                                BitmapDrawable icon =
                                        withIcon
                                                ? new BitmapDrawable(
                                                        activity.getResources(),
                                                        Bitmap.createBitmap(
                                                                new int[] {Color.RED},
                                                                /* width= */ 1,
                                                                /* height= */ 1,
                                                                Bitmap.Config.ARGB_8888))
                                                : null;
                                delegate.onCanMakePaymentCalculated(true);
                                delegate.onPaymentAppCreated(
                                        new MockPaymentApp(
                                                /* identifier= */ scope,
                                                name,
                                                icon,
                                                supportedMethodNames,
                                                supportedDelegations));
                                delegate.onDoneCreatingPaymentApps(this);
                            }
                        });
    }

    /**
     * Installs a mock service worker based payment app with no supported delegations for testing.
     *
     * @param scope The service worker scope that identifies this payment app. Must be unique.
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param withName Whether provide payment app name.
     * @param withIcon Whether provide payment app icon.
     */
    private void installMockServiceWorkerPaymentApp(
            String scope, String[] supportedMethodNames, boolean withName, boolean withIcon) {
        installMockServiceWorkerPaymentApp(
                scope,
                supportedMethodNames,
                withName ? "BobPay" : null,
                withIcon,
                new SupportedDelegations());
    }

    /**
     * Installs a mock service worker based payment app for bobpay with given supported delegations
     * for testing.
     *
     * @param scope The service worker scope that identifies this payment app. Must be unique.
     * @param shippingAddress Whether or not the mock payment app provides shipping address.
     * @param payerName Whether or not the mock payment app provides payer's name.
     * @param payerPhone Whether or not the mock payment app provides payer's phone number.
     * @param payerEmail Whether or not the mock payment app provides payer's email address.
     * @param name The name of the mocked payment app.
     */
    private void installMockServiceWorkerPaymentAppWithDelegations(
            String scope,
            boolean shippingAddress,
            boolean payerName,
            boolean payerPhone,
            boolean payerEmail,
            String name) {
        String[] supportedMethodNames = {"https://bobpay.xyz"};
        installMockServiceWorkerPaymentApp(
                scope,
                supportedMethodNames,
                name,
                /* withIcon= */ true,
                new SupportedDelegations(shippingAddress, payerName, payerPhone, payerEmail));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testNoSupportedPaymentMethods() throws TimeoutException {
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy_with_bobpay", mPaymentRequestTestRule.getShowFailed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"show() rejected", "The payment method", "not supported"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testHasSupportedPaymentMethods() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.test"};
        installMockServiceWorkerPaymentApp("https://bobpay.test", supportedMethodNames, true, true);

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);
        // Payment sheet skips to the app since it is the only available app.
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDoNotCallCanMakePayment() throws TimeoutException {
        String[] supportedMethodNames1 = {"https://bobpay.test"};
        installMockServiceWorkerPaymentApp(
                "https://bobpay.test", supportedMethodNames1, true, true);

        String[] supportedMethodNames2 = {"https://kylepay.test/webpay"};
        installMockServiceWorkerPaymentApp(
                "https://kylepay.test/webpay", supportedMethodNames2, true, true);

        // Sets setCanMakePaymentForTesting(false) to return false for CanMakePayment since there is
        // no real sw payment app, so if CanMakePayment is called then no payment apps will be
        // available, otherwise CanMakePayment is not called.
        PaymentAppServiceBridge.setCanMakePaymentForTesting(false);

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfPaymentApps());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanPreselect() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.test"};
        installMockServiceWorkerPaymentApp("https://bobpay.test", supportedMethodNames, true, true);

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);

        // Payment sheet skips to the app since it is the only available app.
        mPaymentRequestTestRule.clickNodeAndWait("buy", mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutName() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.test"};
        installMockServiceWorkerPaymentApp(
                "https://bobpay.test", supportedMethodNames, false, true);

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentAppLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutIcon() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.test"};
        installMockServiceWorkerPaymentApp(
                "https://bobpay.test", supportedMethodNames, true, false);

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentAppLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanNotPreselectWithoutNameAndIcon() throws TimeoutException {
        String[] supportedMethodNames = {"https://bobpay.test"};
        installMockServiceWorkerPaymentApp(
                "https://bobpay.test", supportedMethodNames, false, false);

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertNull(mPaymentRequestTestRule.getSelectedPaymentAppLabel());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingShippingComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://alicepay.test",
                /* shippingAddress= */ false,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "noSupportedDelegation");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://bobpay.test",
                /* shippingAddress= */ true,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "shippingSupported1");
        // Install the second app supporting shipping delegation to force showing payment sheet.
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://charliepay.test",
                /* shippingAddress= */ true,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "shippingSupported2");

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_shipping_requested", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(3, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // The payment app which provides shipping address must be preselected.
        Assert.assertTrue(
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().contains("shippingSupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingContactComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://alicepay.test",
                /* shippingAddress= */ false,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "noSupportedDelegation");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://bobpay.test",
                /* shippingAddress= */ false,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "contactSupported");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://charliepay.test",
                /* shippingAddress= */ false,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ true,
                /* name= */ "emailOnlySupported");
        // Install the second app supporting contact delegation to force showing payment sheet.
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://davepay.test",
                /* shippingAddress= */ false,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "contactSupported2");

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_contact_requested", mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // The payment app which provides full contact details must be preselected.
        Assert.assertTrue(
                mPaymentRequestTestRule.getSelectedPaymentAppLabel().contains("contactSupported"));
        // The payment app which partially provides the required contact details comes before the
        // one that provides no contact information.
        Assert.assertTrue(
                mPaymentRequestTestRule
                        .getPaymentMethodSuggestionLabel(2)
                        .contains("emailOnlySupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentAppProvidingAllRequiredInfoComesFirst() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://alicepay.test",
                /* shippingAddress= */ true,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "shippingSupported");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://bobpay.test",
                /* shippingAddress= */ false,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "contactSupported");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://charliepay.test",
                /* shippingAddress= */ true,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "shippingAndContactSupported");
        // Install the second app supporting both shipping and contact delegations to force showing
        // payment sheet.
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://davepay.test",
                /* shippingAddress= */ true,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "shippingAndContactSupported2");

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);

        mPaymentRequestTestRule.triggerUIAndWait(
                "buy_with_shipping_and_contact_requested",
                mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfPaymentApps());

        // The payment app which provides all required information must be preselected.
        Assert.assertTrue(
                mPaymentRequestTestRule
                        .getSelectedPaymentAppLabel()
                        .contains("shippingAndContactSupported"));
        // The payment app which provides shipping comes before the one which provides contact
        // details when both required by merchant.
        Assert.assertTrue(
                mPaymentRequestTestRule
                        .getPaymentMethodSuggestionLabel(2)
                        .contains("shippingSupported"));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipsToSinglePaymentAppProvidingShipping() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://alicepay.test",
                /* shippingAddress= */ false,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "noSupportedDelegation");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://bobpay.test",
                /* shippingAddress= */ true,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "shippingSupported");

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy_with_shipping_requested", mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipsToSinglePaymentAppProvidingContact() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://alicepay.test",
                /* shippingAddress= */ false,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "noSupportedDelegation");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://bobpay.test",
                /* shippingAddress= */ false,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "contactSupported");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://charliepay.test",
                /* shippingAddress= */ false,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ true,
                /* name= */ "emailOnlySupported");

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy_with_contact_requested", mPaymentRequestTestRule.getDismissed());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipsToSinglePaymentAppProvidingAllRequiredInfo() throws TimeoutException {
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://alicepay.test",
                /* shippingAddress= */ true,
                /* payerName= */ false,
                /* payerPhone= */ false,
                /* payerEmail= */ false,
                /* name= */ "shippingSupported");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://bobpay.test",
                /* shippingAddress= */ false,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "contactSupported");
        installMockServiceWorkerPaymentAppWithDelegations(
                /* scope= */ "https://charliepay.test",
                /* shippingAddress= */ true,
                /* payerName= */ true,
                /* payerPhone= */ true,
                /* payerEmail= */ true,
                /* name= */ "shippingAndContactSupported");

        PaymentAppServiceBridge.setCanMakePaymentForTesting(true);
        mPaymentRequestTestRule.clickNodeAndWait(
                "buy_with_shipping_and_contact_requested", mPaymentRequestTestRule.getDismissed());
    }
}
