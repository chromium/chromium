// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import android.util.Pair;
import android.webkit.JavascriptInterface;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Tests for the WebViewClient.shouldInterceptRequest() method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwContentsClientShouldInterceptRequestTest extends AwParameterizedTest {

    @Rule public AwActivityTestRule mActivityTestRule;

    private static final int TEAPOT_STATUS_CODE = 418;
    private static final String TEAPOT_RESPONSE_PHRASE = "I'm a teapot";

    // These constants should match the values in
    // /components/embedder_support/android/util/web_resource_response.cc
    private static final String MULTI_COOKIE_VALUE_SEPARATOR = "\0";
    private static final String MULTI_COOKIE_HEADER_NAME = "\0Set-Cookie-Multivalue\0";

    private String addPageToTestServer(TestWebServer webServer, String httpPath, String html) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return webServer.setResponse(httpPath, html, headers);
    }

    private String addJavaScriptToTestServer(
            TestWebServer webServer, String httpPath, String script) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/javascript"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return webServer.setResponse(httpPath, script, headers);
    }

    private String addAboutPageToTestServer(TestWebServer webServer) {
        return addPageToTestServer(
                webServer, "/" + CommonResources.ABOUT_FILENAME, CommonResources.ABOUT_HTML);
    }

    private TestWebServer mWebServer;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient.ShouldInterceptRequestHelper mShouldInterceptRequestHelper;

    public AwContentsClientShouldInterceptRequestTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mShouldInterceptRequestHelper = mContentsClient.getShouldInterceptRequestHelper();
        mAwContents.getSettings().setAllowFileAccess(true);

        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Throwable {
        CookieUtils.clearCookies(
                InstrumentationRegistry.getInstrumentation(), new AwCookieManager());
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectUrlParam() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted", false);
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(1, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(aboutPageUrl, mShouldInterceptRequestHelper.getUrls().get(0));

        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
        Assert.assertEquals(
                CommonResources.ABOUT_TITLE, mActivityTestRule.getTitleOnUiThread(mAwContents));
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectIsMainFrameParam() throws Throwable {
        final String subframeUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithIframeUrl =
                addPageToTestServer(
                        mWebServer,
                        "/page_with_iframe.html",
                        CommonResources.makeHtmlPageFrom(
                                "", "<iframe src=\"" + subframeUrl + "\"/>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithIframeUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(
                false,
                mShouldInterceptRequestHelper
                        .getRequestsForUrl(subframeUrl)
                        .isOutermostMainFrame());
        Assert.assertEquals(
                true,
                mShouldInterceptRequestHelper
                        .getRequestsForUrl(pageWithIframeUrl)
                        .isOutermostMainFrame());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectMethodParam() throws Throwable {
        final String pageToPostToUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithFormUrl =
                addPageToTestServer(
                        mWebServer,
                        "/page_with_form.html",
                        CommonResources.makeHtmlPageWithSimplePostFormTo(pageToPostToUrl));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithFormUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "GET",
                mShouldInterceptRequestHelper.getRequestsForUrl(pageWithFormUrl).getMethod());

        callCount = mShouldInterceptRequestHelper.getCallCount();
        JSUtils.clickOnLinkUsingJs(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "link");
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "POST",
                mShouldInterceptRequestHelper.getRequestsForUrl(pageToPostToUrl).getMethod());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectHasUserGestureParam() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithLinkUrl =
                addPageToTestServer(
                        mWebServer,
                        "/page_with_link.html",
                        CommonResources.makeHtmlPageWithSimpleLinkTo(aboutPageUrl));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithLinkUrl);
        ThreadUtils.runOnUiThreadBlocking(
                () -> WebContentsUtils.simulateEndOfPaintHolding(mAwContents.getWebContents()));
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(
                false,
                mShouldInterceptRequestHelper.getRequestsForUrl(pageWithLinkUrl).hasUserGesture());

        mActivityTestRule.waitForPixelColorAtCenterOfView(
                mAwContents, mTestContainerView, CommonResources.LINK_COLOR);
        callCount = mShouldInterceptRequestHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(
                true,
                mShouldInterceptRequestHelper.getRequestsForUrl(aboutPageUrl).hasUserGesture());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testCalledWithCorrectRefererHeader() throws Throwable {
        final String refererHeaderName = "Referer";
        final String imageUrl =
                mWebServer.setResponseBase64(
                        "/" + CommonResources.TEST_IMAGE_FILENAME,
                        CommonResources.FAVICON_DATA_BASE64,
                        CommonResources.getImagePngHeaders(true));
        final String pageUrl =
                addPageToTestServer(
                        mWebServer,
                        "/main.html",
                        CommonResources.makeHtmlPageFrom(
                                "", "<img src=\'" + CommonResources.TEST_IMAGE_FILENAME + "\'>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(pageUrl, mShouldInterceptRequestHelper.getUrls().get(0));
        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(pageUrl).getRequestHeaders();
        Assert.assertFalse(headers.containsKey(refererHeaderName));
        Assert.assertEquals(imageUrl, mShouldInterceptRequestHelper.getUrls().get(1));
        headers = mShouldInterceptRequestHelper.getRequestsForUrl(imageUrl).getRequestHeaders();
        Assert.assertTrue(headers.containsKey(refererHeaderName));
        Assert.assertEquals(pageUrl, headers.get(refererHeaderName));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectHeadersParam() throws Throwable {
        final String headerName = "X-Test-Header-Name";
        final String headerValue = "TestHeaderValue";
        final String syncGetUrl =
                addPageToTestServer(mWebServer, "/intercept_me", CommonResources.ABOUT_HTML);
        final String mainPageUrl =
                addPageToTestServer(
                        mWebServer,
                        "/main",
                        CommonResources.makeHtmlPageFrom(
                                "",
                                "<script>"
                                        + "  var xhr = new XMLHttpRequest();"
                                        + "  xhr.open('GET', '"
                                        + syncGetUrl
                                        + "', false);"
                                        + "  xhr.setRequestHeader('"
                                        + headerName
                                        + "', '"
                                        + headerValue
                                        + "'); "
                                        + "  xhr.send(null);"
                                        + "</script>"));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, mainPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);

        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(syncGetUrl).getRequestHeaders();
        Assert.assertTrue(headers.containsKey(headerName));
        Assert.assertEquals(headerValue, headers.get(headerName));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnLoadResourceCalledWithCorrectUrl() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final TestAwContentsClient.OnLoadResourceHelper onLoadResourceHelper =
                mContentsClient.getOnLoadResourceHelper();

        int callCount = onLoadResourceHelper.getCallCount();

        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);

        onLoadResourceHelper.waitForCallback(callCount);
        Assert.assertEquals(aboutPageUrl, onLoadResourceHelper.getLastLoadedResource());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnInvalidData_NullInputStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.enqueueResponseWithStream("text/html", "UTF-8", null);
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnInvalidData_NullMimeEncodingAndZeroLengthStream()
            throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.enqueueResponseWithStream(null, null, EmptyInputStream::new);
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnInvalidData_NullMimeEncodingAndInputStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.enqueueResponseWithStream(null, null, null);
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnInvalidData_ResponseWithAllNullValues() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mShouldInterceptRequestHelper.enqueueResponse(
                /* mimeType= */ null,
                /* encoding= */ null,
                /* data= */ null,
                /* statusCode= */ 0,
                /* reasonPhrase= */ null,
                /* responseHeaders= */ null);
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
    }

    private static class EmptyInputStream extends InputStream {
        @Override
        public int available() {
            return 0;
        }

        @Override
        public int read() throws IOException {
            return -1;
        }

        @Override
        public int read(byte[] b) throws IOException {
            return -1;
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            return -1;
        }

        @Override
        public long skip(long n) throws IOException {
            if (n < 0) {
                throw new IOException("skipping negative number of bytes");
            }
            return 0;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnEmptyStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.enqueueResponseWithStream(
                "text/html", "UTF-8", EmptyInputStream::new);
        int shouldInterceptRequestCallCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);

        mShouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCallCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }

    private static class ThrowingInputStream extends EmptyInputStream {
        @Override
        public int available() {
            return 100;
        }

        @Override
        public int read() throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public int read(byte[] b) throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public long skip(long n) {
            return n;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnThrowingStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.enqueueResponseWithStream(
                "text/html", "UTF-8", ThrowingInputStream::new);
        int shouldInterceptRequestCallCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);

        mShouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCallCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }

    private static class SlowWebResourceResponseInfo extends WebResourceResponseInfo {
        private final CallbackHelper mReadStartedCallbackHelper = new CallbackHelper();
        private final CountDownLatch mLatch = new CountDownLatch(1);

        public SlowWebResourceResponseInfo(String mimeType, String encoding, InputStream data) {
            super(mimeType, encoding, data);
        }

        @Override
        public InputStream getData() {
            mReadStartedCallbackHelper.notifyCalled();
            try {
                mLatch.await();
            } catch (InterruptedException e) {
                // ignore
            }
            return super.getData();
        }

        public void unblockReads() {
            mLatch.countDown();
        }

        public CallbackHelper getReadStartedCallbackHelper() {
            return mReadStartedCallbackHelper;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnSlowStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String aboutPageData = makePageWithTitle("some title");
        final String encoding = "UTF-8";

        final SlowWebResourceResponseInfo slowWebResourceResponseInfo =
                new SlowWebResourceResponseInfo(
                        "text/html",
                        encoding,
                        new ByteArrayInputStream(aboutPageData.getBytes(encoding)));

        mShouldInterceptRequestHelper.enqueueResponse(slowWebResourceResponseInfo);
        int callCount = slowWebResourceResponseInfo.getReadStartedCallbackHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        slowWebResourceResponseInfo.getReadStartedCallbackHelper().waitForCallback(callCount);

        // Now the AwContents is "stuck" waiting for the SlowInputStream to finish reading so we
        // delete it to make sure that the dangling 'read' task doesn't cause a crash. Unfortunately
        // this will not always lead to a crash but it should happen often enough for us to notice.

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mActivityTestRule.getActivity().removeAllViews());
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        mActivityTestRule.pollUiThread(() -> AwContents.getNativeInstanceCount() == 0);

        slowWebResourceResponseInfo.unblockReads();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpStatusCodeAndText() throws Throwable {
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        final String syncGetJs =
                "(function() {"
                        + "  var xhr = new XMLHttpRequest();"
                        + "  xhr.open('GET', '"
                        + syncGetUrl
                        + "', false);"
                        + "  xhr.send(null);"
                        + "  console.info('xhr.status = ' + xhr.status);"
                        + "  console.info('xhr.statusText = ' + xhr.statusText);"
                        + "  return '[' + xhr.status + '][' + xhr.statusText + ']';"
                        + "})();";
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.enqueueResponseWithStream("text/html", "UTF-8", null);
        Assert.assertEquals(
                "\"[404][Not Found]\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, syncGetJs));

        mShouldInterceptRequestHelper.enqueueResponseWithStream(
                "text/html", "UTF-8", EmptyInputStream::new);
        Assert.assertEquals(
                "\"[200][OK]\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, syncGetJs));

        mShouldInterceptRequestHelper.enqueueResponseWithStream(
                "text/html",
                "UTF-8",
                EmptyInputStream::new,
                TEAPOT_STATUS_CODE,
                TEAPOT_RESPONSE_PHRASE,
                Collections.emptyMap());
        Assert.assertEquals(
                "\"[" + TEAPOT_STATUS_CODE + "][" + TEAPOT_RESPONSE_PHRASE + "]\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, syncGetJs));
    }

    private String getHeaderValue(
            AwContents awContents,
            TestAwContentsClient contentsClient,
            String url,
            String headerName)
            throws Exception {
        final String syncGetJs =
                "(function() {"
                        + "  var xhr = new XMLHttpRequest();"
                        + "  xhr.open('GET', '"
                        + url
                        + "', false);"
                        + "  xhr.send(null);"
                        + "  console.info(xhr.getAllResponseHeaders());"
                        + "  return xhr.getResponseHeader('"
                        + headerName
                        + "');"
                        + "})();";
        String header =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, contentsClient, syncGetJs);

        if (header.equals("null")) {
            return null;
        }
        // JSON stringification applied by executeJavaScriptAndWaitForResult adds quotes
        // around returned strings.
        Assert.assertTrue(header.length() > 2);
        Assert.assertEquals('"', header.charAt(0));
        Assert.assertEquals('"', header.charAt(header.length() - 1));
        return header.substring(1, header.length() - 1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpResponseClientViaHeader() throws Throwable {
        final String clientResponseHeaderName = "Client-Via";
        final String clientResponseHeaderValue = "shouldInterceptRequest";
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        // The response header is set regardless of whether the embedder has provided a
        // valid resource stream.
        mShouldInterceptRequestHelper.enqueueResponseWithStream("text/html", "UTF-8", null);
        Assert.assertEquals(
                clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));
        mShouldInterceptRequestHelper.enqueueResponseWithStream(
                "text/html", "UTF-8", EmptyInputStream::new);
        Assert.assertEquals(
                clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpResponseHeader() throws Throwable {
        final String clientResponseHeaderName = "X-Test-Header-Name";
        final String clientResponseHeaderValue = "TestHeaderValue";
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.enqueueResponse(
                "text/html",
                "UTF-8",
                null,
                0,
                null,
                Map.of(clientResponseHeaderName, clientResponseHeaderValue));
        Assert.assertEquals(
                clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullHttpResponseHeaders() throws Throwable {
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.enqueueResponse("text/html", "UTF-8", null, 0, null, null);
        Assert.assertEquals(
                null, getHeaderValue(mAwContents, mContentsClient, syncGetUrl, "Some-Header"));
    }

    private String makePageWithTitle(String title) {
        return CommonResources.makeHtmlPageFrom(
                "<title>" + title + "</title>", "<div> The title is: " + title + " </div>");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanInterceptMainFrame() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted", true);
        final String expectedTitle = "testShouldInterceptRequestCanInterceptMainFrame";
        final String expectedPage = makePageWithTitle(expectedTitle);

        mShouldInterceptRequestHelper.enqueueHtmlResponse(expectedPage, null);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(mAwContents));
        Assert.assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
        histogramExpectation.assertExpected();
    }

    // Regression test for b/345306067.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    // Disable this to make sure the cookie call hits the IO thread.
    @CommandLineFlags.Add({"disable-features=NetworkServiceDedicatedThread"})
    public void testGetCookieInAvailable() throws Throwable {
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        // This will intercept the call to `syncGetUrl` in `getHeaderValue()` below.
        Supplier<InputStream> streamSupplier =
                () ->
                        new ByteArrayInputStream("foo".getBytes(StandardCharsets.UTF_8)) {
                            @Override
                            public synchronized int available() {
                                new AwCookieManager().setCookie(aboutPageUrl, "foo");
                                return super.available();
                            }
                        };
        mShouldInterceptRequestHelper.enqueueResponseWithStream(
                "text/html", "UTF-8", streamSupplier);
        Assert.assertEquals(
                "3", getHeaderValue(mAwContents, mContentsClient, syncGetUrl, "Content-Length"));
        Assert.assertEquals(1, mShouldInterceptRequestHelper.getRequestCountForUrl(syncGetUrl));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotChangeReportedUrl() throws Throwable {
        String input = makePageWithTitle("some title");
        mShouldInterceptRequestHelper.enqueueHtmlResponse(input, /* responseHeaders= */ null);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        Assert.assertEquals(aboutPageUrl, mContentsClient.getOnPageFinishedHelper().getUrl());
        Assert.assertEquals(aboutPageUrl, mContentsClient.getOnPageStartedHelper().getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullInputStreamCausesErrorForMainFrame() throws Throwable {
        final OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();

        mShouldInterceptRequestHelper.enqueueResponseWithStream("text/html", "UTF-8", null);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final int callCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        onReceivedErrorHelper.waitForCallback(callCount);
        Assert.assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testCalledForImage() throws Throwable {
        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        mWebServer.setResponseBase64(
                imagePath,
                CommonResources.FAVICON_DATA_BASE64,
                CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
                addPageToTestServer(
                        mWebServer,
                        "/page_with_image.html",
                        CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithImage);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);

        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertTrue(
                mShouldInterceptRequestHelper
                        .getUrls()
                        .get(1)
                        .endsWith(CommonResources.FAVICON_FILENAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorCallback() throws Throwable {
        mShouldInterceptRequestHelper.enqueueResponseWithStream(null, null, null);
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorHelperCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "foo://bar");
        onReceivedErrorHelper.waitForCallback(onReceivedErrorHelperCallCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testSubresourceError_NullMimeEncodingAndInputStream() throws Throwable {
        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        final String imageUrl =
                mWebServer.setResponseBase64(
                        imagePath,
                        CommonResources.FAVICON_DATA_BASE64,
                        CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
                addPageToTestServer(
                        mWebServer,
                        "/page_with_image.html",
                        CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));
        mShouldInterceptRequestHelper.enqueueResponseForUrlWithStream(imageUrl, null, null, null);
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorHelperCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithImage);
        onReceivedErrorHelper.waitForCallback(onReceivedErrorHelperCallCount);
        Assert.assertEquals(imageUrl, onReceivedErrorHelper.getRequest().getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoOnReceivedErrorCallback() throws Throwable {
        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        final String imageUrl =
                mWebServer.setResponseBase64(
                        imagePath,
                        CommonResources.FAVICON_DATA_BASE64,
                        CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
                addPageToTestServer(
                        mWebServer,
                        "/page_with_image.html",
                        CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));
        mShouldInterceptRequestHelper.enqueueResponseForUrlWithStream(
                imageUrl,
                /* mimeType= */ null,
                /* encoding= */ null,
                /* data= */ EmptyInputStream::new);
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorHelperCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithImage);
        Assert.assertEquals(onReceivedErrorHelperCallCount, onReceivedErrorHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForIframe() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithIframeUrl =
                addPageToTestServer(
                        mWebServer,
                        "/page_with_iframe.html",
                        CommonResources.makeHtmlPageFrom(
                                "", "<iframe src=\"" + aboutPageUrl + "\"/>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(aboutPageUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }

    private void calledForUrlTemplate(final String url) throws Exception {
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageStartedCallCount = mContentsClient.getOnPageStartedHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, url);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(url, mShouldInterceptRequestHelper.getUrls().get(0));

        mContentsClient.getOnPageStartedHelper().waitForCallback(onPageStartedCallCount);
        Assert.assertEquals(
                onPageStartedCallCount + 1,
                mContentsClient.getOnPageStartedHelper().getCallCount());
    }

    private void notCalledForUrlTemplate(final String url) throws Exception {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted")
                        .build();
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        // The intercepting must happen before onPageFinished. Since the IPC messages from the
        // renderer should be delivered in order waiting for onPageFinished is sufficient to
        // 'flush' any pending interception messages.
        Assert.assertEquals(callCount, mShouldInterceptRequestHelper.getCallCount());
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForUnsupportedSchemes() throws Throwable {
        calledForUrlTemplate("foobar://resource/1");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFileUrls_notIntercepted() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted", false);
        calledForUrlTemplate("file:///somewhere/something");
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFileUrls_intercepted() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted", true);
        mShouldInterceptRequestHelper.enqueueHtmlResponse("<html>Hello world</html>", null);
        calledForUrlTemplate("file:///somewhere/something");
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForExistingFiles() throws Throwable {
        final String tmpDir =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getCacheDir()
                        .getPath();
        final String fileName = tmpDir + "/testfile.html";
        final String title = "existing file title";
        TestFileUtil.deleteFile(fileName); // Remove leftover file if any.
        TestFileUtil.createNewHtmlFile(fileName, title, "");
        final String existingFileUrl = "file://" + fileName;

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, existingFileUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(existingFileUrl, mShouldInterceptRequestHelper.getUrls().get(0));

        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
        Assert.assertEquals(title, mActivityTestRule.getTitleOnUiThread(mAwContents));
        Assert.assertEquals(
                onPageFinishedCallCount + 1,
                mContentsClient.getOnPageFinishedHelper().getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForExistingResource() throws Throwable {
        notCalledForUrlTemplate("file:///android_res/raw/resource_file.html");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForNonexistentResource() throws Throwable {
        calledForUrlTemplate("file:///android_res/raw/no_file.html");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForExistingAsset() throws Throwable {
        notCalledForUrlTemplate("file:///android_asset/asset_file.html");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForNonexistentAsset() throws Throwable {
        calledForUrlTemplate("file:///android_res/raw/no_file.html");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setAllowContentAccess(true)")
    public void testNotCalledForExistingContentUrl() throws Throwable {
        final String contentResourceName = "target";
        final String existingContentUrl = TestContentProvider.createContentUrl(contentResourceName);

        notCalledForUrlTemplate(existingContentUrl);

        int contentRequestCount =
                TestContentProvider.getResourceRequestCount(
                        InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        contentResourceName);
        Assert.assertEquals(1, contentRequestCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setAllowContentAccess(true)")
    public void testCalledForNonexistentContentUrl() throws Throwable {
        calledForUrlTemplate("content://org.chromium.webview.NoSuchProvider/foo");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testBaseUrlOfLoadDataSentInRefererHeader() throws Throwable {
        final String imageFile = "a.jpg";
        final String pageHtml = "<img src='" + imageFile + "'>";
        final String baseUrl = "http://localhost:666/";
        final String imageUrl = baseUrl + imageFile;
        final String refererHeaderName = "Referer";
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadDataWithBaseUrlAsync(
                mAwContents, pageHtml, "text/html", false, baseUrl, null);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 1);
        Assert.assertEquals(1, mShouldInterceptRequestHelper.getUrls().size());

        // With PlzNavigate, data URLs are intercepted. See
        // https://codereview.chromium.org/2235303002/.
        // TODO(boliu): Not checking the URL yet. It's the empty data URL which should be fixed in
        // crbug.com/669885.
        Assert.assertNotEquals(imageUrl, mShouldInterceptRequestHelper.getUrls().get(0));
        mShouldInterceptRequestHelper.waitForCallback(callCount + 1);
        Assert.assertEquals(imageUrl, mShouldInterceptRequestHelper.getUrls().get(1));

        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(imageUrl).getRequestHeaders();
        Assert.assertTrue(headers.containsKey(refererHeaderName));
        Assert.assertEquals(baseUrl, headers.get(refererHeaderName));
    }

    private static class DeadlockingAwContentsClient extends TestAwContentsClient {
        public DeadlockingAwContentsClient(CountDownLatch ready, CountDownLatch wait) {
            mReady = ready;
            mWait = wait;
        }

        @Override
        public WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request) {
            mReady.countDown();
            try {
                mWait.await();
            } catch (InterruptedException e) {
                // ignore
            }
            return null;
        }

        private final CountDownLatch mReady;
        private final CountDownLatch mWait;
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDeadlock() throws Throwable {
        // The client will lock shouldInterceptRequest to wait for the UI thread.
        // On the UI thread, we will try engaging the IO thread by executing
        // an action that causes IPC message sending. If the client callback
        // is executed on the IO thread, this will cause a deadlock.
        CountDownLatch waitForShouldInterceptRequest = new CountDownLatch(1);
        CountDownLatch signalAfterSendingIpc = new CountDownLatch(1);
        DeadlockingAwContentsClient client =
                new DeadlockingAwContentsClient(
                        waitForShouldInterceptRequest, signalAfterSendingIpc);
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        mAwContents = mTestContainerView.getAwContents();
        mActivityTestRule.loadUrlAsync(mAwContents, "http://www.example.com");
        waitForShouldInterceptRequest.await();
        // The following call will try to send an IPC and wait for a reply from renderer.
        // We do not check the actual result, because it can be bogus. The important
        // thing is that the call does not cause a deadlock.
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, client, "1+1");
        signalAfterSendingIpc.countDown();
    }

    // Webview must be able to intercept requests with the content-id scheme.
    // See https://crbug.com/739658.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testContentIdImage() throws Throwable {
        final String imageContentIdUrl = "cid://intercept-me";
        final String pageUrl =
                addPageToTestServer(
                        mWebServer,
                        "/main.html",
                        CommonResources.makeHtmlPageFrom(
                                "", "<img src=\'" + imageContentIdUrl + "\'>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(imageContentIdUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }

    // Webview must be able to intercept requests with the content-id scheme.
    // See https://crbug.com/739658.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentIdIframe() throws Throwable {
        final String iframeContentIdUrl = "cid://intercept-me";
        final String pageUrl =
                addPageToTestServer(
                        mWebServer,
                        "/main.html",
                        CommonResources.makeHtmlPageFrom(
                                "", "<iframe src=\'" + iframeContentIdUrl + "\'></iframe>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(iframeContentIdUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadDataWithBaseUrlTriggersShouldInterceptRequest() throws Throwable {
        String data = "foo";
        String mimeType = "text/plain";
        boolean isBase64Encoded = false;
        String baseUrl = "http://foo.bar";
        String historyUrl = "http://foo.bar";

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                data,
                mimeType,
                isBase64Encoded,
                baseUrl,
                historyUrl);
        Assert.assertEquals(callCount + 1, mShouldInterceptRequestHelper.getCallCount());
        // TODO(boliu): Not checking the URL yet. It's the empty data URL which should be fixed in
        // crbug.com/669885.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadDataShouldTriggerShouldInterceptRequest() throws Throwable {
        String data = "foo";
        String mimeType = "text/plain";
        boolean isBase64Encoded = false;

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                data,
                mimeType,
                isBase64Encoded);
        Assert.assertEquals(callCount + 1, mShouldInterceptRequestHelper.getCallCount());
        Assert.assertTrue(mShouldInterceptRequestHelper.getUrls().get(0).contains(data));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadDataUrl_notIntercepted() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted", false);
        String url = "data:text/plain,foo";

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        Assert.assertEquals(callCount + 1, mShouldInterceptRequestHelper.getCallCount());
        Assert.assertEquals(url, mShouldInterceptRequestHelper.getUrls().get(0));
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadDataUrl_intercepted() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted", true);
        String url = "data:text/plain,foo";

        mShouldInterceptRequestHelper.enqueueHtmlResponse(
                "<html>Hello world</html>", /* responseHeaders= */ null);
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        Assert.assertEquals(callCount + 1, mShouldInterceptRequestHelper.getCallCount());
        Assert.assertEquals(url, mShouldInterceptRequestHelper.getUrls().get(0));
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForHttpRedirect() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String redirectUrl = mWebServer.setRedirect("/302.html", aboutPageUrl);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), redirectUrl);
        // This should only be called once, for the original URL, not the final URL.
        Assert.assertEquals(callCount + 1, mShouldInterceptRequestHelper.getCallCount());
        Assert.assertEquals(redirectUrl, mShouldInterceptRequestHelper.getUrls().get(0));
    }

    private static final String BASE_URL = "http://some.origin.test/index.html";
    private static final String UNINTERESTING_HTML = "<html><head></head><body>some</body></html>";

    private Future<String> loadPageAndFetchInternal(String url, String stringArgs)
            throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final SettableFuture<String> future = SettableFuture.create();
        String name = "fetchFuture";
        Object injectedObject =
                new Object() {
                    @JavascriptInterface
                    public void success(String type) {
                        future.set(type);
                    }

                    @JavascriptInterface
                    public void error() {
                        future.set("error");
                    }
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(mAwContents, injectedObject, name);

        if (url != null) {
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        } else {
            mActivityTestRule.loadDataWithBaseUrlSync(
                    mAwContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    UNINTERESTING_HTML,
                    "text/html",
                    false,
                    BASE_URL,
                    null);
        }

        String template =
                "Promise.resolve().then(() => fetch(%s))"
                        + ".then((res) => %s.success(res.type), () => %s.error())";
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, String.format(template, stringArgs, name, name));
        return future;
    }

    private Future<String> loadDataAndFetch(String url, String method) throws Throwable {
        return loadPageAndFetchInternal(null, String.format("'%s', {method: '%s'}", url, method));
    }

    private Future<String> loadDataAndFetch(String url) throws Throwable {
        return loadDataAndFetch(url, "GET");
    }

    private Future<String> loadUrlAndFetch(String pageUrl, String fetchUrl, String method)
            throws Throwable {
        return loadPageAndFetchInternal(
                pageUrl, String.format("'%s', {method: '%s'}", fetchUrl, method));
    }

    private Future<String> loadUrlAndFetch(String pageUrl, String fetchUrl) throws Throwable {
        return loadUrlAndFetch(pageUrl, fetchUrl, "GET");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testObserveCorsSuccess() throws Throwable {
        final List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(new Pair("access-control-allow-origin", "http://some.origin.test"));
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        final Future<String> future = loadDataAndFetch(destinationUrl);
        Assert.assertEquals(
                "fetch should succeed",
                "cors",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(destinationUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testObserveCorsFailure() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(new Pair("access-control-allow-origin", "http://example.com"));
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        final Future<String> future = loadDataAndFetch(destinationUrl);
        // The request fails due to origin mismatch.
        Assert.assertEquals(
                "fetch should fail",
                "error",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(destinationUrl, mShouldInterceptRequestHelper.getUrls().get(1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testObserveCorsPreflightSuccess() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(new Pair("access-control-allow-origin", "http://some.origin.test"));
        headers.add(new Pair("access-control-allow-methods", "PUT"));
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        // PUT is not a safelisted method and triggers a preflight.
        final Future<String> future = loadDataAndFetch(destinationUrl, "PUT");
        Assert.assertEquals(
                "fetch should succeed",
                "cors",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(3, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(
                "preflight request should be visible to shouldInterceptRequest",
                destinationUrl,
                mShouldInterceptRequestHelper.getUrls().get(1));
        Assert.assertEquals(
                "actual request should be visible to shouldInterceptRequest",
                destinationUrl,
                mShouldInterceptRequestHelper.getUrls().get(2));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testObserveCorsPreflightFailure() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final List<Pair<String, String>> headers = new ArrayList<>();
        headers.add(new Pair("access-control-allow-origin", "http://some.origin.test"));
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        // PUT is not a safelisted method and triggers a preflight.
        final Future<String> future = loadDataAndFetch(destinationUrl, "PUT");
        // The request fails due to the lack of access-control-allow-methods.
        Assert.assertEquals(
                "fetch should fail",
                "error",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(
                "preflight request should be visible to shouldInterceptRequest",
                destinationUrl,
                mShouldInterceptRequestHelper.getUrls().get(1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testInjectCorsSuccess() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // The respond the web server provides doesn't have access-control-allow-origin, but that
        // doesn't matter.
        final List<Pair<String, String>> headers = new ArrayList<>();
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        // Injecting a response which has a matching access-control-allow-origin
        mShouldInterceptRequestHelper.enqueueResponseForUrl(
                destinationUrl,
                "text/plain",
                "utf-8",
                /* data= */ null,
                200,
                "OK",
                Map.of("access-control-allow-origin", "http://some.origin.test"));

        final Future<String> future = loadDataAndFetch(destinationUrl);
        Assert.assertEquals(
                "fetch should succeed",
                "cors",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(destinationUrl, mShouldInterceptRequestHelper.getUrls().get(1));

        Assert.assertEquals(0, mWebServer.getRequestCount("/hello.txt"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testInterceptedCookieHeaders_readWriteEnabled() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setIncludeCookiesOnIntercept(true);
        try (HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                "Android.WebView.ShouldInterceptRequest."
                                        + "SetCookieHeader.TimeToRun",
                                3)
                        .expectAnyRecord(
                                "Android.WebView.ShouldInterceptRequest."
                                        + "GetCookieHeader.PostMojo.TimeToRun")
                        .expectBooleanRecord(
                                "Android.WebView.ShouldInterceptRequest."
                                        + "DidIncludeMultiCookieHeader",
                                true)
                        .build()) {
            var cookieManager = mAwContents.getBrowserContextForPublicApi().getCookieManager();
            cookieManager.removeAllCookies();
            final String destinationUrl =
                    mWebServer.setResponse("/hello.txt", "", new ArrayList<>());

            cookieManager.setCookie(destinationUrl, "blah=yo");

            List<String> multiCookies = List.of("bar=baz", "baz=foo");
            String multiCoookieString = String.join(MULTI_COOKIE_VALUE_SEPARATOR, multiCookies);

            // Forcing cookies to be set in the response.
            // Ensures that the standard Set-Cookie header can be combined with multi-headers
            // without collision.
            Map<String, String> responseHeaders =
                    Map.of("set-cookie", "foo=bar", MULTI_COOKIE_HEADER_NAME, multiCoookieString);

            mShouldInterceptRequestHelper.enqueueHtmlResponseForUrl(
                    destinationUrl, "hello", responseHeaders);

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), destinationUrl);

            // These are the cookies that were sent before we set a new one.
            var resourceRequest = mShouldInterceptRequestHelper.getRequestsForUrl(destinationUrl);
            Assert.assertTrue(resourceRequest.getRequestHeaders().containsKey("Cookie"));
            Assert.assertEquals("blah=yo", resourceRequest.getRequestHeaders().get("Cookie"));

            // And then we should see our new value in the cookie manager.
            Set<String> cookies = getCookies(cookieManager, destinationUrl);
            Assert.assertEquals(Set.of("blah=yo", "foo=bar", "bar=baz", "baz=foo"), cookies);
            histogramExpectation.assertExpected();
        }
    }

    private static Set<String> getCookies(AwCookieManager cookieManager, String destinationUrl) {
        String cookie = cookieManager.getCookie(destinationUrl);
        Assert.assertNotNull(cookie);
        String[] split = cookie.split(";");
        Set<String> cookies = new HashSet<>();
        for (String s : split) {
            cookies.add(s.trim());
        }
        return cookies;
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testInterceptedCookieHeaders_readWriteDisabled() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setIncludeCookiesOnIntercept(false);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.WebView.ShouldInterceptRequest."
                                        + "SetCookieHeader.TimeToRun")
                        .expectNoRecords(
                                "Android.WebView.ShouldInterceptRequest."
                                        + "GetCookieHeader.PostMojo.TimeToRun")
                        .expectBooleanRecord(
                                "Android.WebView.ShouldInterceptRequest."
                                        + "DidIncludeMultiCookieHeader",
                                false)
                        .build();
        var cookieManager = mAwContents.getBrowserContextForPublicApi().getCookieManager();
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", new ArrayList<>());

        cookieManager.setCookie(destinationUrl, "blah=yo");

        // Forcing a cookie to be set in the response
        Map<String, String> responseHeaders = Map.of("set-cookie", "foo=bar");
        mShouldInterceptRequestHelper.enqueueHtmlResponseForUrl(
                destinationUrl, "hello", responseHeaders);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), destinationUrl);

        // Intercept should not contain the Cookie header.
        var resourceRequest = mShouldInterceptRequestHelper.getRequestsForUrl(destinationUrl);
        Assert.assertFalse(resourceRequest.getRequestHeaders().containsKey("Cookie"));

        // The set-cookie header in the response should not affect stored cookies.
        Assert.assertEquals("blah=yo", cookieManager.getCookie(destinationUrl));
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testInjectCorsFailure() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // The respond the web server provides has matching access-control-allow-origin, but that
        // doesn't matter.
        final List<Pair<String, String>> headers =
                List.of(new Pair<>("access-control-allow-origin", "http://some.origin.test"));
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        // Injecting a response which doesn't have a matching access-control-allow-origin
        mShouldInterceptRequestHelper.enqueueResponseForUrl(
                destinationUrl,
                "text/plain",
                "utf-8",
                /* data= */ null,
                200,
                "OK",
                Collections.emptyMap());

        final Future<String> future = loadDataAndFetch(destinationUrl);
        Assert.assertEquals(
                "fetch should fail",
                "error",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(destinationUrl, mShouldInterceptRequestHelper.getUrls().get(1));

        Assert.assertEquals(0, mWebServer.getRequestCount("/hello.txt"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testInjectCorsPreflightSuccess() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // The respond the web server provides doesn't have matching access-control-allow-origin,
        // but that doesn't matter.
        final List<Pair<String, String>> headers = new ArrayList<>();
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        // Injecting a response which has a matching access-control-allow-origin and
        // access-control-allow-methods.
        Map<String, String> headersForInjectedResponse =
                Map.of(
                        "access-control-allow-origin", "http://some.origin.test",
                        "access-control-allow-methods", "PUT");
        for (int i = 0; i < 3; i++) {
            // We expect 3 requests, so enqueue 3 responses.
            mShouldInterceptRequestHelper.enqueueResponseForUrl(
                    destinationUrl,
                    "text/plain",
                    "utf-8",
                    /* data= */ null,
                    200,
                    "OK",
                    headersForInjectedResponse);
        }

        // PUT is not a safe-listed method and triggers a preflight.
        final Future<String> future = loadDataAndFetch(destinationUrl, "PUT");
        Assert.assertEquals(
                "fetch should succeed",
                "cors",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(3, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(
                "preflight request should be visible to shouldInterceptRequest",
                destinationUrl,
                mShouldInterceptRequestHelper.getUrls().get(1));
        Assert.assertEquals(
                "actual request should be visible to shouldInterceptRequest",
                destinationUrl,
                mShouldInterceptRequestHelper.getUrls().get(2));

        Assert.assertEquals(0, mWebServer.getRequestCount("/hello.txt"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testInjectCorsPreflightFailure() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // The respond the web server provides has matching access-control-allow-origin and
        // access-control-allow-methods, but that doesn't matter.
        final List<Pair<String, String>> headers =
                List.of(
                        new Pair<>("access-control-allow-origin", "http://some.origin.test"),
                        new Pair<>("access-control-allow-methods", "PUT"));
        final String destinationUrl = mWebServer.setResponse("/hello.txt", "", headers);

        // Injecting a response which doesn't have a matching access-control-allow-origin
        for (int i = 0; i < 2; i++) {
            // Inject the 2 expected responses
            mShouldInterceptRequestHelper.enqueueResponseForUrl(
                    destinationUrl,
                    "text/plain",
                    "utf-8",
                    /* data= */ null,
                    200,
                    "OK",
                    Collections.emptyMap());
        }

        // PUT is not a safelisted method and triggers a preflight.
        final Future<String> future = loadDataAndFetch(destinationUrl, "PUT");
        Assert.assertEquals(
                "fetch should fail",
                "error",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(
                "preflight request should be visible to shouldInterceptRequest",
                destinationUrl,
                mShouldInterceptRequestHelper.getUrls().get(1));

        Assert.assertEquals(0, mWebServer.getRequestCount("/hello.txt"));
    }

    private void respondCorsFetchFromCustomSchemeWithAllowOrigin(
            String customScheme, String allowOrigin, String fetchResult) throws Throwable {
        final String pageUrl = customScheme + "main";
        final String fetchPath = "/test";
        final List<Pair<String, String>> responseHeaders = new ArrayList<>();
        if (allowOrigin != null) {
            responseHeaders.add(new Pair<>("Access-Control-Allow-Origin", allowOrigin));
        }
        final String fetchUrl = mWebServer.setResponse(fetchPath, "", responseHeaders);

        mShouldInterceptRequestHelper.enqueueHtmlResponseForUrl(
                pageUrl, /* data= */ "", /* responseHeaders= */ null);

        final Future<String> future = loadUrlAndFetch(pageUrl, fetchUrl);
        Assert.assertEquals(
                "fetch result check",
                fetchResult,
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(pageUrl, mShouldInterceptRequestHelper.getUrls().get(0));
        Assert.assertEquals(fetchUrl, mShouldInterceptRequestHelper.getUrls().get(1));

        // If a custom scheme is used, "<scheme>://" should be set to the Origin header for
        // cross-origin requests. Hostname and port are not defined well, and can not be used
        // though the proper origin requires a triple of scheme, hostname, and port.
        final WebServer.HTTPRequest fetchRequest = mWebServer.getLastRequest(fetchPath);
        Assert.assertEquals(customScheme, fetchRequest.headerValue("Origin"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorsFetchFromCustomSchemeWithAllowAny() throws Throwable {
        respondCorsFetchFromCustomSchemeWithAllowOrigin("foo://", "*", "cors");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorsFetchFromCustomSchemeWithAllowCustom() throws Throwable {
        final String scheme = "foo://";
        respondCorsFetchFromCustomSchemeWithAllowOrigin(scheme, scheme, "cors");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorsFetchFromCustomSchemeWithAllowDifferentOrigin() throws Throwable {
        respondCorsFetchFromCustomSchemeWithAllowOrigin("foo://", "bar://", "error");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorsFetchFromCustomSchemeWithoutAllowOrigin() throws Throwable {
        respondCorsFetchFromCustomSchemeWithAllowOrigin("foo://", /* allowOrigin= */ null, "error");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorsFetchFromCustomSchemeToCustomScheme() throws Throwable {
        final String pageUrl = "foo://main";
        final String fetchUrl = "bar://test";
        mShouldInterceptRequestHelper.enqueueHtmlResponseForUrl(
                pageUrl, /* data= */ "", /* responseHeaders= */ null);

        // Prepare a response to allow CORS accesses just in case, but should not be reached as
        // Blink rejects such non-http(s) requests before making actual request.
        mShouldInterceptRequestHelper.enqueueHtmlResponseForUrl(
                fetchUrl, /* data= */ "", Map.of("Access-Control-Allow-Origin", "*"));

        final Future<String> future = loadUrlAndFetch(pageUrl, fetchUrl);
        Assert.assertEquals(
                "fetch result check",
                "error",
                future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Only the main resource request reaches to the network stack.
        Assert.assertEquals(1, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(pageUrl, mShouldInterceptRequestHelper.getUrls().get(0));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorsPreflightFromCustomSchemeFail() throws Throwable {
        final String customScheme = "foo://";
        final String pageUrl = customScheme + "main";
        mShouldInterceptRequestHelper.enqueueHtmlResponseForUrl(
                pageUrl, /* data= */ "", /* responseHeaders= */ null);
        final String fetchPathToFail = "/fail";
        final String fetchUrlToFail = mWebServer.setEmptyResponse(fetchPathToFail);
        final String preflightTriggeringMethod = "PUT";

        // This CORS preflight triggering request should fail on the CORS preflight.
        final Future<String> futureToFail =
                loadUrlAndFetch(pageUrl, fetchUrlToFail, preflightTriggeringMethod);
        Assert.assertEquals(
                "fetch result check",
                "error",
                futureToFail.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(pageUrl, mShouldInterceptRequestHelper.getUrls().get(0));
        Assert.assertEquals(fetchUrlToFail, mShouldInterceptRequestHelper.getUrls().get(1));

        // Check if the request was the CORS preflight.
        final WebServer.HTTPRequest fetchRequestToFail = mWebServer.getLastRequest(fetchPathToFail);
        Assert.assertEquals("OPTIONS", fetchRequestToFail.getMethod());
        Assert.assertEquals(customScheme, fetchRequestToFail.headerValue("Origin"));
        Assert.assertEquals(
                preflightTriggeringMethod,
                fetchRequestToFail.headerValue("Access-Control-Request-Method"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCorsPreflightFromCustomSchemePass() throws Throwable {
        final String customScheme = "foo://";
        final String pageUrl = customScheme + "main";
        mShouldInterceptRequestHelper.enqueueHtmlResponseForUrl(
                pageUrl, /* data= */ "", /* responseHeaders= */ null);

        // Craft the expected CORS responses to pass.
        final String fetchPathToPass = "/pass";
        final String preflightTriggeringMethod = "PUT";
        final List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
        responseHeaders.add(new Pair("Access-Control-Allow-Origin", customScheme));
        responseHeaders.add(new Pair("Access-Control-Allow-Methods", preflightTriggeringMethod));
        final String fetchUrlToPass = mWebServer.setResponse(fetchPathToPass, "", responseHeaders);

        final Future<String> futureToPass =
                loadUrlAndFetch(pageUrl, fetchUrlToPass, preflightTriggeringMethod);
        Assert.assertEquals(
                "fetch result check",
                "cors",
                futureToPass.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        Assert.assertEquals(3, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(pageUrl, mShouldInterceptRequestHelper.getUrls().get(0));
        Assert.assertEquals(fetchUrlToPass, mShouldInterceptRequestHelper.getUrls().get(1));
        Assert.assertEquals(fetchUrlToPass, mShouldInterceptRequestHelper.getUrls().get(2));

        // Check if the last request was the actual request.
        final WebServer.HTTPRequest fetchRequestToPass = mWebServer.getLastRequest(fetchPathToPass);
        Assert.assertEquals(preflightTriggeringMethod, fetchRequestToPass.getMethod());
        Assert.assertEquals(customScheme, fetchRequestToPass.headerValue("Origin"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDedicatedWorkerSubresourceIntercepted() throws Throwable {
        final String importScriptJs = addJavaScriptToTestServer(mWebServer, "/test-worker.js", "");
        final String workerJs =
                addJavaScriptToTestServer(
                        mWebServer,
                        "/worker.js",
                        String.format(
                                """
                                            self.onmessage = () => {
                                              importScripts('%s');
                                            }
                                """,
                                importScriptJs));
        final String mainPageUrl =
                addPageToTestServer(
                        mWebServer,
                        "/main",
                        CommonResources.makeHtmlPageFrom(
                                "",
                                String.format(
                                        """
                                                    <script>
                                                      const w = new Worker('%s');
                                                      w.postMessage('msg');
                                                    </script>
                                        """,
                                        workerJs)));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, mainPageUrl);
        // 3 below stands for "/main", "/worker.js", and "/test-worker.js".
        mShouldInterceptRequestHelper.waitForCallback(callCount, 3);

        Assert.assertEquals(
                Arrays.asList(mainPageUrl, workerJs, importScriptJs),
                mShouldInterceptRequestHelper.getUrls());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Network"})
    public void testInterceptRequestAllowsThreadBlocking() throws Throwable {
        // This test asserts that WebView is resilient against blocking operations on the thread
        // pool where shouldInterceptRequest calls are made.
        //
        // It works by creating a web page with a large (`parallelRequestCount`) number of sub
        // requests.
        // The number of sub resource requests is chosen to be high enough to exhaust the number of
        // available threads in the default `base::ThreadPool` SequencedTaskRunner if no care is
        // taken to handle blocking threads.
        //
        // Each call to shouldInterceptRequest will block until all the expected calls have been
        // made, and only then will they be allowed to complete.
        //
        // If the thread pool can be exhausted, then WebView will be prevented from calling
        // `shouldInterceptRequest` for some of the expected parallel pages, and the test will
        // eventually time out.
        //
        // See https://crbug.com/404563944 for background.

        final int parallelRequestCount = 10;

        // Configure the main page with a number of iframes that load different pages.
        String iframeTemplate = "<iframe src=\"/sub.html?idx=%d\"></iframe>\n";
        StringBuilder sb = new StringBuilder("<html>");
        for (int i = 0; i < parallelRequestCount; i++) {
            sb.append(String.format(Locale.ROOT, iframeTemplate, i));
        }
        sb.append("</html>");
        final String mainPageUrl = addPageToTestServer(mWebServer, "/", sb.toString());
        addPageToTestServer(mWebServer, "/sub.html", "subpage");

        final CountDownLatch latch = new CountDownLatch(parallelRequestCount);
        mContentsClient =
                new TestAwContentsClient() {
                    @Override
                    public WebResourceResponseInfo shouldInterceptRequest(
                            AwWebResourceRequest request) {
                        if (request.getUrl().contains("sub.html")) {
                            latch.countDown();
                            try {
                                // Block until all expected calls to `shouldInterceptRequest` have
                                // been made.
                                latch.await();
                            } catch (InterruptedException e) {
                                throw new AssertionError("latch wait was interrupted", e);
                            }
                        }
                        return super.shouldInterceptRequest(request);
                    }
                };

        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();

        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mShouldInterceptRequestHelper = mContentsClient.getShouldInterceptRequestHelper();
        int callCount = mShouldInterceptRequestHelper.getCallCount();

        mActivityTestRule.loadUrlAsync(mAwContents, mainPageUrl);

        // Wait until all calls to shouldInterceptRequest (main page + subpages) have completed.
        // This will time out if the thread pool has been exhausted.
        mShouldInterceptRequestHelper.waitForCallback(callCount, 1 + parallelRequestCount);
        Assert.assertTrue(
                "We should have seen at least all the requests for html pages.",
                mShouldInterceptRequestHelper.getUrls().size() >= 1 + parallelRequestCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }
}
