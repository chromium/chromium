// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertNotEquals;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;
import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.webkit.WebSettings;

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
import org.chromium.android_webview.AwContentsClient.AwWebResourceError;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.WebviewErrorCode;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the ContentViewClient.onReceivedError() method. Historically, this test suite focused
 * on the new features added in the 2nd iteration of the callback added in M. Now chromium only
 * supports one version of the callback, so the distinction between this and
 * ClientOnReceivedErrorTest.java is no longer as significant.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClientOnReceivedError2Test extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private VerifyOnReceivedErrorCallClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    // URLs which do not exist on the public internet (because they use the ".test" TLD).
    private static final String BAD_HTML_URL = "http://fake.domain.test/a.html";
    private static final String BAD_IMAGE_URL = "http://fake.domain.test/a.png";

    public ClientOnReceivedError2Test(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new VerifyOnReceivedErrorCallClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    private void startWebServer() throws Exception {
        mWebServer = TestWebServer.start();
    }

    private void useDefaultTestAwContentsClient() {
        mContentsClient.enableBypass();
    }

    private static class VerifyOnReceivedErrorCallClient extends TestAwContentsClient {
        private boolean mBypass;
        private boolean mIsOnPageFinishedCalled;
        private boolean mIsOnReceivedErrorCalled;

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
                        "onReceivedError not called before onPageFinished for " + url,
                        true,
                        mIsOnReceivedErrorCalled);
            }
            super.onPageFinished(url);
        }

        @Override
        public void onReceivedError(AwWebResourceRequest request, AwWebResourceError error) {
            if (!mBypass) {
                Assert.assertEquals(
                        "onReceivedError called twice for " + request.url,
                        false,
                        mIsOnReceivedErrorCalled);
                mIsOnReceivedErrorCalled = true;
            }
            super.onReceivedError(request, error);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMainFrame() throws Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), BAD_HTML_URL);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(BAD_HTML_URL, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        // request headers may or may not be empty, this is an implementation detail,
        // in the network service code path they may e.g. contain user agent, crbug.com/893573.
        Assert.assertTrue(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertNotEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUserGesture() throws Throwable {
        useDefaultTestAwContentsClient();
        final String pageHtml = CommonResources.makeHtmlPageWithSimpleLinkTo(BAD_HTML_URL);
        mActivityTestRule.loadDataAsync(mAwContents, pageHtml, "text/html", false);
        mActivityTestRule.waitForPixelColorAtCenterOfView(
                mAwContents, mTestContainerView, CommonResources.LINK_COLOR);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        onReceivedErrorHelper.waitForCallback(
                onReceivedErrorCount,
                /* numberOfCallsToWaitFor= */ 1,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(BAD_HTML_URL, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        // request headers may or may not be empty, this is an implementation detail,
        // in the network service code path they may e.g. contain user agent, crbug.com/893573.
        Assert.assertTrue(request.isOutermostMainFrame);
        Assert.assertTrue(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertNotEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testIframeSubresource() throws Throwable {
        final String pageHtml =
                CommonResources.makeHtmlPageFrom("", "<iframe src='" + BAD_HTML_URL + "' />");
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                pageHtml,
                "text/html",
                false);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(BAD_HTML_URL, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        // request headers may or may not be empty, this is an implementation detail,
        // in the network service code path they may e.g. contain user agent, crbug.com/893573.
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertNotEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUserGestureForIframeSubresource() throws Throwable {
        useDefaultTestAwContentsClient();
        startWebServer();
        final String iframeHtml = CommonResources.makeHtmlPageWithSimpleLinkTo(BAD_HTML_URL);
        final String iframeUrl = mWebServer.setResponse("/iframe.html", iframeHtml, null);
        final String pageHtml =
                CommonResources.makeHtmlPageFrom(
                        "", "<iframe style='width:100%;height:100%;' src='" + iframeUrl + "' />");
        mActivityTestRule.loadDataAsync(mAwContents, pageHtml, "text/html", false);
        mActivityTestRule.waitForPixelColorAtCenterOfView(
                mAwContents, mTestContainerView, CommonResources.LINK_COLOR);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        onReceivedErrorHelper.waitForCallback(
                onReceivedErrorCount,
                /* numberOfCallsToWaitFor= */ 1,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(BAD_HTML_URL, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        // request headers may or may not be empty, this is an implementation detail,
        // in the network service code path they may e.g. contain user agent, crbug.com/893573.
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertTrue(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertNotEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testImageSubresource() throws Throwable {
        final String pageHtml =
                CommonResources.makeHtmlPageFrom("", "<img src='" + BAD_IMAGE_URL + "' />");
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                pageHtml,
                "text/html",
                false);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(BAD_IMAGE_URL, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        // request headers may or may not be empty, this is an implementation detail,
        // in the network service code path they may e.g. contain user agent, crbug.com/893573.
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        // The particular error code that is returned depends on the configuration of the device
        // (such as existence of a proxy) so we don't test for it.
        assertNotEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnInvalidScheme() throws Throwable {
        final String iframeUrl = "foo://some/resource";
        final String pageHtml =
                CommonResources.makeHtmlPageFrom("", "<iframe src='" + iframeUrl + "' />");
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                pageHtml,
                "text/html",
                false);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(iframeUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        Assert.assertEquals(WebviewErrorCode.ERROR_UNSUPPORTED_SCHEME, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnNonExistentAssetUrl() throws Throwable {
        final String baseUrl = "file:///android_asset/";
        final String iframeUrl = baseUrl + "does_not_exist.html";
        final String pageHtml =
                CommonResources.makeHtmlPageFrom("", "<iframe src='" + iframeUrl + "' />");
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                pageHtml,
                "text/html",
                false,
                baseUrl,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(iframeUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        Assert.assertEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnNonExistentResourceUrl() throws Throwable {
        final String baseUrl = "file:///android_res/raw/";
        final String iframeUrl = baseUrl + "does_not_exist.html";
        final String pageHtml =
                CommonResources.makeHtmlPageFrom("", "<iframe src='" + iframeUrl + "' />");
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                pageHtml,
                "text/html",
                false,
                baseUrl,
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(iframeUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        Assert.assertEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnCacheMiss() throws Throwable {
        final String iframeUrl = "http://example.com/index.html";
        final String pageHtml =
                CommonResources.makeHtmlPageFrom("", "<iframe src='" + iframeUrl + "' />");
        mActivityTestRule
                .getAwSettingsOnUiThread(mAwContents)
                .setCacheMode(WebSettings.LOAD_CACHE_ONLY);
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                pageHtml,
                "text/html",
                false);

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        AwWebResourceRequest request = onReceivedErrorHelper.getRequest();
        Assert.assertNotNull(request);
        Assert.assertEquals(iframeUrl, request.url);
        Assert.assertEquals("GET", request.method);
        Assert.assertNotNull(request.requestHeaders);
        Assert.assertFalse(request.requestHeaders.isEmpty());
        Assert.assertFalse(request.isOutermostMainFrame);
        Assert.assertFalse(request.hasUserGesture);
        AwWebResourceError error = onReceivedErrorHelper.getError();
        Assert.assertEquals(WebviewErrorCode.ERROR_UNKNOWN, error.errorCode);
        Assert.assertNotNull(error.description);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnStopLoading() throws Throwable {
        useDefaultTestAwContentsClient();
        final CountDownLatch latch = new CountDownLatch(1);
        startWebServer();
        final String url =
                mWebServer.setResponseWithRunnableAction(
                        "/about.html",
                        CommonResources.ABOUT_HTML,
                        null,
                        () -> {
                            try {
                                latch.await(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                            } catch (InterruptedException e) {
                                Assert.fail("Caught InterruptedException " + e);
                            }
                        });
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);
        mActivityTestRule.stopLoading(mAwContents);
        onPageFinishedHelper.waitForCallback(
                onPageFinishedCallCount,
                /* numberOfCallsToWaitFor= */ 1,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        latch.countDown(); // Release the server.

        // Instead of waiting for OnReceivedError not to be called, we schedule
        // a load that will result in a error, and check that we have only got one callback,
        // originating from the last attempt.
        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, BAD_HTML_URL);
        onReceivedErrorHelper.waitForCallback(
                onReceivedErrorCount,
                /* numberOfCallsToWaitFor= */ 1,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        Assert.assertEquals(onReceivedErrorCount + 1, onReceivedErrorHelper.getCallCount());
        Assert.assertEquals(BAD_HTML_URL, onReceivedErrorHelper.getRequest().url);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUnsafeRedirect_FileUrl() throws Throwable {
        startWebServer();
        final String redirectUrl = mWebServer.setRedirect("/302.html", "file:///foo");

        TestAwContentsClient.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        final int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), redirectUrl);

        onReceivedErrorHelper.waitForCallback(
                onReceivedErrorCount,
                /* numberOfCallsToWaitFor= */ 1,
                WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        Assert.assertEquals(onReceivedErrorCount + 1, onReceivedErrorHelper.getCallCount());
        AwWebResourceError error = onReceivedErrorHelper.getError();
        Assert.assertEquals("net::ERR_UNSAFE_REDIRECT", error.description);
    }
}
