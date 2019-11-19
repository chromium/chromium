// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.util.List;

/**
 * Tests for the {@link android.webkit.WebView#loadDataWithBaseURL(String, String, String, String,
 * String)} method.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class LoadDataWithBaseUrlTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private AwCookieManager mCookieManager;
    private WebContents mWebContents;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mCookieManager = new AwCookieManager();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mWebContents = mAwContents.getWebContents();
        mCookieManager.setAcceptCookie(true);
    }

    protected void loadDataWithBaseUrlSync(final String data, final String mimeType,
            final boolean isBase64Encoded, final String baseUrl, final String historyUrl)
            throws Throwable {
        mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                mContentsClient.getOnPageFinishedHelper(), data, mimeType, isBase64Encoded, baseUrl,
                historyUrl);
    }

    private static final String SCRIPT_FILE = "/script.js";
    private static final String SCRIPT_LOADED = "Loaded";
    private static final String SCRIPT_NOT_LOADED = "Not loaded";
    private static final String SCRIPT_JS = "script_was_loaded = true;";
    private static final String SIMPLE_HTML = "<html><body></body></html>";

    private String getScriptFileTestPageHtml(final String scriptUrl) {
        return "<html>"
                + "  <head>"
                + "    <title>" + SCRIPT_NOT_LOADED + "</title>"
                + "    <script src='" + scriptUrl + "'></script>"
                + "  </head>"
                + "  <body onload=\"if(script_was_loaded) document.title='" + SCRIPT_LOADED + "'\">"
                + "  </body>"
                + "</html>";
    }

    private String getCrossOriginAccessTestPageHtml(final String iframeUrl) {
        return "<html>"
                + "  <head>"
                + "    <script>"
                + "      function onload() {"
                + "        try {"
                + "          document.title = "
                + "            document.getElementById('frame').contentWindow.location.href;"
                + "        } catch (e) {"
                + "          document.title = 'Exception';"
                + "        }"
                + "      }"
                + "    </script>"
                + "  </head>"
                + "  <body onload='onload()'>"
                + "    <iframe id='frame' src='" + iframeUrl + "'></iframe>"
                + "  </body>"
                + "</html>";
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testImageLoad() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            webServer.setResponseBase64("/" + CommonResources.FAVICON_FILENAME,
                    CommonResources.FAVICON_DATA_BASE64, CommonResources.getImagePngHeaders(true));

            AwSettings contentSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
            contentSettings.setImagesEnabled(true);
            contentSettings.setJavaScriptEnabled(true);

            loadDataWithBaseUrlSync(
                    CommonResources.getOnImageLoadedHtml(CommonResources.FAVICON_FILENAME),
                    "text/html", false, webServer.getBaseUrl(), null);

            Assert.assertEquals("5", mActivityTestRule.getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testScriptLoad() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String scriptUrl = webServer.setResponse(
                    SCRIPT_FILE, SCRIPT_JS, CommonResources.getTextJavascriptHeaders(true));
            final String pageHtml = getScriptFileTestPageHtml(scriptUrl);

            mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(pageHtml, "text/html", false, webServer.getBaseUrl(), null);
            Assert.assertEquals(SCRIPT_LOADED, mActivityTestRule.getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSameOrigin() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);

            mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, webServer.getBaseUrl(), null);
            Assert.assertEquals(frameUrl, mActivityTestRule.getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCrossOrigin() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);
            final String baseUrl = webServer.getBaseUrl().replaceFirst("localhost", "127.0.0.1");

            mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
            loadDataWithBaseUrlSync(html, "text/html", false, baseUrl, null);

            Assert.assertEquals("Exception", mActivityTestRule.getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullBaseUrl() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        final String pageHtml = "<html><body onload='document.title=document.location.href'>"
                + "</body></html>";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, null, null);
        Assert.assertEquals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetCookieInIframe() throws Throwable {
        // Regression test for http://crrev/c/822572 (the first half of crbug.com/793648).
        TestWebServer webServer = TestWebServer.start();
        try {
            List<Pair<String, String>> responseHeaders = CommonResources.getTextHtmlHeaders(true);
            final String cookie = "key=value";
            responseHeaders.add(Pair.create("Set-Cookie", cookie));
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, responseHeaders);
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);
            final String baseUrl = frameUrl;

            loadDataWithBaseUrlSync(html, "text/html", false, baseUrl, null);
            Assert.assertEquals(cookie, mCookieManager.getCookie(frameUrl));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testThirdPartyCookieInIframe() throws Throwable {
        // Regression test for http://crrev/c/827018 (the second half of crbug.com/793648).
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setAcceptThirdPartyCookies(true);
        TestWebServer webServer = TestWebServer.start();
        try {
            List<Pair<String, String>> responseHeaders = CommonResources.getTextHtmlHeaders(true);
            final String cookie = "key=value";
            final String expectedCookieHeader = "Cookie: " + cookie;
            final String frameUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, responseHeaders);
            mCookieManager.setCookie(frameUrl, cookie);
            final String html = getCrossOriginAccessTestPageHtml(frameUrl);
            final String baseUrl = "http://www.google.com/"; // Treat the iframe as 3P.
            loadDataWithBaseUrlSync(html, "text/html", false, baseUrl, null);
            TestWebServer.HTTPRequest request =
                    webServer.getLastRequest("/" + CommonResources.ABOUT_FILENAME);
            Assert.assertEquals("Should send 3P cookies", cookie, request.headerValue("Cookie"));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInvalidBaseUrl() throws Throwable {
        final String invalidBaseUrl = "http://";
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        loadDataWithBaseUrlSync(
                CommonResources.ABOUT_HTML, "text/html", false, invalidBaseUrl, null);
        // Verify that the load succeeds. The actual base url is undefined.
        Assert.assertEquals(
                CommonResources.ABOUT_TITLE, mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testloadDataWithBaseUrlCallsOnPageStarted() throws Throwable {
        final String baseUrl = "http://base.com/";
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int pageStartedCount = onPageStartedHelper.getCallCount();
        final int pageFinishedCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadDataWithBaseUrlAsync(mAwContents, CommonResources.ABOUT_HTML,
                "text/html", false, baseUrl, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        onPageStartedHelper.waitForCallback(pageStartedCount);
        Assert.assertEquals(baseUrl, onPageStartedHelper.getUrl());

        onPageFinishedHelper.waitForCallback(pageFinishedCount);
        Assert.assertEquals("onPageStarted should only be called once", pageStartedCount + 1,
                onPageStartedHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHistoryUrl() throws Throwable {
        final String pageHtml = "<html><body>Hello, world!</body></html>";
        final String baseUrl = "http://example.com";
        // TODO(mnaganov): Use the same string as Android CTS suite uses
        // once GURL issue is resolved (http://code.google.com/p/google-url/issues/detail?id=29)
        final String historyUrl = "http://history.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, historyUrl);
        Assert.assertEquals(historyUrl,
                HistoryUtils.getUrlOnUiThread(
                        InstrumentationRegistry.getInstrumentation(), mWebContents));

        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, null);
        Assert.assertEquals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                HistoryUtils.getUrlOnUiThread(
                        InstrumentationRegistry.getInstrumentation(), mWebContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedUrlIsBaseUrl() throws Throwable {
        final String pageHtml = "<html><body>Hello, world!</body></html>";
        final String baseUrl = "http://example.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, baseUrl);
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, baseUrl, baseUrl);
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        Assert.assertEquals(baseUrl, onPageFinishedHelper.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHistoryUrlIgnoredWithDataSchemeBaseUrl() throws Throwable {
        final String pageHtml = "<html><body>bar</body></html>";
        final String historyUrl = "http://history.com/";
        loadDataWithBaseUrlSync(pageHtml, "text/html", false, "data:foo", historyUrl);
        Assert.assertEquals("data:text/html," + pageHtml,
                HistoryUtils.getUrlOnUiThread(
                        InstrumentationRegistry.getInstrumentation(), mWebContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHistoryUrlNavigation() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String historyUrl = webServer.setResponse("/" + CommonResources.ABOUT_FILENAME,
                    CommonResources.ABOUT_HTML, CommonResources.getTextHtmlHeaders(true));

            final String page1Title = "Page1";
            final String page1Html = "<html><head><title>" + page1Title + "</title>"
                    + "<body>" + page1Title + "</body></html>";

            loadDataWithBaseUrlSync(page1Html, "text/html", false, null, historyUrl);
            Assert.assertEquals(page1Title, mActivityTestRule.getTitleOnUiThread(mAwContents));

            final String page2Title = "Page2";
            final String page2Html = "<html><head><title>" + page2Title + "</title>"
                    + "<body>" + page2Title + "</body></html>";

            final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            mActivityTestRule.loadDataSync(
                    mAwContents, onPageFinishedHelper, page2Html, "text/html", false);
            Assert.assertEquals(page2Title, mActivityTestRule.getTitleOnUiThread(mAwContents));

            HistoryUtils.goBackSync(InstrumentationRegistry.getInstrumentation(), mWebContents,
                    onPageFinishedHelper);
            // The title of first page loaded with loadDataWithBaseUrl.
            Assert.assertEquals(page1Title, mActivityTestRule.getTitleOnUiThread(mAwContents));
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * @return true if |fileUrl| was accessible from a data url with |baseUrl| as it's
     * base URL.
     */
    private boolean canAccessFileFromData(String baseUrl, String fileUrl) throws Throwable {
        final String imageLoaded = "LOADED";
        final String imageNotLoaded = "NOT_LOADED";
        String data = "<html><body>"
                + "<img src=\"" + fileUrl + "\" "
                + "onload=\"document.title=\'" + imageLoaded + "\';\" "
                + "onerror=\"document.title=\'" + imageNotLoaded + "\';\" />"
                + "</body></html>";

        loadDataWithBaseUrlSync(data, "text/html", false, baseUrl, null);

        AwActivityTestRule.pollInstrumentationThread(() -> {
            String title = mActivityTestRule.getTitleOnUiThread(mAwContents);
            return imageLoaded.equals(title) || imageNotLoaded.equals(title);
        });

        return imageLoaded.equals(mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SuppressWarnings("Finally")
    public void testLoadDataWithBaseUrlAccessingFile() throws Throwable {
        // Create a temporary file on the filesystem we can try to read.
        File cacheDir = mActivityTestRule.getActivity().getCacheDir();
        File tempImage = File.createTempFile("test_image", ".png", cacheDir);
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.RGB_565);
        FileOutputStream fos = new FileOutputStream(tempImage);
        try {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, fos);
        } finally {
            fos.close();
        }
        String imagePath = tempImage.getAbsolutePath();

        AwSettings contentSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        contentSettings.setImagesEnabled(true);
        contentSettings.setJavaScriptEnabled(true);

        try {
            final String dataBaseUrl = "data:";
            final String nonDataBaseUrl = "http://example.com";

            mAwContents.getSettings().setAllowFileAccess(false);
            String token = "" + System.currentTimeMillis();
            // All access to file://, including android_asset and android_res is blocked
            // with a data: base URL, regardless of AwSettings.getAllowFileAccess().
            Assert.assertFalse(canAccessFileFromData(
                    dataBaseUrl, "file:///android_asset/asset_icon.png?" + token));
            Assert.assertFalse(canAccessFileFromData(
                    dataBaseUrl, "file:///android_res/raw/resource_icon.png?" + token));
            Assert.assertFalse(
                    canAccessFileFromData(dataBaseUrl, "file://" + imagePath + "?" + token));

            // WebView always has access to android_asset and android_res for non-data
            // base URLs and can access other file:// URLs based on the value of
            // AwSettings.getAllowFileAccess().
            Assert.assertTrue(canAccessFileFromData(
                    nonDataBaseUrl, "file:///android_asset/asset_icon.png?" + token));
            Assert.assertTrue(canAccessFileFromData(
                    nonDataBaseUrl, "file:///android_res/raw/resource_icon.png?" + token));
            Assert.assertFalse(
                    canAccessFileFromData(nonDataBaseUrl, "file://" + imagePath + "?" + token));

            token += "a";
            mAwContents.getSettings().setAllowFileAccess(true);
            // We should still be unable to access any file:// with when loading with a
            // data: base URL, but we should now be able to access the wider file system
            // (still restricted by OS-level permission checks) with a non-data base URL.
            Assert.assertFalse(canAccessFileFromData(
                    dataBaseUrl, "file:///android_asset/asset_icon.png?" + token));
            Assert.assertFalse(canAccessFileFromData(
                    dataBaseUrl, "file:///android_res/raw/resource_icon.png?" + token));
            Assert.assertFalse(
                    canAccessFileFromData(dataBaseUrl, "file://" + imagePath + "?" + token));

            Assert.assertTrue(canAccessFileFromData(
                    nonDataBaseUrl, "file:///android_asset/asset_icon.png?" + token));
            Assert.assertTrue(canAccessFileFromData(
                    nonDataBaseUrl, "file:///android_res/raw/resource_icon.png?" + token));
            Assert.assertTrue(
                    canAccessFileFromData(nonDataBaseUrl, "file://" + imagePath + "?" + token));
        } finally {
            if (!tempImage.delete()) throw new AssertionError();
        }
    }

    /**
     * Disallowed from running on Svelte devices due to OOM errors: crbug.com/598013
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testLoadLargeData() throws Throwable {
        // Chrome only allows URLs up to 2MB in IPC. Test something larger than this.
        // Note that the real URI may be significantly large if it gets encoded into
        // base64.
        final int kDataLength = 5 * 1024 * 1024;
        StringBuilder doc = new StringBuilder();
        doc.append("<html><head></head><body><!-- ");
        int i = doc.length();
        doc.setLength(i + kDataLength);
        while (i < doc.length()) doc.setCharAt(i++, 'A');
        doc.append("--><script>window.gotToEndOfBody=true;</script></body></html>");

        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        loadDataWithBaseUrlSync(doc.toString(), "text/html", false, null, null);
        Assert.assertEquals("true",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "window.gotToEndOfBody"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedWhenInterrupted() throws Throwable {
        // See crbug.com/594001 -- when a javascript: URL is loaded, the pending entry
        // gets discarded and the previous load goes through a different path
        // inside NavigationController.
        final String pageHtml = "<html><body>Hello, world!</body></html>";
        final String baseUrl = "http://example.com/";
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int callCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadDataWithBaseUrlAsync(
                mAwContents, pageHtml, "text/html", false, baseUrl, null);
        mActivityTestRule.loadUrlAsync(mAwContents, "javascript:42");
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals(baseUrl, onPageFinishedHelper.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinishedWithInvalidBaseUrlWhenInterrupted() throws Throwable {
        final String pageHtml = CommonResources.ABOUT_HTML;
        final String invalidBaseUrl = "http://";
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int callCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        mActivityTestRule.loadDataWithBaseUrlAsync(
                mAwContents, pageHtml, "text/html", false, invalidBaseUrl, null);
        mActivityTestRule.loadUrlAsync(mAwContents, "javascript:42");
        onPageFinishedHelper.waitForCallback(callCount);
        // Verify that the load succeeds. The actual base url is undefined.
        Assert.assertEquals(
                CommonResources.ABOUT_TITLE, mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBaseUrlMetrics_empty() throws Throwable {
        loadContentAndCheckMetrics(null, AwContents.UrlScheme.EMPTY);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBaseUrlMetrics_data() throws Throwable {
        loadContentAndCheckMetrics("data:text/html", AwContents.UrlScheme.DATA_SCHEME);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBaseUrlMetrics_http() throws Throwable {
        loadContentAndCheckMetrics("http://www.google.com/", AwContents.UrlScheme.HTTP_SCHEME);
    }

    private void loadContentAndCheckMetrics(String baseUrl, int expectedSchemeEnum)
            throws Throwable {
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        AwContents.DATA_BASE_URL_SCHEME_HISTOGRAM_NAME));
        loadDataWithBaseUrlSync(SIMPLE_HTML, "text/html", false, baseUrl, null);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        AwContents.DATA_BASE_URL_SCHEME_HISTOGRAM_NAME));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        AwContents.DATA_BASE_URL_SCHEME_HISTOGRAM_NAME, expectedSchemeEnum));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWindowOriginForHttpSchemeUrl() throws Throwable {
        String baseUri = "https://google.com";
        AwSettings contentSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        contentSettings.setJavaScriptEnabled(true);
        loadDataWithBaseUrlSync("", "text/html", false, baseUri, null);
        Assert.assertEquals("\"https://google.com\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "window.origin;"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWindowOriginForCustomSchemeUrl() throws Throwable {
        String baseUri = "x-thread://-86516399/2465766146407674724";
        AwSettings contentSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        contentSettings.setJavaScriptEnabled(true);
        loadDataWithBaseUrlSync("", "text/html", false, baseUri, null);
        Assert.assertEquals("\"x-thread://\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "window.origin;"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testXhrForHttpSchemeUrl() throws Throwable {
        Assert.assertTrue(verifyXhrForUrls("https://google.com/1", "https://google.com/2"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    // https://crbug.com/900528
    public void testXhrForCustomSchemeUrl() throws Throwable {
        Assert.assertTrue(verifyXhrForUrls("myscheme://mydomain/1", "myscheme://mydomain/2"));
    }

    /**
     * Verify that XHR can be correctly run with the set base URI, regardless of its scheme.
     *
     * @param baseUri Base URI to start from.
     * @param textUri The text URI to fetch the text from.
     * @throws Throwable
     */
    private boolean verifyXhrForUrls(String baseUri, String textUri) throws Throwable {
        final String successMsg = "SUCCESS";
        final String errorMsg = "ERROR";
        final String data = "<html><head><script type='text/javascript'>"
                + "var xhr = new XMLHttpRequest();"
                + "xhr.open('GET', '" + textUri + "', true);"
                + "xhr.onload = function(e) {"
                + "  if (xhr.readyState === 4 && xhr.status === 200) {"
                + "    document.title = xhr.responseText;"
                + "  } else {"
                + "    console.log('Error status: ' + xhr.statusText);"
                + "    document.title = '" + errorMsg + "';"
                + "  }"
                + "};"
                + "xhr.onerror = function(e) {"
                + "  console.log('Error status: ' + xhr.statusText);"
                + "  document.title = '" + errorMsg + "';"
                + "};"
                + "xhr.send(null);"
                + "</script></head></html>";

        // Intercept TEXT URI request, and respond with 'SUCCESS'.
        TestAwContentsClient client = new TestAwContentsClient() {
            @Override
            public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
                String url = request.url;
                if (textUri.equals(url)) {
                    return new AwWebResourceResponse(
                            "text/plaintext", "utf-8", createInputStreamForString(successMsg));
                } else {
                    return super.shouldInterceptRequest(request);
                }
            }
        };

        // We need extra setup with the new client.
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.getAwSettingsOnUiThread(awContents).setJavaScriptEnabled(true);

        // Starting from BASE URI, load data that loads JS URI.
        mActivityTestRule.loadDataWithBaseUrlSync(awContents, client.getOnPageFinishedHelper(),
                data, "text/html", false, baseUri, null);

        // Polling here as XHR may take extra steps to change the title.
        AwActivityTestRule.pollInstrumentationThread(() -> {
            String title = mActivityTestRule.getTitleOnUiThread(awContents);
            return successMsg.equals(title) || errorMsg.equals(title);
        });
        return successMsg.equals(mActivityTestRule.getTitleOnUiThread(awContents));
    }

    private InputStream createInputStreamForString(String str) {
        return new ByteArrayInputStream(str.getBytes(Charset.defaultCharset()));
    }
}
