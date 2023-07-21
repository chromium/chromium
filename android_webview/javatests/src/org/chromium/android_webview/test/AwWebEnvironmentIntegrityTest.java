// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.webkit.JavascriptInterface;

import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.ShouldInterceptRequestHelper;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.components.environment_integrity.IntegrityServiceBridge;
import org.chromium.components.environment_integrity.IntegrityServiceBridgeDelegate;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for WebEnvironmentIntegrity in WebView.
 *
 * These tests are in addition to
 * {@link org.chromium.chrome.browser.environment_integrity.EnvironmentIntegrityTest}
 * and only supposed to test WebView-specific differences.
 */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AwWebEnvironmentIntegrityTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private TestWebServer mWebServer;
    private static final String ORIGIN_TRIAL_URL = "https://example.com/";
    private static final String ORIGIN_TRIAL_HEADER = "Origin-Trial";
    private static final String ORIGIN_TRIAL_TOKEN =
            "A1GBGCeaLBRlky1ITf9uRak5iluqLWnUdSTKVTO0Ce/I7a35nik6DKqPJNZSPd9KEAIuJKmi2dmL9HWThDWgdA"
            + "cAAABheyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJXZWJFbnZpcm"
            + "9ubWVudEludGVncml0eSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

    private static final long HANDLE = 123456789L;

    private static final byte[] TOKEN = {1, 2, 3, 4};
    private static final String TOKEN_BASE64 = "AQIDBA==";

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();

        mContentsClient = new TestAwContentsClient();
        AwTestContainerView mContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @After
    public void tearDown() throws Exception {
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    public void testWebEnvironmentIntegrityApiNotAvailableByDefault() throws Throwable {
        // Load a web page from localhost to get a secure context
        mWebServer.setResponse("/", "<html>", Collections.emptyList());
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mWebServer.getBaseUrl());
        // Check that the 'getEnvironmentIntegrity' method is available.
        final String script = "'getEnvironmentIntegrity' in navigator ? 'available': 'missing'";
        String result = mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, script);
        // The result is expected to have extra quotes as a JSON-encoded string.
        Assert.assertEquals("This test is expected to fail if runtime_enabled_features.json5"
                        + " is updated to mark the feature as 'stable'.",
                "\"missing\"", result);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-features=" + BlinkFeatures.WEB_ENVIRONMENT_INTEGRITY})
    public void testWebEnvironmentIntegrityApiAvailable() throws Throwable {
        // Load a web page from localhost to get a secure context
        mWebServer.setResponse("/", "<html>", Collections.emptyList());
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mWebServer.getBaseUrl());
        // Check that the 'getEnvironmentIntegrity' method is available.
        final String script = "'getEnvironmentIntegrity' in navigator ? 'available': 'missing'";
        String result = mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, script);
        // The result is expected to have extra quotes as a JSON-encoded string.
        Assert.assertEquals("\"available\"", result);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=" + BlinkFeatures.WEB_ENVIRONMENT_INTEGRITY})
    public void testWebEnvironmentIntegrityApiCanBeDisabled() throws Throwable {
        // Load a web page from localhost to get a secure context
        mWebServer.setResponse("/", "<html>", Collections.emptyList());
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mWebServer.getBaseUrl());
        // Check that the 'getEnvironmentIntegrity' method is available.
        final String script = "'getEnvironmentIntegrity' in navigator ? 'available': 'missing'";
        String result = mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, script);
        // The result is expected to have extra quotes as a JSON-encoded string.
        Assert.assertEquals("\"missing\"", result);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA="})
    public void testAppIdentityEnabledByOriginTrial() throws Throwable {
        // Set up a response with the origin trial header.
        // Since origin trial tokens are tied to the origin, we use an request intercept to load
        // the content when making a request to the origin trial URL, instead of relying on the
        // server, which serves from an unknown port.
        var body = new ByteArrayInputStream(
                "<!DOCTYPE html><html><body>Hello, World".getBytes(StandardCharsets.UTF_8));
        var responseInfo = new WebResourceResponseInfo("text/html", "utf-8", body, 200, "OK",
                Map.of(ORIGIN_TRIAL_HEADER, ORIGIN_TRIAL_TOKEN));

        final ShouldInterceptRequestHelper requestInterceptHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        requestInterceptHelper.setReturnValueForUrl(ORIGIN_TRIAL_URL, responseInfo);

        final TestIntegrityServiceBridgeDelegateImpl delegateForTesting =
                new TestIntegrityServiceBridgeDelegateImpl();
        mActivityTestRule.runOnUiThread(
                () -> IntegrityServiceBridge.setDelegateForTesting(delegateForTesting));

        final ExecutionCallbackListener listener = new ExecutionCallbackListener();
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(mAwContents, listener, "testListener");

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), ORIGIN_TRIAL_URL);

        final String script = "(() => {"
                + "if ('getEnvironmentIntegrity' in navigator) {"
                + "  navigator.getEnvironmentIntegrity('contentBinding')"
                + "    .then(s => testListener.result(s.encode()))"
                + "    .catch(e => testListener.result('error: ' + e));"
                + "  return 'available';"
                + "} else {return 'unavailable';}"
                + "})();";
        String scriptResult = mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, script);
        // The result is expected to have extra quotes as a JSON-encoded string.
        Assert.assertEquals("\"available\"", scriptResult);

        // Wait until the result callback has been triggered, to inspect the state of the delegate
        // The actual result should just be an error we don't care about.
        String result = listener.waitForResult();
        Assert.assertEquals(TOKEN_BASE64, result);
    }

    static class ExecutionCallbackListener {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private String mResult;

        @JavascriptInterface
        public void result(String s) {
            mResult = s;
            mCallbackHelper.notifyCalled();
        }

        String waitForResult() throws TimeoutException {
            mCallbackHelper.waitForNext(5, TimeUnit.SECONDS);
            return mResult;
        }
    }

    private static class TestIntegrityServiceBridgeDelegateImpl
            implements IntegrityServiceBridgeDelegate {
        @Override
        public ListenableFuture<Long> createEnvironmentIntegrityHandle(
                boolean bindAppIdentity, int timeoutMilliseconds) {
            return Futures.immediateFuture(HANDLE);
        }

        @Override
        public ListenableFuture<byte[]> getEnvironmentIntegrityToken(
                long handle, byte[] requestHash, int timeoutMilliseconds) {
            return Futures.immediateFuture(TOKEN);
        }

        @Override
        public boolean canUseGms() {
            return true;
        }
    }
}
