// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Base64;
import android.util.Pair;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.net.test.util.TestWebServer;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test suite for loadUrl().
 */
@RunWith(AwJUnit4ClassRunner.class)
public class LoadUrlTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwEmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = AwEmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDataUrl() throws Throwable {
        final String expectedTitle = "dataUrlTest";
        final String data =
                "<html><head><title>" + expectedTitle + "</title></head><body>foo</body></html>";

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadDataSync(
                awContents, contentsClient.getOnPageFinishedHelper(), data, "text/html", false);
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDataUrlBase64() throws Throwable {
        final String expectedTitle = "dataUrlTestBase64";
        final String unencodedData =
                "<html><head><title>" + expectedTitle + "</title></head><body>foo</body></html>";
        final String data = Base64.encodeToString(unencodedData.getBytes(), Base64.NO_PADDING);

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadDataSync(
                awContents, contentsClient.getOnPageFinishedHelper(), data, "text/html", true);
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDataUrlBase64WithTrickyCharacters() throws Throwable {
        // We want all of these characters to be treated literally (e.g. "%3f" should be "%3f")
        final String expectedTextContent =
                "This text\nhas tricky characters: %3f!#$&'()*+,\\/:;=?@[]";
        final String unencodedData =
                "<html><body><pre>" + expectedTextContent + "</pre></body></html>";
        final String data = Base64.encodeToString(unencodedData.getBytes(), Base64.NO_PADDING);

        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadDataSync(
                awContents, contentsClient.getOnPageFinishedHelper(), data, "text/html", true);
        String textContent =
                mActivityTestRule.getJavaScriptResultBodyTextContent(awContents, contentsClient);
        // The JavaScript result escapes special characters - we need to unescape them.
        textContent = textContent.replace("\\n", "\n").replace("\\\\", "\\");
        Assert.assertEquals(expectedTextContent, textContent);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDataUrlCharset() throws Throwable {
        // Note that the \u00a3 (pound sterling) is the important character in the following
        // string as it's not in the US_ASCII character set.
        final String expectedTitle = "You win \u00a3100!";
        final String data =
                "<html><head><title>" + expectedTitle + "</title></head><body>foo</body></html>";
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadDataSyncWithCharset(awContents,
                contentsClient.getOnPageFinishedHelper(), data, "text/html", false, "UTF-8");
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    private static class OnProgressChangedClient extends TestAwContentsClient {
        List<Integer> mProgresses = new ArrayList<Integer>();

        @Override
        public void onProgressChanged(int progress) {
            super.onProgressChanged(progress);
            mProgresses.add(Integer.valueOf(progress));
            if (progress == 100 && mCallbackHelper.getCallCount() == 0) {
                mCallbackHelper.notifyCalled();
            }
        }

        public void waitForFullLoad() throws TimeoutException {
            mCallbackHelper.waitForFirst();
        }
        private CallbackHelper mCallbackHelper = new CallbackHelper();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testProgress() throws Throwable {
        final OnProgressChangedClient contentsClient = new OnProgressChangedClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);

        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = webServer.setResponse("/page.html", "<html>Page</html>", null);

            /* Before loading, progress is 100. */
            InstrumentationRegistry.getInstrumentation().runOnMainSync(
                    ()
                            -> Assert.assertEquals(100,
                                    testContainerView.getAwContents().getMostRecentProgress()));

            InstrumentationRegistry.getInstrumentation().runOnMainSync(
                    () -> testContainerView.getAwContents().loadUrl(url, null));
            contentsClient.waitForFullLoad();
            /* After loading, progress is 100. */
            InstrumentationRegistry.getInstrumentation().runOnMainSync(
                    ()
                            -> Assert.assertEquals(100,
                                    testContainerView.getAwContents().getMostRecentProgress()));

            /* At some point during the load, progress was not 100. */
            Assert.assertTrue(contentsClient.mProgresses.size() > 1);
            Assert.assertFalse(contentsClient.mProgresses.get(0) == 100);

        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    protected void loadUrlWithExtraHeadersSync(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            final String url,
            final Map<String, String> extraHeaders) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> awContents.loadUrl(url, extraHeaders));
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    private static List<Pair<String, String>> createHeadersList(String[] namesAndValues) {
        List<Pair<String, String>> result = new ArrayList<Pair<String, String>>();
        for (int i = 0; i < namesAndValues.length; i += 2) {
            result.add(Pair.create(namesAndValues[i], namesAndValues[i + 1]));
        }
        return result;
    }

    private static Map<String, String> createHeadersMap(String[] namesAndValues) {
        Map<String, String> result = new HashMap<String, String>();
        for (int i = 0; i < namesAndValues.length; i += 2) {
            result.put(namesAndValues[i], namesAndValues[i + 1]);
        }
        return result;
    }

    private void validateHeadersValue(final AwContents awContents,
            final TestAwContentsClient contentsClient, String[] extraHeader,
            boolean shouldHeaderExist) throws Exception {
        String textContent =
                mActivityTestRule.getJavaScriptResultBodyTextContent(awContents, contentsClient);
        String[] header_values = textContent.split("\\\\n");
        for (int i = 0; i < extraHeader.length; i += 2) {
            Assert.assertEquals(
                    shouldHeaderExist ? extraHeader[i + 1] : "None", header_values[i / 2]);
        }
    }

    private void validateHeadersFromJson(final AwContents awContents,
            final TestAwContentsClient contentsClient, String[] extraHeader, String jsonName,
            boolean shouldHeaderExist) throws Exception {
        String textContent =
                mActivityTestRule.getJavaScriptResultBodyTextContent(awContents, contentsClient)
                        .replaceAll("\\\\\"", "\"");
        JSONObject jsonObject = new JSONObject(textContent);
        JSONArray jsonArray = jsonObject.getJSONArray(jsonName);
        for (int i = 0; i < extraHeader.length; i += 2) {
            String header = jsonArray.getString(i / 2);
            Assert.assertEquals(shouldHeaderExist ? extraHeader[i + 1] : "None", header);
        }
    }

    private final String encodeUrl(String url) {
        try {
            return URLEncoder.encode(url, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            throw new AssertionError(e);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadUrlWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1", "x-extraHeaders2", "EXTRA-HEADER-DATA2"};

        final String url1 = mTestServer.getURL("/image-response-if-header-not-exists?resource="
                + encodeUrl(CommonResources.FAVICON_DATA_BASE64) + "&header=" + extraHeaders[0]
                + "&header=" + extraHeaders[2]);
        final String url2 = mTestServer.getURL("/image-onload-html?imagesrc=" + encodeUrl(url1)
                + "&header=" + extraHeaders[0] + "&header=" + extraHeaders[2]);

        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                contentsClient.getOnReceivedTitleHelper();
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        loadUrlWithExtraHeadersSync(awContents, contentsClient.getOnPageFinishedHelper(), url2,
                createHeadersMap(extraHeaders));
        // Verify that extra headers are passed to the loaded url.
        validateHeadersValue(awContents, contentsClient, extraHeaders, true);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
        Assert.assertEquals("5", onReceivedTitleHelper.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoOverridingOfExistingHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        final String url = mTestServer.getURL("/echoheader?user-agent");
        String[] extraHeaders = {"user-agent", "Borewicz 07 & Bond 007"};

        loadUrlWithExtraHeadersSync(awContents, contentsClient.getOnPageFinishedHelper(), url,
                createHeadersMap(extraHeaders));
        String header =
                mActivityTestRule.getJavaScriptResultBodyTextContent(awContents, contentsClient);
        // Just check that the value is there, and it's not the one we provided.
        Assert.assertFalse(header.isEmpty());
        Assert.assertFalse(extraHeaders[1].equals(header));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReloadWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1", "x-extraHeaders2", "EXTRA-HEADER-DATA2"};
        final String url =
                mTestServer.getURL("/echoheader?" + extraHeaders[0] + "&" + extraHeaders[2]);

        loadUrlWithExtraHeadersSync(awContents, contentsClient.getOnPageFinishedHelper(), url,
                createHeadersMap(extraHeaders));
        validateHeadersValue(awContents, contentsClient, extraHeaders, true);
        mActivityTestRule.reloadSync(awContents, contentsClient.getOnPageFinishedHelper());
        validateHeadersValue(awContents, contentsClient, extraHeaders, true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRedirectAndReloadWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final String echoRedirectedUrlHeader = "echo header";
        final String echoInitialUrlHeader = "data content";

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1", "x-extraHeaders2", "EXTRA-HEADER-DATA2"};
        final String redirectedUrl = mTestServer.getURL("/echoheader-and-set-data?header="
                + extraHeaders[0] + "&header=" + extraHeaders[2]);
        final String initialUrl =
                mTestServer.getURL("/server-redirect-echoheader?url=" + encodeUrl(redirectedUrl)
                        + "&header=" + extraHeaders[0] + "&header=" + extraHeaders[2]);
        loadUrlWithExtraHeadersSync(awContents, contentsClient.getOnPageFinishedHelper(),
                initialUrl, createHeadersMap(extraHeaders));
        validateHeadersFromJson(
                awContents, contentsClient, extraHeaders, echoRedirectedUrlHeader, true);
        validateHeadersFromJson(
                awContents, contentsClient, extraHeaders, echoInitialUrlHeader, true);

        // WebView will only reload the main page.
        mActivityTestRule.reloadSync(awContents, contentsClient.getOnPageFinishedHelper());
        // No extra headers. This is consistent with legacy behavior.
        validateHeadersFromJson(
                awContents, contentsClient, extraHeaders, echoRedirectedUrlHeader, false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRendererNavigationAndGoBackWithExtraHeaders() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        String[] extraHeaders = {
                "X-ExtraHeaders1", "extra-header-data1", "x-extraHeaders2", "EXTRA-HEADER-DATA2"};
        final String redirectedUrl =
                mTestServer.getURL("/echoheader?" + extraHeaders[0] + "&" + extraHeaders[2]);
        final String initialUrl =
                mTestServer.getURL("/click-redirect?url=" + encodeUrl(redirectedUrl)
                        + "&header=" + extraHeaders[0] + "&header=" + extraHeaders[2]);

        loadUrlWithExtraHeadersSync(awContents, contentsClient.getOnPageFinishedHelper(),
                initialUrl, createHeadersMap(extraHeaders));
        validateHeadersValue(awContents, contentsClient, extraHeaders, true);

        int currentCallCount = contentsClient.getOnPageFinishedHelper().getCallCount();

        // Using a user gesture for the redirect since the history intervention will not allow to
        // go back to a page that does a redirect without any user interaction since the page
        // loaded.
        JSUtils.clickNodeWithUserGesture(testContainerView.getWebContents(), "click");

        contentsClient.getOnPageFinishedHelper().waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        // No extra headers for the page navigated via clicking.
        validateHeadersValue(awContents, contentsClient, extraHeaders, false);

        HistoryUtils.goBackSync(InstrumentationRegistry.getInstrumentation(),
                awContents.getWebContents(), contentsClient.getOnPageFinishedHelper());
        validateHeadersValue(awContents, contentsClient, extraHeaders, true);
    }

    private static class OnReceivedTitleClient extends TestAwContentsClient {
        void setOnReceivedTitleCallback(Runnable onReceivedTitleCallback) {
            mOnReceivedTitleCallback = onReceivedTitleCallback;
        }
        @Override
        public void onReceivedTitle(String title) {
            super.onReceivedTitle(title);
            mOnReceivedTitleCallback.run();
        }
        private Runnable mOnReceivedTitleCallback;
    }

    // See crbug.com/494929. Need to make sure that loading a javascript: URL
    // from inside onReceivedTitle works.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadUrlFromOnReceivedTitle() throws Throwable {
        final OnReceivedTitleClient contentsClient = new OnReceivedTitleClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        contentsClient.setOnReceivedTitleCallback(
                () -> awContents.loadUrl("javascript:testProperty=42;void(0);"));

        TestWebServer webServer = TestWebServer.start();
        try {
            // We need to have a navigation entry, but with an empty title. Note that
            // trying to load a page with no title makes the received title to be
            // the URL of the page so instead we use a "204 No Content" response.
            final String url = webServer.setResponseWithNoContentStatus("/page.html");
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), url);
            TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                    contentsClient.getOnReceivedTitleHelper();
            final String pageTitle = "Hello, World!";
            int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(
                    awContents, "javascript:document.title=\"" + pageTitle + "\";void(0);");
            onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
            Assert.assertEquals(pageTitle, onReceivedTitleHelper.getTitle());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnReceivedTitleForUnchangingTitle() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String title = "Title";
            final String url1 = webServer.setResponse("/page1.html",
                    "<html><head><title>" + title + "</title></head>Page 1</html>", null);
            final String url2 = webServer.setResponse("/page2.html",
                    "<html><head><title>" + title + "</title></head>Page 2</html>", null);
            TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                    contentsClient.getOnReceivedTitleHelper();
            int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), url1);
            onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
            Assert.assertEquals(title, onReceivedTitleHelper.getTitle());
            // Verify that even if we load another page with the same title,
            // onReceivedTitle is still being called.
            onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), url2);
            onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
            Assert.assertEquals(title, onReceivedTitleHelper.getTitle());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCrossDomainNavigation() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        final String data = "<html><head><title>foo</title></head></html>";

        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                contentsClient.getOnReceivedTitleHelper();
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();

        mActivityTestRule.loadDataSync(
                awContents, contentsClient.getOnPageFinishedHelper(), data, "text/html", false);
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
        Assert.assertEquals("foo", onReceivedTitleHelper.getTitle());
        TestWebServer webServer = TestWebServer.start();

        try {
            final String url =
                    webServer.setResponse("/page.html", CommonResources.ABOUT_HTML, null);
            onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), url);
            onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
            Assert.assertEquals(CommonResources.ABOUT_TITLE, onReceivedTitleHelper.getTitle());
        } finally {
            webServer.shutdown();
        }
    }

