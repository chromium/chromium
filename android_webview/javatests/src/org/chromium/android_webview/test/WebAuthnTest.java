// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.PendingIntent;
import android.net.Uri;
import android.os.Build;
import android.os.ResultReceiver;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import com.google.android.gms.tasks.OnFailureListener;
import com.google.android.gms.tasks.OnSuccessListener;

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
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.webauthn.AuthenticationContextProvider;
import org.chromium.components.webauthn.Fido2ApiCallHelper;
import org.chromium.components.webauthn.GmsCoreUtils;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.cred_man.CredManSupportProvider;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

/** Test WebAuthn settings in WebView. */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Batch(Batch.PER_CLASS)
public class WebAuthnTest {
    private static class TestFido2ApiCallHelper extends Fido2ApiCallHelper {
        public boolean mGetAssertionCalled;

        @Override
        public void invokeFido2GetAssertion(
                AuthenticationContextProvider authenticationContextProvider,
                PublicKeyCredentialRequestOptions options,
                Uri uri,
                byte[] clientDataHash,
                ResultReceiver resultReceiver,
                OnSuccessListener<PendingIntent> successCallback,
                OnFailureListener failureCallback) {
            mGetAssertionCalled = true;
            failureCallback.onFailure(new Exception("MOCK_FIDO2_API_INVOKED"));
        }
    }

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

        // We need to mock out the GMS Core version, otherwise Blink will eagerly declare that
        // WebAuthn is unsupported. The actual GMS Core version does not matter, because test cases
        // are responsible for intercepting WebAuthn requests before they are actually delivered to
        // GMS Core.
        GmsCoreUtils.setGmsCoreVersionForTesting(240700000);
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

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWebAuthnBlockedOnSslError() throws Throwable {
        CredManSupportProvider.setupForTesting(Build.VERSION_CODES.TIRAMISU, false);
        TestFido2ApiCallHelper testHelper = new TestFido2ApiCallHelper();
        Fido2ApiCallHelper.overrideInstanceForTesting(testHelper);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_MISMATCHED_NAME);
        try {
            mAwSettings.setJavaScriptEnabled(true);
            mAwSettings.setWebauthnSupport(WebauthnMode.APP);
            mContentsClient.setAllowSslError(true);

            final String pageUrl =
                    testServer.getURLWithHostName(
                            "a.test", "/android_webview/test/data/hello_world.html");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents,
                    mContentsClient,
                    """
                    window.webauthnResult = 'PENDING';
                    navigator.credentials.get({
                      publicKey: {
                        challenge: new Uint8Array([1, 2, 3, 4]),
                        timeout: 500,
                        rpId: 'a.test'
                      }
                    }).then(
                      () => { window.webauthnResult = 'SUCCESS'; },
                      (e) => { window.webauthnResult = e.name + ': ' + e.message; }
                    );
                    """);

            AwActivityTestRule.pollInstrumentationThread(
                    () -> {
                        String val =
                                mActivityTestRule.executeJavaScriptAndWaitForResult(
                                        mAwContents, mContentsClient, "window.webauthnResult");
                        return !"\"PENDING\"".equals(val) && !"null".equals(val);
                    });

            String result =
                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                            mAwContents, mContentsClient, "window.webauthnResult");

            Assert.assertFalse(
                    "FIDO2 API should not be invoked for pages with SSL errors",
                    testHelper.mGetAssertionCalled);
            Assert.assertTrue(
                    "WebAuthn should be blocked due to SSL error, but got: " + result,
                    result.contains("NotAllowedError") && result.contains("certificate errors"));
        } finally {
            testServer.stopAndDestroyServer();
        }
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
