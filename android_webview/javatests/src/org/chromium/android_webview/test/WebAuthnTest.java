// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

/** Test WebAuthn settings in WebView. */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Batch(Batch.PER_CLASS)
public class WebAuthnTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private AwSettings mAwSettings;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mAwSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testSetWebAuthnSupportFromNonUIThread() throws Throwable {
        // Call setWebauthnSupport from the instrumentation thread (non-UI thread).
        // This should not crash.
        mAwSettings.setWebauthnSupport(WebauthnMode.APP);

        // Verify the setting was applied.
        Assert.assertEquals(WebauthnMode.APP, mAwSettings.getWebauthnSupport());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @DisableFeatures(AwFeatures.WEBVIEW_FORCE_WEB_AUTHN)
    public void testWebAuthnDefaultDisabled() throws Throwable {
        mAwSettings.setJavaScriptEnabled(true);

        Assert.assertEquals(
                "WebAuthn default should be NONE by default.",
                WebauthnMode.NONE,
                mAwSettings.getWebauthnSupport());

        final String pageUrl =
                mWebServer.setResponse("/test.html", "<html><body></body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertFalse(
                "WebAuthn JavaScript interface should NOT be exposed",
                hasWebAuthnJavaScriptInterfaces());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @EnableFeatures(AwFeatures.WEBVIEW_FORCE_WEB_AUTHN)
    public void testWebAuthnEnabledByFlag() throws Throwable {
        mAwSettings.setJavaScriptEnabled(true);

        Assert.assertEquals(
                "WebAuthn default should be APP mode when the flag is enabled.",
                WebauthnMode.APP,
                mAwSettings.getWebauthnSupport());

        final String pageUrl =
                mWebServer.setResponse("/test.html", "<html><body></body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertTrue(
                "WebAuthn JavaScript interface should be exposed",
                hasWebAuthnJavaScriptInterfaces());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @EnableFeatures(AwFeatures.WEBVIEW_FORCE_WEB_AUTHN)
    public void testWebAuthnFlagAndAppSetting() throws Throwable {
        mAwSettings.setJavaScriptEnabled(true);

        Assert.assertEquals(
                "WebAuthn default should be APP mode when the flag is enabled.",
                WebauthnMode.APP,
                mAwSettings.getWebauthnSupport());

        final String pageUrl =
                mWebServer.setResponse("/test.html", "<html><body></body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertTrue(
                "WebAuthn JavaScript interface should be exposed",
                hasWebAuthnJavaScriptInterfaces());

        // Change the mode and reload the apge. Verify that the interfaces disappear again.
        mAwSettings.setWebauthnSupport(WebauthnMode.NONE);
        Assert.assertEquals(
                "setWebauthnSupport() API should still take effect.",
                WebauthnMode.NONE,
                mAwSettings.getWebauthnSupport());
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertFalse(
                "WebAuthn JavaScript interface should NOT be exposed",
                hasWebAuthnJavaScriptInterfaces());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetWebAuthnSupportLogsPermissionStatus() throws Throwable {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.Webauthn.BrowserModePermissionGranted", false);

        // Since the test app does not have CREDENTIAL_MANAGER_SET_ORIGIN permission,
        // this call should log `false` to the histogram and succeed without throwing.
        mAwSettings.setWebauthnSupport(WebauthnMode.BROWSER);

        histogramWatcher.assertExpected();
    }

    private static boolean isSecureDomain(GURL url) {
        if ("https".equals(url.getScheme())) {
            return true;
        }
        if ("http".equals(url.getScheme()) && "localhost".equals(url.getHost())) {
            return true;
        }
        if ("http".equals(url.getScheme()) && "127.0.0.1".equals(url.getHost())) {
            return true;
        }
        return false;
    }

    private boolean hasWebAuthnJavaScriptInterfaces() throws Throwable {
        if (!isSecureDomain(mAwContents.getUrl())) {
            throw new Exception(
                    "This web URL ("
                            + mAwContents.getUrl()
                            + ") is insecure, however WebAuthn interfaces are only ever exposed for"
                            + " secure web schemes. Please rewrite this test case so that it uses"
                            + " a localhost HTTP server (locahost is treated as 'trusted' for"
                            + " testing purposes).");
        }
        String jsResult =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents,
                        mContentsClient,
                        "typeof window.PublicKeyCredential !== 'undefined'");
        return "true".equals(jsResult);
    }
}
