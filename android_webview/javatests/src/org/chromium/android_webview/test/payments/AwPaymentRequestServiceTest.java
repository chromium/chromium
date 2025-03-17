// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.payments;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunnerWithParameters;
import org.chromium.android_webview.test.AwParameterizedTest;
import org.chromium.android_webview.test.AwSettingsMutation;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.TestAwContentsClient;
import org.chromium.android_webview.test.TestWebMessageListener;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.payments.MockPaymentApp;
import org.chromium.components.payments.MockPaymentAppInstaller;
import org.chromium.components.payments.PaymentRequestTestWebPageContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;

/** Tests that the PaymentRequest API works as expected in WebView. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwPaymentRequestServiceTest extends AwParameterizedTest {
    private static final String PAYMENT_METHOD_NAME = "https://payments.test/web-pay";
    private static final String OTHER_PAYMENT_METHOD_NAME =
            "https://other-payments.example/web-pay";

    @Rule public AwActivityTestRule mActivityTestRule;
    private MockPaymentAppInstaller mMockPaymentAppInstaller;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebMessageListener mWebMessageListener;
    private TestWebServer mMerchantServer;
    private PaymentRequestTestWebPageContents mPageContents;

    public AwPaymentRequestServiceTest(AwSettingsMutation params) {
        this.mActivityTestRule = new AwActivityTestRule(params.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mMockPaymentAppInstaller = new MockPaymentAppInstaller();

        mTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(new TestAwContentsClient());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "resultListener", new String[] {"*"}, mWebMessageListener);

        mMerchantServer = TestWebServer.start();
        mPageContents = new PaymentRequestTestWebPageContents();

        mAwContents.getSettings().setPaymentRequestEnabled(true);
    }

    @After
    public void tearDown() throws Exception {
        mMockPaymentAppInstaller.reset();
        mMerchantServer.close();
    }

    /**
     * Tests that disabling the WEB_PAYMENTS feature flag makes the PaymentRequest API not available
     * in WebView.
     */
    @Test
    @SmallTest
    @DisableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestIsNotDefined() throws Exception {
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkPaymentRequestDefined");

        Assert.assertEquals(
                "PaymentRequest is not defined.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that enabling the WEB_PAYMENTS feature flag makes the PaymentRequest API available in
     * WebView.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestIsDefined() throws Exception {
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkPaymentRequestDefined");

        Assert.assertEquals(
                "PaymentRequest is defined.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView returns the correct response when checking for ability to make payments
     * with a payment app that is not available on the device.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestCannotMakePaymentWithoutApps() throws Exception {
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest cannot make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView returns the correct response when checking for an enrolled instrument in a
     * payment app that is not available on the device.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestHasNoEnrolledInstrumentsWithoutApps() throws Exception {
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest does not have enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView returns the correct error message when attempting to launch a payment app
     * that is not available on the device.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestCannotLaunchAppsWithoutApps() throws Exception {
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        Assert.assertEquals(
                String.format("NotSupportedError: The payment method \"%s\" is not supported.",
                        PAYMENT_METHOD_NAME),
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /** Tests that WebView can check whether the installed payment app can make payments. */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestCanMakePayments() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest can make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView can check whether the installed payment app has an enrolled instrument.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestHasEnrolledInstrument() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest has enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /** Tests that WebView can invoke a payment app with PaymentRequest API. */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestLaunchPaymentApp() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(
                        String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
    }

    /**
     * If the merchant website supports more than one payment method in PaymentRequest API, but the
     * user has only one matching payment app on the device, then WebView should indicate that
     * payments are possible by returning "true" from the canMakePayment() API call.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestCanMakePaymentsWhenMerchantSupportsMultiplePaymentMethods()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest can make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * If the merchant website supports more than one payment method in PaymentRequest API, but the
     * user has only one matching payment app (and this app has an enrolled instrument), then
     * WebView should indicate that the user has an enrolled instrument by returning "true" from the
     * hasEnrolledInstrument() API call.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestHasEnrolledInstrumentWhenMerchantSupportsMultiplePaymentMethods()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest has enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * If the merchant website supports more than one payment method in PaymentRequest API, but the
     * user has only one matching payment app on the device, then WebView can invoke this one
     * payment app.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestLaunchPaymentAppWhenMerchantSupportsMultiplePaymentMethods()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(
                        String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
    }

    /**
     * If more than one app matches the PaymentRequest parameters from the merchant, then WebView
     * should indicate that payments are not possible with these parameters by returning "false"
     * from the canMakePayment() API call. This lets the merchant website gracefully fallback to
     * different PaymentRequest parameters or to skip using PaymentRequest API altogether.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestCannotMakePaymentsWithMoreThaOneAppAtOnce() throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp())
                .addApp(createOtherPaymentApp())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest cannot make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * If more than one app matches the PaymentRequest parameters from the merchant, then WebView
     * should indicate that the user has no enrolled instruments by returning "false" from the
     * hasEnrolledInstrument() API call. This lets the merchant website gracefully fallback to
     * different PaymentRequest parameters or to skip using PaymentRequest API altogether.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestHasNoEnrolledInstrumentWithMoreThaOneAppAtOnce()
            throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp())
                .addApp(createOtherPaymentApp())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest does not have enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * If more than one app matches the PaymentRequest parameters from the merchant, then show()
     * should return an error instead of launching an app, because WebView does not show UI for the
     * user to choose between their apps.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestCannotLaunchPaymentAppWithMoreThanOneAppAtOnce()
            throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp())
                .addApp(createOtherPaymentApp())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        Assert.assertEquals(
                "NotSupportedError: WebView supports launching only one payment app at a time.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /** Tests that retrying payment is disabled in the WebView implementation of PaymentRequest. */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCannotRetry() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(mPageContents.addMethod(PAYMENT_METHOD_NAME).build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "retryPayment");

        Assert.assertEquals(
                "NotSupportedError: PaymentResponse.retry() is disabled in WebView.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView indicates lack of ability to make payments when the merchant requests a
     * shipping address, but the user's payment app does not support returning a shipping address.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCannotMakePaymentsWithAddressWithAppThatCannotReturnAddress() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestShippingAddress().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest cannot make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView indicates lack of ability to make payments when the merchant requests
     * contact information, but the user's payment app does not support returning contact
     * information.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCannotMakePaymentsWithContactInfoWithAppThatCannotReturnContactInfo()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestContactInformation().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest cannot make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView confirms ability to make payments when the merchant requests a shipping
     * address and the user's payment app supports returning a shipping address.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCanMakePaymentsWithAddressWhenAppCanReturnAddress() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp().setHandlesShippingAddress()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestShippingAddress().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest can make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView confirms ability to make payments when the merchant requests contact
     * information and the user's payment app supports returning contact information.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCanMakePaymentsWithContactInfosWhenAppCanReturnContactInfos() throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp().setHandlesContactInformation())
                .install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestContactInformation().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest can make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView confirms ability to make payments when the merchant requests both shipping
     * address and contact information, and the user's payment app supports returning both of these.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCanMakePaymentsWithAddressAndContactWhenAppCanReturnBoth() throws Exception {
        mMockPaymentAppInstaller
                .addApp(
                        createPaymentApp()
                                .setHandlesShippingAddress()
                                .setHandlesContactInformation())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .requestShippingAddress()
                        .requestContactInformation()
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest can make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView indicates lack of enrolled instruments when the merchant requests a
     * shipping address, but the user's payment app does not support returning a shipping address.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testHasNoEnrolledInstrumentWithAddressWithAppThatCannotReturnAddress()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestShippingAddress().build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest does not have enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView indicates lack of enrolled instruments when the merchant requests contact
     * information, but the user's payment app does not support returning contact information.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testHasNoEnrolledInstrumentWithContactInfoWithAppThatCannotReturnContactInfo()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestContactInformation().build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest does not have enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that WebView indicates presence of enrolled instruments when the merchant requests a
     * shipping address and the user's payment app supports returning a shipping address.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testHasEnrolledInstrumentWithAddressWithAppThatCanReturnAddress() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp().setHandlesShippingAddress()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestShippingAddress().build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest has enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests presence of enrolled instruments when the merchant requests contact information and the
     * user's payment app supports returning contact information.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testHasEnrolledInstrumentWithContactInfoWithAppThatCanReturnContactInfo()
            throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp().setHandlesContactInformation())
                .install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestContactInformation().build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest has enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests presence of enrolled instruments when the merchant requests a shipping address and
     * contact information and the user's payment app supports returning both.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testHasEnrolledInstrumentWithAddressAndContactWithAppThatCanReturnBoth()
            throws Exception {
        mMockPaymentAppInstaller
                .addApp(
                        createPaymentApp()
                                .setHandlesShippingAddress()
                                .setHandlesContactInformation())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .requestShippingAddress()
                        .requestContactInformation()
                        .build());

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest has enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that it is not possible to invoke a payment app that does not support returning a
     * shipping address, if the merchant requests shipping address through PaymentRequest API in
     * WebView.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCannotLaunchAppWithoutShippingAddressWhenMerchantRequestsShippingAddress()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestShippingAddress().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        Assert.assertEquals(
                String.format(
                        "NotSupportedError: The payment method \"%s\" is not supported.",
                        PAYMENT_METHOD_NAME),
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests that it is not possible to invoke a payment app that does not support returning contact
     * information, if the merchant requests contact information through PaymentRequest API in
     * WebView.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCannotLaunchAppWithoutContactInfoWhenMerchantRequestsContactInfo()
            throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestContactInformation().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        Assert.assertEquals(
                String.format(
                        "NotSupportedError: The payment method \"%s\" is not supported.",
                        PAYMENT_METHOD_NAME),
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Tests launching a payment app that supports returning a shipping address when the merchant
     * requests a shipping address through PaymentRequest API in WebView.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testLaunchAppWithShippingAddress() throws Exception {
        mMockPaymentAppInstaller.addApp(createPaymentApp().setHandlesShippingAddress()).install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestShippingAddress().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
        Assert.assertTrue(
                "Shipping address should be in " + response,
                response.contains(String.format("\"shippingAddress\":{")));
        Assert.assertTrue(
                "Shipping address country code should be in " + response,
                response.contains(String.format("\"country\":\"CA\"")));
    }

    /**
     * Tests launching a payment app that supports returning contact information when the merchant
     * requests contact information through PaymentRequest API in WebView.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testLaunchAppWithContactInformation() throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp().setHandlesContactInformation())
                .install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestContactInformation().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
        Assert.assertTrue(
                "Payer name should be in " + response,
                response.contains(String.format("\"payerName\":\"John Smith\"")));
        Assert.assertTrue(
                "Payer phone should be in " + response,
                response.contains(String.format("\"payerPhone\":\"+15555555555\"")));
        Assert.assertTrue(
                "Payer email should be in " + response,
                response.contains(String.format("\"payerEmail\":\"John.Smith@gmail.com\"")));
    }

    /**
     * Tests launching a payment app that supports returning both shipping addresses and contact
     * information when the merchant requests both of these pieces of information.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testLaunchAppWithBothShippingAddressAndContactInformation() throws Exception {
        mMockPaymentAppInstaller
                .addApp(
                        createPaymentApp()
                                .setHandlesShippingAddress()
                                .setHandlesContactInformation())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .requestShippingAddress()
                        .requestContactInformation()
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
        Assert.assertTrue(
                "Shipping address should be in " + response,
                response.contains(String.format("\"shippingAddress\":{")));
        Assert.assertTrue(
                "Shipping address country code should be in " + response,
                response.contains(String.format("\"country\":\"CA\"")));
        Assert.assertTrue(
                "Payer name should be in " + response,
                response.contains(String.format("\"payerName\":\"John Smith\"")));
        Assert.assertTrue(
                "Payer phone should be in " + response,
                response.contains(String.format("\"payerPhone\":\"+15555555555\"")));
        Assert.assertTrue(
                "Payer email should be in " + response,
                response.contains(String.format("\"payerEmail\":\"John.Smith@gmail.com\"")));
    }

    /**
     * Tests launching a payment app that supports returning both shipping address and contact
     * information when a merchant requests a strictly smaller set set of capabilities: only
     * shipping address in this instance.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testLaunchWithBotShippingAddressAndContactInformationWhenMerchantWantsOnlyAddress()
            throws Exception {
        mMockPaymentAppInstaller
                .addApp(
                        createPaymentApp()
                                .setHandlesShippingAddress()
                                .setHandlesContactInformation())
                .install();
        loadMerchantCheckoutPage(
                mPageContents.addMethod(PAYMENT_METHOD_NAME).requestShippingAddress().build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
        Assert.assertTrue(
                "Shipping address should be in " + response,
                response.contains(String.format("\"shippingAddress\":{")));
        Assert.assertTrue(
                "Shipping address country code should be in " + response,
                response.contains(String.format("\"country\":\"CA\"")));
    }

    /**
     * Tests that the payment app that supports returning a shipping address is launched when the
     * merchant requests a shipping address on user's device that has two payment apps that match
     * the PaymentRequest API parameters, but one app does not support returning a shipping address.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testLaunchAppWithShippingAddressWhenMerchantRequestsItButOtherAppDoesNotSupportIt()
            throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp())
                .addApp(createOtherPaymentApp().setHandlesShippingAddress())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .requestShippingAddress()
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(
                        String.format("\"methodName\":\"%s\"", OTHER_PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
        Assert.assertTrue(
                "Shipping address should be in " + response,
                response.contains(String.format("\"shippingAddress\":{")));
        Assert.assertTrue(
                "Shipping address country code should be in " + response,
                response.contains(String.format("\"country\":\"CA\"")));
    }

    /**
     * Tests that the payment app that supports returning contact information is launched when the
     * merchant requests contact information on user's device that has two payment app that match
     * the PaymentRequest API parameters, but the second app does not support returning contact
     * information.
     */
    @Test
    @SmallTest
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testLaunchAppWithContactInfoWhenMerchantRequestsItButOtherappDoesNotSupportIt()
            throws Exception {
        mMockPaymentAppInstaller
                .addApp(createPaymentApp().setHandlesContactInformation())
                .addApp(createOtherPaymentApp())
                .install();
        loadMerchantCheckoutPage(
                mPageContents
                        .addMethod(PAYMENT_METHOD_NAME)
                        .addMethod(OTHER_PAYMENT_METHOD_NAME)
                        .requestContactInformation()
                        .build());

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
        Assert.assertTrue(
                "Payer name should be in " + response,
                response.contains(String.format("\"payerName\":\"John Smith\"")));
        Assert.assertTrue(
                "Payer phone should be in " + response,
                response.contains(String.format("\"payerPhone\":\"+15555555555\"")));
        Assert.assertTrue(
                "Payer email should be in " + response,
                response.contains(String.format("\"payerEmail\":\"John.Smith@gmail.com\"")));
    }

    /**
     * Loads a test web-page for exercising the PaymentRequest API.
     *
     * @param webPageContents The contents of the test web page to load.
     */
    private void loadMerchantCheckoutPage(String contents) throws Exception {
        String merchantCheckoutPageUrl =
                mMerchantServer.setResponse("/checkout", contents, /* responseHeaders= */ null);
        mActivityTestRule.loadUrlAsync(mAwContents, merchantCheckoutPageUrl);
        Data messageFromPage = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("Page loaded.", messageFromPage.getAsString());
    }

    private MockPaymentApp createPaymentApp() {
        return new MockPaymentApp()
                .setLabel("Test Payments App")
                .setPackage("test.payments.app")
                .setMethod(PAYMENT_METHOD_NAME)
                .setSignature("AABBCCDDEEFF001122334455")
                .setSha256CertificateFingerprint(
                        "79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9:BE:06:50:CE:E4:75:80:62:65:02:FB:"
                                + "F6:F9:91:AB:6E:BE:21:7E");
    }

    private MockPaymentApp createOtherPaymentApp() {
        return new MockPaymentApp()
                .setLabel("Other Test Payments App")
                .setPackage("test.payments.other.app")
                .setMethod(OTHER_PAYMENT_METHOD_NAME)
                .setSignature("001122334455AABBCCDDEEFF")
                .setSha256CertificateFingerprint(
                        "01:9D:A6:93:7D:A2:1D:64:25:D8:D4:93:37:29:55:20:D9:54:16:A0:99:DD:E3:CA:"
                                + "31:EE:94:A4:70:AD:BD:70");
    }
}
