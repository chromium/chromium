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

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.TestAwContentsClient;
import org.chromium.android_webview.test.TestWebMessageListener;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.payments.MockPaymentApp;
import org.chromium.components.payments.MockPaymentAppInstaller;
import org.chromium.components.payments.PaymentRequestTestWebPageContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests the default behavior of PaymentRequest when a WebView hasn't been explicitly configured for
 * it (i.e. AwSettings.setPaymentRequestEnabled(...) has not been called). This test should never be
 * migrated to Parameterized, as that may modify the setting.
 */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@SmallTest
public class AwPaymentRequestDefaultSettingsTest {
    private static final String PAYMENT_METHOD_NAME = "https://payments.test/web-pay";

    @Rule public AwActivityTestRule mActivityTestRule;
    private AwContents mAwContents;
    private TestWebMessageListener mWebMessageListener;
    private TestWebServer mMerchantServer;
    private MockPaymentAppInstaller mMockPaymentAppInstaller;

    public AwPaymentRequestDefaultSettingsTest() {
        this.mActivityTestRule = new AwActivityTestRule();
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
     * If the WEB_PAYMENTS feature is not explicitly modified and the WebView settings are not
     * changed, then the PaymentRequest interface is undefined in JavaScript.
     */
    @Test
    public void testPaymentRequestInterfaceUndefinedByDefault() throws Exception {
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkPaymentRequestDefined");

        Assert.assertEquals(
                "PaymentRequest is not defined.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
    }

    /**
     * By default, when no settings are changed, PaymentRequest API interface is undefined in
     * JavaScript.
     */
    @Test
    @EnableFeatures(ContentFeatures.WEB_PAYMENTS)
    public void testPaymentRequestJavaScriptInterfaceIsUndefinedWithDefaultSettings()
            throws Exception {
        // Intentionally do not enable or disable PaymentRequest API or hasEnrolledInstrument().
        loadPage();

        JSUtils.clickNodeWithUserGesture(
                mAwContents.getWebContents(), "checkPaymentRequestDefined");

        Assert.assertEquals(
                "PaymentRequest is not defined.",
                mWebMessageListener.waitForOnPostMessage().getAsString());
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
