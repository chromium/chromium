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
import org.chromium.components.payments.MockPaymentAppInstaller;
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

    public AwPaymentRequestServiceTest(AwSettingsMutation params) {
        this.mActivityTestRule = new AwActivityTestRule(params.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mMockPaymentAppInstaller =
                new MockPaymentAppInstaller(PAYMENT_METHOD_NAME, OTHER_PAYMENT_METHOD_NAME);

        mTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(new TestAwContentsClient());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "resultListener", new String[] {"*"}, mWebMessageListener);

        mMerchantServer = TestWebServer.start();
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
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ false);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ false);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ false);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ false);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ true);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ false);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ true);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ false);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ true);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ true);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ true);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ true);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ true);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ true);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ true);

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
        mMockPaymentAppInstaller.installPaymentApps(/* multipleApps= */ false);
        loadMerchantCheckoutPage(/* multiplePaymentMethods= */ false);

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "retryPayment");

        Assert.assertEquals(
                "NotSupportedError: PaymentResponse.retry() is disabled in WebView.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * Loads a test web-page for exercising the PaymentRequest API.
     *
     * @param multiplePaymentMethods Whether multiple payment methods should be requested in the
     *     PaymentRequest API call.
     */
    private void loadMerchantCheckoutPage(boolean multiplePaymentMethods) throws Exception {
        String checkoutPageHtmlFormat =
                """
            <!doctype html>
            <button id="checkPaymentRequestDefined">Check defined</button>
            <button id="checkCanMakePayment">Check can make payment</button>
            <button id="checkHasEnrolledInstrument">Check has enrolled instrument</button>
            <button id="launchPaymentApp">Launch payment app</button>
            <button id="retryPayment">Retry payment</button>

            <script>
              function createPaymentRequest() {
                const firstMethod = '%s';
                const secondMethod = '%s';
                const total = {label: 'Total', amount: {value: '0.01', currency: 'USD'}};
                return secondMethod
                       ? new PaymentRequest([{supportedMethods: firstMethod},
                                             {supportedMethods: secondMethod}], {total})
                       : new PaymentRequest([{supportedMethods: firstMethod}], {total});
              }

              function checkPaymentRequestDefined() {
                if (!window.PaymentRequest) {
                  resultListener.postMessage('PaymentRequest is not defined.');
                } else {
                  resultListener.postMessage('PaymentRequest is defined.');
                }
              }

              async function checkCanMakePayment() {
                try {
                  const request = createPaymentRequest();
                  if (await request.canMakePayment()) {
                    resultListener.postMessage('PaymentRequest can make payments.');
                  } else {
                    resultListener.postMessage('PaymentRequest cannot make payments.');
                  }
                } catch (e) {
                  resultListener.postMessage(e.toString());
                }
              }

              async function checkHasEnrolledInstrument() {
                try {
                  const request = createPaymentRequest();
                  if (await request.hasEnrolledInstrument()) {
                    resultListener.postMessage('PaymentRequest has enrolled instrument.');
                  } else {
                    resultListener.postMessage('PaymentRequest does not have enrolled instrument.');
                  }
                } catch (e) {
                  resultListener.postMessage(e.toString());
                }
              }

              async function launchPaymentApp() {
                try {
                  const request = createPaymentRequest();
                  const response = await request.show();
                  await response.complete('success');
                  resultListener.postMessage(JSON.stringify(response));
                } catch (e) {
                  resultListener.postMessage(e.toString());
                }
              }

              async function retryPayment() {
                try {
                  const request = createPaymentRequest();
                  let response = await request.show();
                  response = await response.retry();
                  await response.complete('success');
                  resultListener.postMessage(JSON.stringify(response));
                } catch (e) {
                  resultListener.postMessage(e.toString());
                }
              }

              document.getElementById('checkPaymentRequestDefined')
                  .addEventListener('click', checkPaymentRequestDefined);
              document.getElementById('checkCanMakePayment')
                  .addEventListener('click', checkCanMakePayment);
              document.getElementById('checkHasEnrolledInstrument')
                  .addEventListener('click', checkHasEnrolledInstrument);
              document.getElementById('launchPaymentApp')
                  .addEventListener('click', launchPaymentApp);
              document.getElementById('retryPayment')
                  .addEventListener('click', retryPayment);

              resultListener.postMessage('Page loaded.');
            </script>
            """;

        String merchantCheckoutPageUrl =
                mMerchantServer.setResponse(
                        "/checkout",
                        String.format(
                                checkoutPageHtmlFormat,
                                PAYMENT_METHOD_NAME,
                                multiplePaymentMethods ? OTHER_PAYMENT_METHOD_NAME : ""),
                        /* responseHeaders= */ null);
        mActivityTestRule.loadUrlAsync(mAwContents, merchantCheckoutPageUrl);
        Data messageFromPage = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals("Page loaded.", messageFromPage.getAsString());
    }
}
