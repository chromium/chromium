// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.payments;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

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
import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.payments.AndroidIntentLauncher;
import org.chromium.components.payments.AndroidPaymentAppFinder;
import org.chromium.components.payments.MockPackageManagerDelegate;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Tests that the PaymentRequest API works as expected in WebView. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwPaymentRequestServiceTest extends AwParameterizedTest {
    private static final String PAYMENT_METHOD_NAME = "https://payments.test/web-pay";
    private static final String OTHER_PAYMENT_METHOD_NAME =
            "https://other-payments.example/web-pay";

    @Rule public AwActivityTestRule mActivityTestRule;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebMessageListener mWebMessageListener;
    private TestWebServer mMerchantServer;

    public AwPaymentRequestServiceTest(AwSettingsMutation params) {
        this.mActivityTestRule = new AwActivityTestRule(params.getMutation());
    }

    @Before
    public void setUp() throws Exception {
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
        AndroidPaymentAppFinder.setPackageManagerDelegateForTest(null);
        AndroidPaymentAppFinder.setDownloaderForTest(null);
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
        installPaymentApps(/* multipleApps= */ false);
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
        installPaymentApps(/* multipleApps= */ false);
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
        installPaymentApps(/* multipleApps= */ false);
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
        installPaymentApps(/* multipleApps= */ false);
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
        installPaymentApps(/* multipleApps= */ false);
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
        installPaymentApps(/* multipleApps= */ false);
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
        installPaymentApps(/* multipleApps= */ true);
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
        installPaymentApps(/* multipleApps= */ true);
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
        installPaymentApps(/* multipleApps= */ true);
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
        installPaymentApps(/* multipleApps= */ false);
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

    /**
     * Injects a fake Android payment app into the package manager delegate, with the correct
     * signature being returned from the downloader. Also turns off connecting to the
     * IS_READY_TO_PAY service or sending the PAY intent to this app.
     *
     * @param multipleApps Whether multiple apps should be installed.
     */
    private void installPaymentApps(boolean multipleApps) {
        MockPackageManagerDelegate packageManagerDelegate = new MockPackageManagerDelegate();
        // The SHA256 of the string "AABBCCDDEEFF001122334455" equals to the fingerprints[0].value
        // in the "downloaded" manifest file.
        packageManagerDelegate.installPaymentApp(
                "Test Payment App",
                "test.payments.app",
                PAYMENT_METHOD_NAME,
                "AABBCCDDEEFF001122334455");
        if (multipleApps) {
            // The SHA256 of the string "001122334455AABBCCDDEEFF" equals to the
            // fingerprints[0].value in the "downloaded" manifest file.
            packageManagerDelegate.installPaymentApp(
                    "Other Test Payment App",
                    "test.payments.other.app",
                    OTHER_PAYMENT_METHOD_NAME,
                    "001122334455AABBCCDDEEFF");
        }
        AndroidPaymentAppFinder.setPackageManagerDelegateForTest(packageManagerDelegate);
        AndroidPaymentAppFinder.setDownloaderForTest(new MockPaymentManifestDownloader());
        AndroidPaymentAppFinder.setAndroidIntentLauncherForTest(new MockAndroidIntentLauncher());
        AndroidPaymentAppFinder.bypassIsReadyToPayServiceInTest();
    }

    /**
     * An override for the downloader with static responses instead of querying servers on the
     * network.
     */
    private static final class MockPaymentManifestDownloader extends PaymentManifestDownloader {
        // The fingerprints[0].value is the SHA256 of the string "AABBCCDDEEFF001122334455".
        private static final String MANIFEST_JSON =
                """
            {
              "default_applications": ["/web-pay/manifest.json"],
              "related_applications": [{
                  "platform": "play",
                  "id": "test.payments.app",
                  "min_version": "1",
                  "fingerprints": [{
                    "type": "sha256_cert",
                    "value": "79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9:BE:06:50:CE:E4:75:80:62:65:02:FB:F6:F9:91:AB:6E:BE:21:7E"
                  }]
              }]
            }
            """;
        // The fingerprints[0].value is the SHA256 of the string "001122334455AABBCCDDEEFF".
        private static final String OTHER_MANIFEST_JSON =
                """
            {
              "default_applications": ["/web-pay/manifest.json"],
              "related_applications": [{
                  "platform": "play",
                  "id": "test.payments.other.app",
                  "min_version": "1",
                  "fingerprints": [{
                    "type": "sha256_cert",
                    "value": "01:9D:A6:93:7D:A2:1D:64:25:D8:D4:93:37:29:55:20:D9:54:16:A0:99:DD:E3:CA:31:EE:94:A4:70:AD:BD:70"
                  }]
              }]
            }
            """;

        @Override
        public void downloadPaymentMethodManifest(
                Origin merchantOrigin, GURL url, ManifestDownloadCallback callback) {
            callback.onPaymentMethodManifestDownloadSuccess(
                    url,
                    Origin.create(url),
                    url.getSpec().equals(PAYMENT_METHOD_NAME)
                            ? MANIFEST_JSON
                            : OTHER_MANIFEST_JSON);
        }

        @Override
        public void downloadWebAppManifest(
                Origin paymentMethodManifestOrigin, GURL url, ManifestDownloadCallback callback) {
            callback.onWebAppManifestDownloadSuccess(
                    url.getSpec().startsWith(PAYMENT_METHOD_NAME)
                            ? MANIFEST_JSON
                            : OTHER_MANIFEST_JSON);
        }
    }

    /**
     * An app launcher that does not fire off any Android intents, but instead immediately returns a
     * successful intent result.
     */
    private static final class MockAndroidIntentLauncher implements AndroidIntentLauncher {
        @Override
        public void launchPaymentApp(
                Intent intent,
                Callback<String> errorCallback,
                WindowAndroid.IntentCallback intentCallback) {
            Bundle launchParameters = intent.getExtras();
            String paymentMethodName = launchParameters.getStringArrayList("methodNames").get(0);

            Intent response = new Intent();
            Bundle extras = new Bundle();
            extras.putString("methodName", paymentMethodName);
            extras.putString("details", "{\"key\": \"value\"}");
            response.putExtras(extras);
            intentCallback.onIntentCompleted(Activity.RESULT_OK, response);
        }
    }
}
