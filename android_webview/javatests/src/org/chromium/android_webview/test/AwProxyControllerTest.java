// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.MediumTest;
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
import org.chromium.android_webview.AwProxyController;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Executor;

/** AwProxyController tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwProxyControllerTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String MATCH_ALL_SCHEMES = "*";
    private static final String DIRECT = "direct://";
    private static final String LOOPBACK = "<-loopback>";
    private static final String CONTENT = "CONTENT";
    private static final String PROXY = "PROXY";

    private AwProxyController mAwProxyController;
    private TestWebServer mContentServer;
    private TestWebServer mProxyServer;
    private String mContentUrl;
    private String mProxyUrl;

    public AwProxyControllerTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setup() throws Exception {
        mAwProxyController = new AwProxyController();
        mContentServer = TestWebServer.start();
        mProxyServer = TestWebServer.startAdditional();
        mContentUrl =
                mContentServer.setResponse(
                        "/",
                        "<html><head><title>" + CONTENT + "</title></head>Page 1</html>",
                        null);
        mProxyUrl =
                mProxyServer
                        .setResponse(
                                mContentUrl,
                                "<html><head><title>" + PROXY + "</title></head>Page 1</html>",
                                null)
                        .replace(mContentUrl, "")
                        .replace("http://", "")
                        .replace("/", "");
    }

    @After
    public void tearDown() throws Exception {
        clearProxyOverrideSync();
        mContentServer.shutdown();
        mProxyServer.shutdown();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testProxyOverride() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        int proxyServerRequestCount = mProxyServer.getRequestCount(mContentUrl);

        // Set proxy override and load content url Localhost should use proxy with loopback rule
        setProxyOverrideSync(
                new String[][] {{MATCH_ALL_SCHEMES, mProxyUrl}}, new String[] {LOOPBACK}, false);
        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                contentsClient.getOnReceivedTitleHelper();
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();

        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), mContentUrl);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);

        proxyServerRequestCount++;
        Assert.assertEquals(proxyServerRequestCount, mProxyServer.getRequestCount(mContentUrl));
        Assert.assertEquals(PROXY, onReceivedTitleHelper.getTitle());

        // Clear proxy override and load content url
        clearProxyOverrideSync();
        onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), mContentUrl);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);

        Assert.assertEquals(proxyServerRequestCount, mProxyServer.getRequestCount(mContentUrl));
        Assert.assertEquals(CONTENT, onReceivedTitleHelper.getTitle());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testProxyOverrideLocalhost() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        int proxyServerRequestCount = mProxyServer.getRequestCount(mContentUrl);

        // Set proxy override and load a local url Localhost should not use proxy settings
        setProxyOverrideSync(
                new String[][] {{MATCH_ALL_SCHEMES, mProxyUrl}}, new String[] {}, false);
        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                contentsClient.getOnReceivedTitleHelper();
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), mContentUrl);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);

        Assert.assertEquals(proxyServerRequestCount, mProxyServer.getRequestCount(mContentUrl));
        Assert.assertEquals(CONTENT, onReceivedTitleHelper.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReverseBypassRules() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        String url = "http://www.example.com/";
        String bypassUrl = "www.example.com";

        mProxyServer.setResponse(
                url, "<html><head><title>" + PROXY + "</title></head>Page 1</html>", null);
        int proxyServerRequestCount = mProxyServer.getRequestCount(url);

        // Set proxy override with reverse bypass, that is, only use proxy settings
        // with URLs in the bypass list
        setProxyOverrideSync(
                new String[][] {{MATCH_ALL_SCHEMES, mProxyUrl}}, new String[] {bypassUrl}, true);
        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                contentsClient.getOnReceivedTitleHelper();
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        mActivityTestRule.loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(), url);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);

        proxyServerRequestCount++;
        Assert.assertEquals(proxyServerRequestCount, mProxyServer.getRequestCount(url));
        Assert.assertEquals(PROXY, onReceivedTitleHelper.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallbacks() throws Throwable {
        // Test setProxyOverride's callback
        setProxyOverrideSync(null, null, false);
        // Test clearProxyOverride's callback with a proxy override setting
        clearProxyOverrideSync();
        // Test clearProxyOverride's callback without a proxy override setting
        clearProxyOverrideSync();
        // If we got to this point it means all callbacks were called as expected
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testValidInput() throws Throwable {
        String[][] proxyRules = {
            {MATCH_ALL_SCHEMES, DIRECT},
            {MATCH_ALL_SCHEMES, "www.example.com"},
            {MATCH_ALL_SCHEMES, "http://www.example.com"},
            {MATCH_ALL_SCHEMES, "https://www.example.com"},
            {MATCH_ALL_SCHEMES, "www.example.com:123"},
            {MATCH_ALL_SCHEMES, "http://www.example.com:123"},
            {MATCH_ALL_SCHEMES, "10.0.0.1"},
            {MATCH_ALL_SCHEMES, "10.0.0.1:123"},
            {MATCH_ALL_SCHEMES, "http://10.0.0.1"},
            {MATCH_ALL_SCHEMES, "https://10.0.0.1"},
            {MATCH_ALL_SCHEMES, "http://10.0.0.1:123"},
            {MATCH_ALL_SCHEMES, "[FE80:CD00:0000:0CDE:1257:0000:211E:729C]"},
            {MATCH_ALL_SCHEMES, "[FE80:CD00:0:CDE:1257:0:211E:729C]"}
        };
        String[] bypassRules = {
            "www.rule.com", "*.rule.com", "*rule.com", "www.*.com", "www.rule*"
        };
        setProxyOverrideSync(proxyRules, bypassRules, false);
        // If we got to this point it means our input was accepted as expected
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInvalidProxyUrls() throws Throwable {
        String[] invalidProxyUrls = {
            null,
            "", // empty
            "   ", // spaces only
            "dddf:", // bad port
            "dddd:d", // bad port
            "http://", // no valid host/port
            "http:/", // ambiguous, will fail due to bad port
            "http:", // ambiguous, will fail due to bad port
            "direct://xyz", // direct shouldn't have host/port
        };

        for (String proxyUrl : invalidProxyUrls) {
            try {
                setProxyOverrideSync(new String[][] {{MATCH_ALL_SCHEMES, proxyUrl}}, null, false);
                Assert.fail("No exception for invalid proxy url: " + proxyUrl);
            } catch (IllegalArgumentException e) {
                // Expected
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInvalidBypassRules() throws Throwable {
        String[] invalidBypassRules = {
            null,
            "", // empty
            "http://", // no valid host/port
            "20:example.com", // bad port
            "example.com:-20" // bad port
        };

        for (String bypassRule : invalidBypassRules) {
            try {
                setProxyOverrideSync(null, new String[] {bypassRule}, false);
                Assert.fail("No exception for invalid bypass rule: " + bypassRule);
            } catch (IllegalArgumentException e) {
                // Expected
            }
        }
    }

    private void setProxyOverrideSync(
            String[][] proxyRules, String[] bypassRules, boolean reverseBypass) throws Exception {
        CallbackHelper ch = new CallbackHelper();
        int callCount = ch.getCallCount();
        runOnUiThreadBlocking(
                () -> {
                    mAwProxyController.setProxyOverride(
                            proxyRules,
                            bypassRules,
                            new Runnable() {
                                @Override
                                public void run() {
                                    ch.notifyCalled();
                                }
                            },
                            new SynchronousExecutor(),
                            reverseBypass);
                });
        ch.waitForCallback(callCount);
    }

    private void clearProxyOverrideSync() throws Exception {
        CallbackHelper ch = new CallbackHelper();
        int callCount = ch.getCallCount();
        runOnUiThreadBlocking(
                () -> {
                    mAwProxyController.clearProxyOverride(
                            new Runnable() {
                                @Override
                                public void run() {
                                    ch.notifyCalled();
                                }
                            },
                            new SynchronousExecutor());
                });
        ch.waitForCallback(callCount);
    }

    private void runOnUiThreadBlocking(Runnable r) throws Exception {
        try {
            ThreadUtils.runOnUiThreadBlocking(r);
        } catch (Exception e) {
            throw (Exception) e.getCause();
        }
    }

    static class SynchronousExecutor implements Executor {
        @Override
        public void execute(Runnable r) {
            r.run();
        }
    }
}
