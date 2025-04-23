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
import org.chromium.android_webview.test.TestAwContentsClient;
import org.chromium.android_webview.test.TestWebMessageListener;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.payments.MockPaymentApp;
import org.chromium.components.payments.MockPaymentAppInstaller;
import org.chromium.components.payments.PaymentRequestTestWebPageContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests that the WebView settings for PaymentRequest and hasEnrolledInstrument() work as expected.
 * All test cases are setup to have one payment app that is being requested by the merchant website.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
@SmallTest
public class AwPaymentRequestSettingsTest extends AwParameterizedTest {
    private static final String PAYMENT_METHOD_NAME = "https://payments.test/web-pay";

    @Rule public AwActivityTestRule mActivityTestRule;
    private AwContents mAwContents;
    private TestWebMessageListener mWebMessageListener;
    private TestWebServer mMerchantServer;
    private MockPaymentAppInstaller mMockPaymentAppInstaller;

    public AwPaymentRequestSettingsTest(AwSettingsMutation params) {
        this.mActivityTestRule = new AwActivityTestRule(params.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(new TestAwContentsClient())
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "resultListener", new String[] {"*"}, mWebMessageListener);

        mMerchantServer = TestWebServer.start();

        mMockPaymentAppInstaller = new MockPaymentAppInstaller();
        mMockPaymentAppInstaller
                .addApp(
                        new MockPaymentApp()
                                .setLabel("Test Payments App")
                                .setPackage("test.payments.app")
                                .setMethod(PAYMENT_METHOD_NAME)
                                .setSignature("AABBCCDDEEFF001122334455")
                                .setSha256CertificateFingerprint(
                                        "79:5C:8E:4D:57:7B:76:49:3A:0A:0B:93:B9:BE:06:50:CE:E4:75:"
                                                + "80:62:65:02:FB:F6:F9:91:AB:6E:BE:21:7E"))
                .install();
    }

    @After
    public void tearDown() throws Exception {
        mMockPaymentAppInstaller.reset();
        mMerchantServer.close();
    }

    /**
     * If the WEB_PAYMENTS feature flag is disabled, then the PaymentRequest API interface stays
     * undefined in JavaScript, even if the WebView setting for PaymentRequest API is enabled.
     */
    @Test
    @DisableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestJavaScriptInterfaceIsUndefinedWithoutWebPaymentsFeatureFlag()
            throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(true);
        mAwContents.getSettings().setHasEnrolledInstrumentEnabled(true);
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkPaymentRequestDefined");

        Assert.assertEquals(
                "PaymentRequest is not defined.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * The PaymentRequest API is defined in JavaScript when the PaymentRequest API is enabled in
     * WebView settings.
     */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestIsDefinedInJavaScriptWhenWebViewSettingEnabled()
            throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(true);
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkPaymentRequestDefined");

        Assert.assertEquals(
                "PaymentRequest is defined.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * The PaymentRequest API is undefined in JavaScript when the PaymentRequest API is disabled in
     * WebView settings.
     */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestIsUndefinedInJavaScriptEvenWhenWebViewSettingDisabled()
            throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(false);
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkPaymentRequestDefined");

        Assert.assertEquals(
                "PaymentRequest is not defined.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /** Can make payments when PaymentRequest API is enabled. */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testCanMakePaymentWhenPaymentRequestIsEnabled() throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(true);
        loadPage();

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "checkCanMakePayment");

        Assert.assertEquals(
                "PaymentRequest can make payments.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * By default, if the has enrolled instrument setting is not set, but the PaymentRequest API is
     * enabled, then the PaymentRequest.hasEnrolledInstrument() answers truthfully, which in this
     * test case setup means there is an enrolled instrument.
     */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testDefaultSettingForHasEnrolledInstrumentIsTrue() throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(true);
        // Intentionally do not modify the setting for the hasEnrolledInstrument().
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest has enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /** Has enrolled instrument when the corresponding setting is enabled. */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testHasEnrolledInstrumentIsTruthfulWhenThatSettingIsEnabled() throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(true);
        mAwContents.getSettings().setHasEnrolledInstrumentEnabled(true);
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest has enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /** No enrolled instrument when the corresponding setting is disabled. */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testNoEnrolledInstrumentWhenThatSettingIsDisabled() throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(true);
        mAwContents.getSettings().setHasEnrolledInstrumentEnabled(false);
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkHasEnrolledInstrument");

        Assert.assertEquals(
                "PaymentRequest does not have enrolled instrument.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /** Successful launch of payment app when PaymentRequest API is enabled. */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testLaunchPaymentAppWithEnabledPaymentRequest() throws Exception {
        mAwContents.getSettings().setPaymentRequestEnabled(true);
        loadPage();

        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "launchPaymentApp");

        String response = mWebMessageListener.waitForOnPostMessage().getAsString();
        Assert.assertTrue(
                response.contains(String.format("\"methodName\":\"%s\"", PAYMENT_METHOD_NAME)));
        Assert.assertTrue(response.contains(String.format("\"details\":{\"key\":\"value\"}")));
    }

    private void loadPage() throws Exception {
        mActivityTestRule.loadUrlAsync(
                mAwContents,
                mMerchantServer.setResponse(
                        "/checkout",
                        new PaymentRequestTestWebPageContents()
                                .addMethod(PAYMENT_METHOD_NAME)
                                .build(),
                        /* responseHeaders= */ null));
        Assert.assertEquals(
                "Page loaded.", mWebMessageListener.waitForOnPostMessage().getAsString());
    }
}
