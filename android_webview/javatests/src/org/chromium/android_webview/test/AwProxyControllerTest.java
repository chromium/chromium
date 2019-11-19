// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwProxyController;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;

/**
 * AwProxyController tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwProxyControllerTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

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

    @Before
    public void setup() throws Exception {
        mAwProxyController = new AwProxyController();
        mContentServer = TestWebServer.start();
        mProxyServer = TestWebServer.startAdditional();
        mContentUrl = mContentServer.setResponse(
                "/", "<html><head><title>" + CONTENT + "</title></head>Page 1</html>", null);
        mProxyUrl = mProxyServer
                            .setResponse("/",
                                    "<html><head><title>" + PROXY + "</title></head>Page 1</html>",
                                    null)
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

        int proxyServerRequestCount = mProxyServer.getRequestCount("/");

        // Set proxy override and load content url
        // Localhost should use proxy with loopback rule
        setProxyOverrideSync(
                new String[][] {{MATCH_ALL_SCHEMES, mProxyUrl}}, new String[] {LOOPBACK});
        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                contentsClient.getOnReceivedTitleHelper();
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), mContentUrl);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);

        proxyServerRequestCount++;
        Assert.assertEquals(proxyServerRequestCount, mProxyServer.getRequestCount("/"));
        Assert.assertEquals(PROXY, onReceivedTitleHelper.getTitle());

        // Clear proxy override and load content url
        clearProxyOverrideSync();
        onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), mContentUrl);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);

        Assert.assertEquals(proxyServerRequestCount, mProxyServer.getRequestCount("/"));
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

        int proxyServerRequestCount = mProxyServer.getRequestCount("/");

        // Set proxy override and load a local url
        // Localhost should not use proxy settings
        setProxyOverrideSync(new String[][] {{MATCH_ALL_SCHEMES, mProxyUrl}}, new String[] {});
        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                contentsClient.getOnReceivedTitleHelper();
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), mContentUrl);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);

        Assert.assertEquals(proxyServerRequestCount, mProxyServer.getRequestCount("/"));
        Assert.assertEquals(CONTENT, onReceivedTitleHelper.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallbacks() throws Throwable {
        // Test setProxyOverride's callback
        setProxyOverrideSync(null, null);
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
        String[][] proxyRules = {{MATCH_ALL_SCHEMES, DIRECT},
                {MATCH_ALL_SCHEMES, "www.example.com"},
                {MATCH_ALL_SCHEMES, "http://www.example.com"},
                {MATCH_ALL_SCHEMES, "https://www.example.com"},
                {MATCH_ALL_SCHEMES, "www.example.com:123"},
                {MATCH_ALL_SCHEMES, "http://www.example.com:123"}, {MATCH_ALL_SCHEMES, "10.0.0.1"},
                {MATCH_ALL_SCHEMES, "10.0.0.1:123"}, {MATCH_ALL_SCHEMES, "http://10.0.0.1"},
                {MATCH_ALL_SCHEMES, "https://10.0.0.1"}, {MATCH_ALL_SCHEMES, "http://10.0.0.1:123"},
                {MATCH_ALL_SCHEMES, "[FE80:CD00:0000:0CDE:1257:0000:211E:729C]"},
                {MATCH_ALL_SCHEMES, "[FE80:CD00:0:CDE:1257:0:211E:729C]"}};
        String[] bypassRules = {
                "www.rule.com", "*.rule.com", "*rule.com", "www.*.com", "www.rule*"};
        setProxyOverrideSync(proxyRules, bypassRules);
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
                setProxyOverrideSync(new String[][] {{MATCH_ALL_SCHEMES, proxyUrl}}, null);
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
                setProxyOverrideSync(null, new String[] {bypassRule});
                Assert.fail("No exception for invalid bypass rule: " + bypassRule);
            } catch (IllegalArgumentException e) {
                // Expected
            }
        }
    }

    private void setProxyOverrideSync(String[][] proxyRules, String[] bypassRules)
            throws Exception {
        CallbackHelper ch = new CallbackHelper();
        int callCount = ch.getCallCount();
        runOnUiThreadBlocking(() -> {
            mAwProxyController.setProxyOverride(proxyRules, bypassRules, new Runnable() {
                @Override
                public void run() {
                    ch.notifyCalled();
                }
            }, new SynchronousExecutor());
        });
        ch.waitForCallback(callCount);
    }

    private void clearProxyOverrideSync() throws Exception {
        CallbackHelper ch = new CallbackHelper();
        int callCount = ch.getCallCount();
        runOnUiThreadBlocking(() -> {
            mAwProxyController.clearProxyOverride(new Runnable() {
                @Override
                public void run() {
                    ch.notifyCalled();
                }
            }, new SynchronousExecutor());
        });
        ch.waitForCallback(callCount);
    }

    private void runOnUiThreadBlocking(Runnable r) throws Exception {
        try {
            TestThreadUtils.runOnUiThreadBlocking(r);
        } catch (RuntimeException e) {
            Throwable cause = e.getCause();
            if (cause instanceof ExecutionException) cause = cause.getCause();
            if (cause instanceof IllegalArgumentException) throw (IllegalArgumentException) cause;
            throw e;
        }
    }

    static class SynchronousExecutor implements Executor {
        @Override
        public void execute(Runnable r) {
            r.run();
        }
    }
}