    // Test loadDataSync() with a page containing an iframe that has a data:
    // URL for its source. WebView handles conversion from data: URLs to origins
    // in  a different way than normal desktop and Android builds so we want to
    // make sure commit time checks properly pass on WebView.
    // See http://crbug.com/1013171 for details.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadDataWithDataUrlIframe() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        final String iframeLoadedMessage = "iframe loaded";
        final String iframeHtml = "<html><body><script>"
                + "console.log('" + iframeLoadedMessage + "')"
                + ";</script></body></html>";
        final String pageHtml = "<html><body>"
                + "<iframe src=\"data:text/html," + iframeHtml + "\"></iframe>"
                + "</body></html>";

        CallbackHelper onPageFinishedHelper = contentsClient.getOnPageFinishedHelper();
        int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();

        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                contentsClient.getAddMessageToConsoleHelper();
        int logCallCount = addMessageToConsoleHelper.getCallCount();

        // Test load with an anonymous opaque origin.
        mActivityTestRule.loadDataSync(
                awContents, contentsClient.getOnPageFinishedHelper(), pageHtml, "text/html", false);
        onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);

        addMessageToConsoleHelper.waitForCallback(logCallCount);
        Assert.assertEquals(iframeLoadedMessage, addMessageToConsoleHelper.getMessage());
    }

    // Test loadUrlSync() with a page containing an iframe that has a data: URL
    // for its source. WebView handles conversion from data: URLs to origins in
    // a different way than normal desktop and Android builds so we want to make
    // sure commit time checks properly pass on WebView.
    // See http://crbug.com/1013171 for details.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadUrlWithDataUrlIframe() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        final String iframeLoadedMessage = "iframe loaded";
        final String iframeHtml = "<html><body><script>"
                + "console.log('" + iframeLoadedMessage + "')"
                + ";</script></body></html>";
        final String pageHtml = "<html><body>"
                + "<iframe src=\"data:text/html," + iframeHtml + "\"></iframe>"
                + "</body></html>";

        CallbackHelper onPageFinishedHelper = contentsClient.getOnPageFinishedHelper();
        int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();

        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                contentsClient.getAddMessageToConsoleHelper();
        int logCallCount = addMessageToConsoleHelper.getCallCount();

        // Test load with an opaque origin that contains precursor info.
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = webServer.setResponse("/page.html", pageHtml, null);

            mActivityTestRule.loadUrlSync(
                    awContents, contentsClient.getOnPageFinishedHelper(), url);
            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);

            addMessageToConsoleHelper.waitForCallback(logCallCount);
            Assert.assertEquals(iframeLoadedMessage, addMessageToConsoleHelper.getMessage());
        } finally {
            webServer.shutdown();
        }
    }
}
