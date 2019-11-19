// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedHttpError() method.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class ClientOnReceivedHttpErrorTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private VerifyOnReceivedHttpErrorCallClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new VerifyOnReceivedHttpErrorCallClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mWebServer = TestWebServer.start();
    }

    private void useDefaultTestAwContentsClient() {
        mContentsClient.enableBypass();
    }

    private static class VerifyOnReceivedHttpErrorCallClient extends TestAwContentsClient {
        private boolean mBypass;
        private boolean mIsOnPageFinishedCalled;
        private boolean mIsOnReceivedHttpErrorCalled;

        void enableBypass() {
            mBypass = true;
        }

        @Override
        public void onPageFinished(String url) {
            if (!mBypass) {
                Assert.assertEquals(
                        "onPageFinished called twice for " + url, false, mIsOnPageFinishedCalled);
                mIsOnPageFinishedCalled = true;
                Assert.assertEquals(
                        "onReceivedHttpError not called before onPageFinished for " + url, true,
                        mIsOnReceivedHttpErrorCalled);
            }
            super.onPageFinished(url);
        }

        @Override
        public void onReceivedHttpError(
                AwWebResourceRequest request, AwWebResourceResponse response) {
            if (!mBypass) {
                Assert.assertEquals("onReceivedHttpError called twice for " + request.url, false,
                        mIsOnReceivedHttpErrorCalled);
                mIsOnReceivedHttpErrorCalled = true;
            }
            super.onReceivedHttpError(request, response);
        }
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testForMainResource() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        headers.add(Pair.create("Coalesce", ""));
        headers.add(Pair.create("Coalesce", "a"));
        headers.add(Pair.create("Coalesce", ""));
        headers.add(Pair.create("Coalesce", "a"));
        final String url = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(url, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertTrue(request.isMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        Assert.assertEquals(404, response.getStatusCode());
        Assert.assertEquals("Not Found", response.getReasonPhrase());
        Assert.assertEquals("text/html", response.getMimeType());
        Assert.assertEquals("utf-8", response.getCharset());
        Assert.assertNotNull(response.getResponseHeaders());
        Assert.assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        Assert.assertEquals(
                "text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
        Assert.assertTrue(response.getResponseHeaders().containsKey("Coalesce"));
        Assert.assertEquals("a, a", response.getResponseHeaders().get("Coalesce"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testForUserGesture() throws Throwable {
        useDefaultTestAwContentsClient();
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String badUrl = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        final String pageWithLinkUrl = mWebServer.setResponse("/page_with_link.html",
                CommonResources.makeHtmlPageWithSimpleLinkTo(badUrl), null);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithLinkUrl);
        mActivityTestRule.waitForPixelColorAtCenterOfView(
                mAwContents, mTestContainerView, CommonResources.LINK_COLOR);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        onReceivedHttpErrorHelper.waitForCallback(onReceivedHttpErrorCallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(badUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertTrue(request.isMainFrame);
        Assert.assertTrue(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        Assert.assertEquals(404, response.getStatusCode());
        Assert.assertEquals("Not Found", response.getReasonPhrase());
        Assert.assertEquals("text/html", response.getMimeType());
        Assert.assertEquals("utf-8", response.getCharset());
        Assert.assertNotNull(response.getResponseHeaders());
        Assert.assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        Assert.assertEquals(
                "text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testForSubresource() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String imageUrl = mWebServer.setResponseWithNotFoundStatus("/404.png", headers);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<img src='" + imageUrl + "' class='img.big' />");
        final String pageUrl = mWebServer.setResponse("/page.html", pageHtml, null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(imageUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertFalse(request.isMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        Assert.assertEquals(404, response.getStatusCode());
        Assert.assertEquals("Not Found", response.getReasonPhrase());
        Assert.assertEquals("text/html", response.getMimeType());
        Assert.assertEquals("utf-8", response.getCharset());
        Assert.assertNotNull(response.getResponseHeaders());
        Assert.assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        Assert.assertEquals(
                "text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledIfNoHttpError() throws Throwable {
        useDefaultTestAwContentsClient();
        final String goodUrl = mWebServer.setResponse("/1.html", CommonResources.ABOUT_HTML, null);
        final String badUrl = mWebServer.setResponseWithNotFoundStatus("/404.html");
        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        final int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), goodUrl);

        // Instead of waiting for OnReceivedHttpError not to be called, we schedule
        // a load that will result in a error, and check that we have only got one callback,
        // originating from the last attempt.
        mActivityTestRule.loadUrlAsync(mAwContents, badUrl);
        onReceivedHttpErrorHelper.waitForCallback(onReceivedHttpErrorCallCount,
                1 /* numberOfCallsToWaitFor */,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                onReceivedHttpErrorCallCount + 1, onReceivedHttpErrorHelper.getCallCount());
        Assert.assertEquals(badUrl, onReceivedHttpErrorHelper.getRequest().url);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAfterRedirect() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String secondUrl = mWebServer.setResponseWithNotFoundStatus("/404.html", headers);
        final String firstUrl = mWebServer.setRedirect("/302.html", secondUrl);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), firstUrl);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(secondUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertTrue(request.isMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceResponse response = onReceivedHttpErrorHelper.getResponse();
        Assert.assertEquals(404, response.getStatusCode());
        Assert.assertEquals("Not Found", response.getReasonPhrase());
        Assert.assertEquals("text/html", response.getMimeType());
        Assert.assertEquals("utf-8", response.getCharset());
        Assert.assertNotNull(response.getResponseHeaders());
        Assert.assertTrue(response.getResponseHeaders().containsKey("Content-Type"));
        Assert.assertEquals(
                "text/html; charset=utf-8", response.getResponseHeaders().get("Content-Type"));
    }
}
