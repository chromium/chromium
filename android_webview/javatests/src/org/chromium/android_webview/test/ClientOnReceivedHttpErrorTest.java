// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.util.Pair;

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
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for the ContentViewClient.onReceivedHttpError() method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClientOnReceivedHttpErrorTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private VerifyOnReceivedHttpErrorCallClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    public ClientOnReceivedHttpErrorTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

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
                        "onReceivedHttpError not called before onPageFinished for " + url,
                        true,
                        mIsOnReceivedHttpErrorCalled);
            }
            super.onPageFinished(url);
        }

        @Override
        public void onReceivedHttpError(
                AwWebResourceRequest request, WebResourceResponseInfo response) {
            if (!mBypass) {
                Assert.assertEquals(
                        "onReceivedHttpError called twice for " + request.url,
                        false,
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
        Assert.assertTrue(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        WebResourceResponseInfo response = onReceivedHttpErrorHelper.getResponse();
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
        final String pageWithLinkUrl =
                mWebServer.setResponse(
                        "/page_with_link.html",
                        CommonResources.makeHtmlPageWithSimpleLinkTo(badUrl),
                        null);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithLinkUrl);
        mActivityTestRule.waitForPixelColorAtCenterOfView(
                mAwContents, mTestContainerView, CommonResources.LINK_COLOR);

        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        int onReceivedHttpErrorCallCount = onReceivedHttpErrorHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        onReceivedHttpErrorHelper.waitForCallback(
                onReceivedHttpErrorCallCount,
                /* numberOfCallsToWaitFor= */ 1,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(badUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertTrue(request.isOutermostMainFrame);
        Assert.assertTrue(request.hasUserGesture);
        WebResourceResponseInfo response = onReceivedHttpErrorHelper.getResponse();
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
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testForSubresource() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html; charset=utf-8"));
        final String imageUrl = mWebServer.setResponseWithNotFoundStatus("/404.png", headers);
        final String pageHtml =
                CommonResources.makeHtmlPageFrom(
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
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        WebResourceResponseInfo response = onReceivedHttpErrorHelper.getResponse();
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
        onReceivedHttpErrorHelper.waitForCallback(
                onReceivedHttpErrorCallCount,
                /* numberOfCallsToWaitFor= */ 1,
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
        Assert.assertTrue(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        WebResourceResponseInfo response = onReceivedHttpErrorHelper.getResponse();
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
    public void testOnPageStartedAndFinishedEmpty() throws Throwable {
        useDefaultTestAwContentsClient();
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        final String badUrl = mWebServer.getResponseUrl("/404.html");
        final String goodUrl =
                mWebServer.setResponse("/good.html", CommonResources.ABOUT_HTML, null);
        final int initialOnHttpErrorCount = onReceivedHttpErrorHelper.getCallCount();
        final int initialOnPageStartedCount = onPageStartedHelper.getCallCount();
        final int initialOnPageFinishedCount = onPageFinishedHelper.getCallCount();

        // Navigate to a URL that doesn't exist.
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, badUrl);
        Assert.assertEquals(
                "onReceivedHttpErrorHelper should be called once",
                initialOnHttpErrorCount + 1,
                onReceivedHttpErrorHelper.getCallCount());
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        Assert.assertNotNull("onReceivedHttpError should have a non-null request", request);
        Assert.assertEquals(badUrl, request.url);
        Assert.assertEquals(
                "onPageStartedHelper should be called once",
                initialOnPageStartedCount + 1,
                onPageStartedHelper.getCallCount());
        Assert.assertEquals(badUrl, onPageStartedHelper.getUrl());
        Assert.assertEquals(
                "onPageFinishedHelper should be called once",
                initialOnPageFinishedCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals(badUrl, onPageFinishedHelper.getUrl());

        // Rather than wait a fixed time to see that additional callbacks for badUrl aren't
        // called, we load another valid page since callbacks arrive sequentially.
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, goodUrl);
        Assert.assertEquals(initialOnHttpErrorCount + 1, onReceivedHttpErrorHelper.getCallCount());
        Assert.assertEquals(initialOnPageStartedCount + 2, onPageStartedHelper.getCallCount());
        Assert.assertEquals(goodUrl, onPageStartedHelper.getUrl());
        Assert.assertEquals(initialOnPageFinishedCount + 2, onPageFinishedHelper.getCallCount());
        Assert.assertEquals(goodUrl, onPageFinishedHelper.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageStartedAndFinishedNonEmpty() throws Throwable {
        useDefaultTestAwContentsClient();
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        TestAwContentsClient.OnReceivedHttpErrorHelper onReceivedHttpErrorHelper =
                mContentsClient.getOnReceivedHttpErrorHelper();
        final String badUrl = mWebServer.setResponseWithNotFoundStatus("/404.html");
        final String goodUrl =
                mWebServer.setResponse("/good.html", CommonResources.ABOUT_HTML, null);
        final int initialOnHttpErrorCount = onReceivedHttpErrorHelper.getCallCount();
        final int initialOnPageStartedCount = onPageStartedHelper.getCallCount();
        final int initialOnPageFinishedCount = onPageFinishedHelper.getCallCount();

        // Navigate to a URL that 404s but has a non-empty body (because
        // setResponseWithNotFoundStatus will add some content to 404 responses).
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, badUrl);
        Assert.assertEquals(
                "onReceivedHttpErrorHelper should be called once",
                initialOnHttpErrorCount + 1,
                onReceivedHttpErrorHelper.getCallCount());
        AwWebResourceRequest request = onReceivedHttpErrorHelper.getRequest();
        Assert.assertNotNull("onReceivedHttpError should have a non-null request", request);
        Assert.assertEquals(badUrl, request.url);
        Assert.assertEquals(
                "onPageStartedHelper should be called once",
                initialOnPageStartedCount + 1,
                onPageStartedHelper.getCallCount());
        Assert.assertEquals(badUrl, onPageStartedHelper.getUrl());
        Assert.assertEquals(
                "onPageFinishedHelper should be called once",
                initialOnPageFinishedCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals(badUrl, onPageFinishedHelper.getUrl());

        // Rather than wait a fixed time to see that additional callbacks for badUrl aren't
        // called, we load another valid page since callbacks arrive sequentially.
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, goodUrl);
        Assert.assertEquals(initialOnHttpErrorCount + 1, onReceivedHttpErrorHelper.getCallCount());
        Assert.assertEquals(initialOnPageStartedCount + 2, onPageStartedHelper.getCallCount());
        Assert.assertEquals(goodUrl, onPageStartedHelper.getUrl());
        Assert.assertEquals(initialOnPageFinishedCount + 2, onPageFinishedHelper.getCallCount());
        Assert.assertEquals(goodUrl, onPageFinishedHelper.getUrl());
    }
}
