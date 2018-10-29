// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;

/**
 * Tests for the WebViewClient.shouldInterceptRequest() method.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientShouldInterceptRequestTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static class ShouldInterceptRequestClient extends TestAwContentsClient {

        public static class ShouldInterceptRequestHelper extends CallbackHelper {
            private List<String> mShouldInterceptRequestUrls = new ArrayList<String>();
            private ConcurrentHashMap<String, AwWebResourceResponse> mReturnValuesByUrls =
                    new ConcurrentHashMap<String, AwWebResourceResponse>();
            private ConcurrentHashMap<String, AwWebResourceRequest> mRequestsByUrls =
                    new ConcurrentHashMap<String, AwWebResourceRequest>();
            // This is read on another thread, so needs to be marked volatile.
            private volatile AwWebResourceResponse mShouldInterceptRequestReturnValue;
            void setReturnValue(AwWebResourceResponse value) {
                mShouldInterceptRequestReturnValue = value;
            }
            void setReturnValueForUrl(String url, AwWebResourceResponse value) {
                mReturnValuesByUrls.put(url, value);
            }
            public List<String> getUrls() {
                assert getCallCount() > 0;
                return mShouldInterceptRequestUrls;
            }
            public AwWebResourceResponse getReturnValue(String url) {
                AwWebResourceResponse value = mReturnValuesByUrls.get(url);
                if (value != null) return value;
                return mShouldInterceptRequestReturnValue;
            }
            public AwWebResourceRequest getRequestsForUrl(String url) {
                assert getCallCount() > 0;
                assert mRequestsByUrls.containsKey(url);
                return mRequestsByUrls.get(url);
            }
            public void notifyCalled(AwWebResourceRequest request) {
                mShouldInterceptRequestUrls.add(request.url);
                mRequestsByUrls.put(request.url, request);
                notifyCalled();
            }
        }

        @Override
        public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
            AwWebResourceResponse returnValue =
                    mShouldInterceptRequestHelper.getReturnValue(request.url);
            mShouldInterceptRequestHelper.notifyCalled(request);
            return returnValue;
        }

        private ShouldInterceptRequestHelper mShouldInterceptRequestHelper;

        public ShouldInterceptRequestClient() {
            mShouldInterceptRequestHelper = new ShouldInterceptRequestHelper();
        }

        public ShouldInterceptRequestHelper getShouldInterceptRequestHelper() {
            return mShouldInterceptRequestHelper;
        }
    }

    private static final int TEAPOT_STATUS_CODE = 418;
    private static final String TEAPOT_RESPONSE_PHRASE = "I'm a teapot";

    private String addPageToTestServer(TestWebServer webServer, String httpPath, String html) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return webServer.setResponse(httpPath, html, headers);
    }

    private String addAboutPageToTestServer(TestWebServer webServer) {
        return addPageToTestServer(webServer, "/" + CommonResources.ABOUT_FILENAME,
                CommonResources.ABOUT_HTML);
    }

    private AwWebResourceResponse stringToAwWebResourceResponse(String input) throws Throwable {
        final String mimeType = "text/html";
        final String encoding = "UTF-8";

        return new AwWebResourceResponse(
                mimeType, encoding, new ByteArrayInputStream(input.getBytes(encoding)));
    }

    private TestWebServer mWebServer;
    private ShouldInterceptRequestClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private ShouldInterceptRequestClient.ShouldInterceptRequestHelper mShouldInterceptRequestHelper;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new ShouldInterceptRequestClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mShouldInterceptRequestHelper = mContentsClient.getShouldInterceptRequestHelper();

        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectUrlParam() throws Throwable {
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
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectIsMainFrameParam() throws Throwable {
        final String subframeUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithIframeUrl = addPageToTestServer(mWebServer, "/page_with_iframe.html",
                CommonResources.makeHtmlPageFrom("",
                    "<iframe src=\"" + subframeUrl + "\"/>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithIframeUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(
                false, mShouldInterceptRequestHelper.getRequestsForUrl(subframeUrl).isMainFrame);
        Assert.assertEquals(true,
                mShouldInterceptRequestHelper.getRequestsForUrl(pageWithIframeUrl).isMainFrame);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectMethodParam() throws Throwable {
        final String pageToPostToUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithFormUrl = addPageToTestServer(mWebServer, "/page_with_form.html",
                CommonResources.makeHtmlPageWithSimplePostFormTo(pageToPostToUrl));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithFormUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "GET", mShouldInterceptRequestHelper.getRequestsForUrl(pageWithFormUrl).method);

        callCount = mShouldInterceptRequestHelper.getCallCount();
        JSUtils.clickOnLinkUsingJs(InstrumentationRegistry.getInstrumentation(), mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(), "link");
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "POST", mShouldInterceptRequestHelper.getRequestsForUrl(pageToPostToUrl).method);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectHasUserGestureParam() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final String pageWithLinkUrl = addPageToTestServer(mWebServer, "/page_with_link.html",
                CommonResources.makeHtmlPageWithSimpleLinkTo(aboutPageUrl));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageWithLinkUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(false,
                mShouldInterceptRequestHelper.getRequestsForUrl(pageWithLinkUrl).hasUserGesture);

        mActivityTestRule.waitForPixelColorAtCenterOfView(
                mAwContents, mTestContainerView, CommonResources.LINK_COLOR);
        callCount = mShouldInterceptRequestHelper.getCallCount();
        AwTestTouchUtils.simulateTouchCenterOfView(mTestContainerView);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(
                true, mShouldInterceptRequestHelper.getRequestsForUrl(aboutPageUrl).hasUserGesture);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectRefererHeader() throws Throwable {
        final String refererHeaderName = "Referer";
        final String imageUrl = mWebServer.setResponseBase64(
                "/" + CommonResources.TEST_IMAGE_FILENAME,
                CommonResources.FAVICON_DATA_BASE64,
                CommonResources.getImagePngHeaders(true));
        final String pageUrl = addPageToTestServer(mWebServer, "/main.html",
                CommonResources.makeHtmlPageFrom(
                        "", "<img src=\'" + CommonResources.TEST_IMAGE_FILENAME + "\'>"));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);
        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertEquals(pageUrl, mShouldInterceptRequestHelper.getUrls().get(0));
        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(pageUrl).requestHeaders;
        Assert.assertFalse(headers.containsKey(refererHeaderName));
        Assert.assertEquals(imageUrl, mShouldInterceptRequestHelper.getUrls().get(1));
        headers = mShouldInterceptRequestHelper.getRequestsForUrl(imageUrl).requestHeaders;
        Assert.assertTrue(headers.containsKey(refererHeaderName));
        Assert.assertEquals(pageUrl, headers.get(refererHeaderName));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledWithCorrectHeadersParam() throws Throwable {
        final String headerName = "X-Test-Header-Name";
        final String headerValue = "TestHeaderValue";
        final String syncGetUrl = addPageToTestServer(mWebServer, "/intercept_me",
                CommonResources.ABOUT_HTML);
        final String mainPageUrl = addPageToTestServer(mWebServer, "/main",
                CommonResources.makeHtmlPageFrom("",
                "<script>"
                + "  var xhr = new XMLHttpRequest();"
                + "  xhr.open('GET', '" + syncGetUrl + "', false);"
                + "  xhr.setRequestHeader('" + headerName + "', '" + headerValue + "'); "
                + "  xhr.send(null);"
                + "</script>"));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, mainPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);

        Map<String, String> headers =
                mShouldInterceptRequestHelper.getRequestsForUrl(syncGetUrl).requestHeaders;
        Assert.assertTrue(headers.containsKey(headerName));
        Assert.assertEquals(headerValue, headers.get(headerName));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnLoadResourceCalledWithCorrectUrl() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final ShouldInterceptRequestClient.OnLoadResourceHelper onLoadResourceHelper =
                mContentsClient.getOnLoadResourceHelper();

        int callCount = onLoadResourceHelper.getCallCount();

        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);

        onLoadResourceHelper.waitForCallback(callCount);
        Assert.assertEquals(aboutPageUrl, onLoadResourceHelper.getLastLoadedResource());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnInvalidData() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse(null, null, new ByteArrayInputStream(new byte[0])));
        callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse(null, null, null));
        callCount = mShouldInterceptRequestHelper.getCallCount();
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
        public int read(byte b[]) throws IOException {
            return -1;
        }

        @Override
        public int read(byte b[], int off, int len) throws IOException {
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

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream()));
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
        public int read(byte b[]) throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public int read(byte b[], int off, int len) throws IOException {
            throw new IOException("test exception");
        }

        @Override
        public long skip(long n) throws IOException {
            return n;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotCrashOnThrowingStream() throws Throwable {
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new ThrowingInputStream()));
        int shouldInterceptRequestCallCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();

        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);

        mShouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCallCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
    }

    private static class SlowAwWebResourceResponse extends AwWebResourceResponse {
        private CallbackHelper mReadStartedCallbackHelper = new CallbackHelper();
        private CountDownLatch mLatch = new CountDownLatch(1);

        public SlowAwWebResourceResponse(String mimeType, String encoding, InputStream data) {
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
        final SlowAwWebResourceResponse slowAwWebResourceResponse =
                new SlowAwWebResourceResponse("text/html", encoding,
                        new ByteArrayInputStream(aboutPageData.getBytes(encoding)));

        mShouldInterceptRequestHelper.setReturnValue(slowAwWebResourceResponse);
        int callCount = slowAwWebResourceResponse.getReadStartedCallbackHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        slowAwWebResourceResponse.getReadStartedCallbackHelper().waitForCallback(callCount);

        // Now the AwContents is "stuck" waiting for the SlowInputStream to finish reading so we
        // delete it to make sure that the dangling 'read' task doesn't cause a crash. Unfortunately
        // this will not always lead to a crash but it should happen often enough for us to notice.

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mActivityTestRule.getActivity().removeAllViews());
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        mActivityTestRule.pollUiThread(() -> AwContents.getNativeInstanceCount() == 0);

        slowAwWebResourceResponse.unblockReads();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpStatusCodeAndText() throws Throwable {
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        final String syncGetJs =
                "(function() {"
                + "  var xhr = new XMLHttpRequest();"
                + "  xhr.open('GET', '" + syncGetUrl + "', false);"
                + "  xhr.send(null);"
                + "  console.info('xhr.status = ' + xhr.status);"
                + "  console.info('xhr.statusText = ' + xhr.statusText);"
                + "  return '[' + xhr.status + '][' + xhr.statusText + ']';"
                + "})();";
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));
        Assert.assertEquals("\"[404][Not Found]\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, syncGetJs));

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream()));
        Assert.assertEquals("\"[200][OK]\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, syncGetJs));

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream(),
                        TEAPOT_STATUS_CODE, TEAPOT_RESPONSE_PHRASE, new HashMap<String, String>()));
        Assert.assertEquals("\"[" + TEAPOT_STATUS_CODE + "][" + TEAPOT_RESPONSE_PHRASE + "]\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, syncGetJs));
    }

    private String getHeaderValue(AwContents awContents, TestAwContentsClient contentsClient,
            String url, String headerName) throws Exception {
        final String syncGetJs =
                "(function() {"
                + "  var xhr = new XMLHttpRequest();"
                + "  xhr.open('GET', '" + url + "', false);"
                + "  xhr.send(null);"
                + "  console.info(xhr.getAllResponseHeaders());"
                + "  return xhr.getResponseHeader('" + headerName + "');"
                + "})();";
        String header = mActivityTestRule.executeJavaScriptAndWaitForResult(
                awContents, contentsClient, syncGetJs);

        if (header.equals("null")) return null;
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
        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));
        Assert.assertEquals(clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));
        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", new EmptyInputStream()));
        Assert.assertEquals(clientResponseHeaderValue,
                getHeaderValue(mAwContents, mContentsClient, syncGetUrl, clientResponseHeaderName));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpResponseHeader() throws Throwable {
        final String clientResponseHeaderName = "X-Test-Header-Name";
        final String clientResponseHeaderValue = "TestHeaderValue";
        final String syncGetUrl = mWebServer.getResponseUrl("/intercept_me");
        final Map<String, String> headers = new HashMap<String, String>();
        headers.put(clientResponseHeaderName, clientResponseHeaderValue);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null, 0, null, headers));
        Assert.assertEquals(clientResponseHeaderValue,
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

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null, 0, null, null));
        Assert.assertEquals(
                null, getHeaderValue(mAwContents, mContentsClient, syncGetUrl, "Some-Header"));
    }

    private String makePageWithTitle(String title) {
        return CommonResources.makeHtmlPageFrom("<title>" + title + "</title>",
                "<div> The title is: " + title + " </div>");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCanInterceptMainFrame() throws Throwable {
        final String expectedTitle = "testShouldInterceptRequestCanInterceptMainFrame";
        final String expectedPage = makePageWithTitle(expectedTitle);

        mShouldInterceptRequestHelper.setReturnValue(
                stringToAwWebResourceResponse(expectedPage));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), aboutPageUrl);

        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(mAwContents));
        Assert.assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDoesNotChangeReportedUrl() throws Throwable {
        mShouldInterceptRequestHelper.setReturnValue(
                stringToAwWebResourceResponse(makePageWithTitle("some title")));

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

        mShouldInterceptRequestHelper.setReturnValue(
                new AwWebResourceResponse("text/html", "UTF-8", null));

        final String aboutPageUrl = addAboutPageToTestServer(mWebServer);
        final int callCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, aboutPageUrl);
        onReceivedErrorHelper.waitForCallback(callCount);
        Assert.assertEquals(0, mWebServer.getRequestCount("/" + CommonResources.ABOUT_FILENAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForImage() throws Throwable {
        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        mWebServer.setResponseBase64(imagePath,
                CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
                addPageToTestServer(mWebServer, "/page_with_image.html",
                        CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithImage);
        mShouldInterceptRequestHelper.waitForCallback(callCount, 2);

        Assert.assertEquals(2, mShouldInterceptRequestHelper.getUrls().size());
        Assert.assertTrue(mShouldInterceptRequestHelper.getUrls().get(1).endsWith(
                CommonResources.FAVICON_FILENAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedErrorCallback() throws Throwable {
        mShouldInterceptRequestHelper.setReturnValue(new AwWebResourceResponse(null, null, null));
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorHelperCallCount = onReceivedErrorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "foo://bar");
        onReceivedErrorHelper.waitForCallback(onReceivedErrorHelperCallCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoOnReceivedErrorCallback() throws Throwable {
        final String imagePath = "/" + CommonResources.FAVICON_FILENAME;
        final String imageUrl = mWebServer.setResponseBase64(imagePath,
                CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));
        final String pageWithImage =
                addPageToTestServer(mWebServer, "/page_with_image.html",
                        CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME));
        mShouldInterceptRequestHelper.setReturnValueForUrl(
                imageUrl, new AwWebResourceResponse(null, null, null));
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
        final String pageWithIframeUrl = addPageToTestServer(mWebServer, "/page_with_iframe.html",
                CommonResources.makeHtmlPageFrom("",
                    "<iframe src=\"" + aboutPageUrl + "\"/>"));

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
        Assert.assertEquals(onPageStartedCallCount + 1,
                mContentsClient.getOnPageStartedHelper().getCallCount());
    }

    private void notCalledForUrlTemplate(final String url) throws Exception {
        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        // The intercepting must happen before onPageFinished. Since the IPC messages from the
        // renderer should be delivered in order waiting for onPageFinished is sufficient to
        // 'flush' any pending interception messages.
        Assert.assertEquals(callCount, mShouldInterceptRequestHelper.getCallCount());
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
    public void testCalledForNonexistentFiles() throws Throwable {
        calledForUrlTemplate("file:///somewhere/something");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForExistingFiles() throws Throwable {
        final String tmpDir = InstrumentationRegistry.getInstrumentation()
                                      .getTargetContext()
                                      .getCacheDir()
                                      .getPath();
        final String fileName = tmpDir + "/testfile.html";
        final String title = "existing file title";
        TestFileUtil.deleteFile(fileName);  // Remove leftover file if any.
        TestFileUtil.createNewHtmlFile(fileName, title, "");
        final String existingFileUrl = "file://" + fileName;

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, existingFileUrl);
        mShouldInterceptRequestHelper.waitForCallback(callCount);
        Assert.assertEquals(existingFileUrl, mShouldInterceptRequestHelper.getUrls().get(0));

        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);
        Assert.assertEquals(title, mActivityTestRule.getTitleOnUiThread(mAwContents));
        Assert.assertEquals(onPageFinishedCallCount + 1,
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
    public void testNotCalledForExistingContentUrl() throws Throwable {
        final String contentResourceName = "target";
        final String existingContentUrl = TestContentProvider.createContentUrl(contentResourceName);

        notCalledForUrlTemplate(existingContentUrl);

        int contentRequestCount = TestContentProvider.getResourceRequestCount(
                InstrumentationRegistry.getInstrumentation().getTargetContext(),
                contentResourceName);
        Assert.assertEquals(1, contentRequestCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledForNonexistentContentUrl() throws Throwable {
        calledForUrlTemplate("content://org.chromium.webview.NoSuchProvider/foo");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
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
                mShouldInterceptRequestHelper.getRequestsForUrl(imageUrl).requestHeaders;
        Assert.assertTrue(headers.containsKey(refererHeaderName));
        Assert.assertEquals(baseUrl, headers.get(refererHeaderName));
    }

    private static class DeadlockingAwContentsClient extends TestAwContentsClient {
        public DeadlockingAwContentsClient(CountDownLatch ready, CountDownLatch wait) {
            mReady = ready;
            mWait = wait;
        }

        @Override
        public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
            mReady.countDown();
            try {
                mWait.await();
            } catch (InterruptedException e) {
                // ignore
            }
            return null;
        }

        private CountDownLatch mReady;
        private CountDownLatch mWait;
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
        DeadlockingAwContentsClient client = new DeadlockingAwContentsClient(
                waitForShouldInterceptRequest, signalAfterSendingIpc);
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        mAwContents = mTestContainerView.getAwContents();
        mActivityTestRule.loadUrlAsync(mAwContents, "http://www.example.com");
        waitForShouldInterceptRequest.await();
        // The following call will try to send an IPC and wait for a reply from renderer.
        // We do not check the actual result, because it can be bogus. The important
        // thing is that the call does not cause a deadlock.
        mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, client, "1+1");
        signalAfterSendingIpc.countDown();
    }

    // Webview must be able to intercept requests with the content-id scheme.
    // See https://crbug.com/739658.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testContentIdImage() throws Throwable {
        final String imageContentIdUrl = "cid://intercept-me";
        final String pageUrl = addPageToTestServer(mWebServer, "/main.html",
                CommonResources.makeHtmlPageFrom("", "<img src=\'" + imageContentIdUrl + "\'>"));

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
        final String pageUrl = addPageToTestServer(mWebServer, "/main.html",
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
        mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                mContentsClient.getOnPageFinishedHelper(), data, mimeType, isBase64Encoded, baseUrl,
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
        mActivityTestRule.loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), data,
                mimeType, isBase64Encoded);
        Assert.assertEquals(callCount + 1, mShouldInterceptRequestHelper.getCallCount());
        Assert.assertTrue(mShouldInterceptRequestHelper.getUrls().get(0).contains(data));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadDataUrlShouldTriggerShouldInterceptRequest() throws Throwable {
        String url = "data:text/plain,foo";

        int callCount = mShouldInterceptRequestHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        Assert.assertEquals(callCount + 1, mShouldInterceptRequestHelper.getCallCount());
        Assert.assertEquals(url, mShouldInterceptRequestHelper.getUrls().get(0));
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
}
