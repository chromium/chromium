// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import android.util.Pair;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/** Tests for the ContentViewClient.onPageFinished() method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClientOnPageFinishedTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public ClientOnPageFinishedTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        setTestAwContentsClient(new TestAwContentsClient());
    }

    private void setTestAwContentsClient(TestAwContentsClient contentsClient) {
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPassesCorrectUrl() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        String html = "<html><body>Simple page.</body></html>";
        int currentCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadDataAsync(mAwContents, html, "text/html", false);

        onPageFinishedHelper.waitForCallback(currentCallCount);
        Assert.assertEquals("data:text/html," + html, onPageFinishedHelper.getUrl());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledAfterError() throws Throwable {
        class LocalTestClient extends TestAwContentsClient {
            private boolean mIsOnReceivedErrorCalled;
            private boolean mIsOnPageFinishedCalled;
            private boolean mAllowAboutBlank;

            @Override
            public void onReceivedError(AwWebResourceRequest request, AwWebResourceError error) {
                Assert.assertEquals(
                        "onReceivedError called twice for " + request.url,
                        false,
                        mIsOnReceivedErrorCalled);
                mIsOnReceivedErrorCalled = true;
                Assert.assertEquals(
                        "onPageFinished called before onReceivedError for " + request.url,
                        false,
                        mIsOnPageFinishedCalled);
                super.onReceivedError(request, error);
            }

            @Override
            public void onPageFinished(String url) {
                if (mAllowAboutBlank && ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(url)) {
                    super.onPageFinished(url);
                    return;
                }
                Assert.assertEquals(
                        "onPageFinished called twice for " + url, false, mIsOnPageFinishedCalled);
                mIsOnPageFinishedCalled = true;
                Assert.assertEquals(
                        "onReceivedError not called before onPageFinished for " + url,
                        true,
                        mIsOnReceivedErrorCalled);
                super.onPageFinished(url);
            }

            void setAllowAboutBlank() {
                mAllowAboutBlank = true;
            }
        }
        LocalTestClient testContentsClient = new LocalTestClient();
        setTestAwContentsClient(testContentsClient);

        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        String invalidUrl = "http://localhost:7/non_existent";
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, invalidUrl);

        Assert.assertEquals(invalidUrl, onReceivedErrorHelper.getRequest().url);
        Assert.assertEquals(invalidUrl, onPageFinishedHelper.getUrl());

        // Rather than wait a fixed time to see that another onPageFinished callback isn't issued
        // we load a valid page. Since callbacks arrive sequentially, this will ensure that
        // any extra calls of onPageFinished / onReceivedError will arrive to our client.
        testContentsClient.setAllowAboutBlank();
        mActivityTestRule.loadUrlSync(
                mAwContents, onPageFinishedHelper, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledAfterRedirectedUrlIsOverridden() throws Throwable {
        /*
         * If url1 is redirected url2, and url2 load is overridden, onPageFinished should still be
         * called for url2.
         * Steps:
         * 1. load url1. url1 onPageStarted
         * 2. server redirects url1 to url2. url2 onPageStarted
         * 3. shouldOverridedUrlLoading called for url2 and returns true
         * 4. url2 onPageFinishedCalled
         */

        TestWebServer webServer = TestWebServer.start();
        try {
            final String redirectTargetPath = "/redirect_target.html";
            final String redirectTargetUrl =
                    webServer.setResponse(
                            redirectTargetPath, "<html><body>hello world</body></html>", null);
            final String redirectUrl = webServer.setRedirect("/302.html", redirectTargetUrl);

            final TestAwContentsClient.ShouldOverrideUrlLoadingHelper urlOverrideHelper =
                    mContentsClient.getShouldOverrideUrlLoadingHelper();
            // Override the load of redirectTargetUrl
            urlOverrideHelper.setShouldOverrideUrlLoadingReturnValue(true);

            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();

            final int currentOnPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(mAwContents, redirectUrl);

            onPageFinishedHelper.waitForCallback(currentOnPageFinishedCallCount);
            // onPageFinished needs to be called for redirectTargetUrl, but not for redirectUrl
            Assert.assertEquals(redirectTargetUrl, onPageFinishedHelper.getUrl());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForValidSubresources() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String testHtml = "<html><head>Header</head><body>Body</body></html>";
            final String testPath = "/test.html";
            final String syncPath = "/sync.html";

            final String testUrl = webServer.setResponse(testPath, testHtml, null);
            final String syncUrl = webServer.setResponse(syncPath, testHtml, null);

            Assert.assertEquals(0, onPageFinishedHelper.getCallCount());
            final int pageWithSubresourcesCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadDataAsync(
                    mAwContents,
                    "<html><iframe src=\"" + testUrl + "\" /></html>",
                    "text/html",
                    false);

            onPageFinishedHelper.waitForCallback(pageWithSubresourcesCallCount);

            // Rather than wait a fixed time to see that an onPageFinished callback isn't issued
            // we load another valid page. Since callbacks arrive sequentially if the next callback
            // we get is for the synchronizationUrl we know that the previous load did not schedule
            // a callback for the iframe.
            final int synchronizationPageCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(mAwContents, syncUrl);

            onPageFinishedHelper.waitForCallback(synchronizationPageCallCount);
            Assert.assertEquals(syncUrl, onPageFinishedHelper.getUrl());
            Assert.assertEquals(2, onPageFinishedHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledForHistoryApi() throws Throwable {
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        TestWebServer webServer = TestWebServer.start();
        try {
            final String testHtml = "<html><head>Header</head><body>Body</body></html>";
            final String testPath = "/test.html";
            final String historyPath = "/history.html";
            final String syncPath = "/sync.html";

            final String testUrl = webServer.setResponse(testPath, testHtml, null);
            final String historyUrl = webServer.getResponseUrl(historyPath);
            final String syncUrl = webServer.setResponse(syncPath, testHtml, null);

            Assert.assertEquals(0, onPageStartedHelper.getCallCount());
            Assert.assertEquals(0, onPageFinishedHelper.getCallCount());
            mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, testUrl);

            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents,
                    mContentsClient,
                    "history.pushState(null, null, '" + historyUrl + "');");

            // Rather than wait a fixed time to see that an onPageFinished callback isn't issued
            // we load another valid page. Since callbacks arrive sequentially if the next callback
            // we get is for the synchronizationUrl we know that the previous load did not schedule
            // a callback for the iframe.
            final int synchronizationPageStartedCallCount = onPageStartedHelper.getCallCount();
            final int synchronizationPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(mAwContents, syncUrl);

            onPageStartedHelper.waitForCallback(synchronizationPageStartedCallCount);
            onPageFinishedHelper.waitForCallback(synchronizationPageFinishedCallCount);
            Assert.assertEquals(syncUrl, onPageStartedHelper.getUrl());
            Assert.assertEquals(2, onPageStartedHelper.getCallCount());
            Assert.assertEquals(syncUrl, onPageFinishedHelper.getUrl());
            Assert.assertEquals(2, onPageFinishedHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledForHrefNavigations() throws Throwable {
        doTestOnPageFinishedCalledForHrefNavigations(false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledForHrefNavigationsWithBaseUrl() throws Throwable {
        doTestOnPageFinishedCalledForHrefNavigations(true);
    }

    private void doTestOnPageFinishedCalledForHrefNavigations(boolean useBaseUrl) throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        TestCallbackHelperContainer.OnPageStartedHelper onPageStartedHelper =
                mContentsClient.getOnPageStartedHelper();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        TestWebServer webServer = TestWebServer.start();
        try {
            final String testHtml =
                    CommonResources.makeHtmlPageFrom(
                            "", "<a href=\"#anchor\" id=\"link\">anchor</a>");
            final String testPath = "/test.html";
            final String testUrl = webServer.setResponse(testPath, testHtml, null);

            if (useBaseUrl) {
                mActivityTestRule.loadDataWithBaseUrlSync(
                        mAwContents,
                        onPageFinishedHelper,
                        testHtml,
                        "text/html",
                        false,
                        webServer.getBaseUrl(),
                        null);
            } else {
                mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, testUrl);
            }

            int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            int onPageStartedCallCount = onPageStartedHelper.getCallCount();

            JSUtils.clickOnLinkUsingJs(
                    InstrumentationRegistry.getInstrumentation(),
                    mAwContents,
                    mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                    "link");

            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
            Assert.assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());

            onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            onPageStartedCallCount = onPageStartedHelper.getCallCount();

            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "window.history.go(-1)");

            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
            Assert.assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationForBlankWebView() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, null);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationAfterNonCommittedLoadFromApi() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebServer webServer = TestWebServer.start();
        try {
            final String noContentUrl = webServer.setResponseWithNoContentStatus("/nocontent.html");
            mActivityTestRule.loadUrlAsync(mAwContents, noContentUrl);
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, noContentUrl);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationWithJavascriptUrlAfterNonCommittedLoadFromApi()
            throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebServer webServer = TestWebServer.start();
        try {
            final CountDownLatch latch = new CountDownLatch(1);
            final String url =
                    webServer.setResponseWithRunnableAction(
                            "/about.html",
                            CommonResources.ABOUT_HTML,
                            null,
                            () -> {
                                try {
                                    Assert.assertTrue(
                                            latch.await(
                                                    SCALED_WAIT_TIMEOUT_MS,
                                                    java.util.concurrent.TimeUnit.MILLISECONDS));
                                } catch (InterruptedException e) {
                                    Assert.fail("Caught InterruptedException " + e);
                                }
                            });
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(mAwContents, url);
            mActivityTestRule.loadUrlAsync(
                    mAwContents,
                    "javascript:(function(){document.body.innerHTML='Hello,%20World!';})()");
            mActivityTestRule.stopLoading(mAwContents);
            // We now have 3 possible outcomes:
            //  - the good one -- onPageFinished only fires for the first load;
            //  - two bad ones:
            //      - onPageFinished fires for the dom mutation, then for the first load. (1)
            //      - onPageFinished fires for the first load, then for the dom mutation; (2)
            // We verify that (1) doesn't happen with the code below. Then we load a sync page,
            // and make sure that we are getting onPageFinished for the sync page, not due
            // to the dom mutation, thus verifying that (2) doesn't happen as well.
            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
            Assert.assertEquals(url, onPageFinishedHelper.getUrl());
            Assert.assertEquals(onPageFinishedCallCount + 1, onPageFinishedHelper.getCallCount());
            latch.countDown(); // Release the server.
            final String syncUrl = webServer.setResponse("/sync.html", "", null);
            mActivityTestRule.loadUrlAsync(mAwContents, syncUrl);
            onPageFinishedHelper.waitForCallback(onPageFinishedCallCount + 1);
            Assert.assertEquals(syncUrl, onPageFinishedHelper.getUrl());
            Assert.assertEquals(onPageFinishedCallCount + 2, onPageFinishedHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationAfterLoadUrl() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String testUrl =
                    webServer.setResponse("/test.html", CommonResources.ABOUT_HTML, null);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), testUrl);
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, null);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNotCalledOnDomModificationAfterLoadData() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            mActivityTestRule.loadDataSync(
                    mAwContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    CommonResources.ABOUT_HTML,
                    "text/html",
                    false);
            doTestOnPageFinishedNotCalledOnDomMutation(webServer, null);
        } finally {
            webServer.shutdown();
        }
    }

    private void doTestOnPageFinishedNotCalledOnDomMutation(TestWebServer webServer, String syncUrl)
            throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final int onPageFinishedCallCount = onPageFinishedHelper.getCallCount();
        // Mutate DOM.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "document.body.innerHTML='Hello, World!'");
        // Rather than wait a fixed time to see that an onPageFinished callback isn't issued
        // we load another valid page. Since callbacks arrive sequentially if the next callback
        // we get is for the synchronizationUrl we know that DOM mutation did not schedule
        // a callback for the iframe.
        if (syncUrl == null) {
            syncUrl = webServer.setResponse("/sync.html", "", null);
            mActivityTestRule.loadUrlAsync(mAwContents, syncUrl);
        }
        onPageFinishedHelper.waitForCallback(onPageFinishedCallCount);
        Assert.assertEquals(syncUrl, onPageFinishedHelper.getUrl());
        Assert.assertEquals(onPageFinishedCallCount + 1, onPageFinishedHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledAfter204Reply() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = webServer.setResponseWithNoContentStatus("/page.html");
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            int currentCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(mAwContents, url);
            onPageFinishedHelper.waitForCallback(currentCallCount);
            Assert.assertEquals(url, onPageFinishedHelper.getUrl());
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Ensure onPageFinished is called for an empty response (if the response status isn't
     * HttpStatus.SC_NO_CONTENT).
     */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledOnEmptyResponse() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = webServer.setEmptyResponse("/page.html");
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            int currentCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(mAwContents, url);
            onPageFinishedHelper.waitForCallback(currentCallCount);
            Assert.assertEquals(url, onPageFinishedHelper.getUrl());
        } finally {
            webServer.shutdown();
        }
    }

    /** Ensure onPageFinished is called when a provisional load is cancelled. */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledOnCancelingProvisionalLoad() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        final CountDownLatch firstUrlLatch = new CountDownLatch(1);
        try {
            final String url =
                    webServer.setResponseWithRunnableAction(
                            "/slow_page.html",
                            "",
                            /* headers= */ null,
                            () -> {
                                try {
                                    // Delay the server response so that we guarantee stopLoading()
                                    // comes before the server response.
                                    Assert.assertTrue(
                                            firstUrlLatch.await(
                                                    SCALED_WAIT_TIMEOUT_MS,
                                                    java.util.concurrent.TimeUnit.MILLISECONDS));
                                } catch (InterruptedException e) {
                                    Assert.fail("Caught InterruptedException " + e);
                                }
                            });
            TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                    mContentsClient.getOnPageFinishedHelper();
            int initialCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(mAwContents, url);

            // Cancel before getting a response from the server.
            mActivityTestRule.stopLoading(mAwContents);

            onPageFinishedHelper.waitForCallback(initialCallCount);
            Assert.assertEquals(url, onPageFinishedHelper.getUrl());
            firstUrlLatch.countDown();

            // Load another page to ensure onPageFinished isn't called several times.
            final String syncUrl = webServer.setResponse("/sync.html", "", /* headers= */ null);
            mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, syncUrl);
            Assert.assertEquals(syncUrl, onPageFinishedHelper.getUrl());
            final int finalCallCount = onPageFinishedHelper.getCallCount();
            Assert.assertEquals(
                    "onPageFinished should be called twice", initialCallCount + 2, finalCallCount);
        } finally {
            webServer.shutdown();
        }
    }

    /** Ensure onPageFinished is called when a committed load is cancelled. */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testCalledOnCancelingCommittedLoad() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        TestWebServer webServer = TestWebServer.start();
        final CountDownLatch serverImageUrlLatch = new CountDownLatch(1);
        final CountDownLatch testDoneLatch = new CountDownLatch(1);
        try {
            final String stallingImageUrl =
                    webServer.setResponseWithRunnableAction(
                            "/stallingImage.html",
                            "",
                            /* headers= */ null,
                            () -> {
                                serverImageUrlLatch.countDown();
                                try {
                                    Assert.assertTrue(
                                            testDoneLatch.await(
                                                    SCALED_WAIT_TIMEOUT_MS,
                                                    java.util.concurrent.TimeUnit.MILLISECONDS));
                                } catch (InterruptedException e) {
                                    Assert.fail("Caught InterruptedException " + e);
                                }
                            });

            final String mainPageHtml =
                    CommonResources.makeHtmlPageFrom("", "<img src=" + stallingImageUrl + ">");
            final String mainPageUrl = webServer.setResponse("/mainPage.html", mainPageHtml, null);

            Assert.assertEquals(0, onPageFinishedHelper.getCallCount());
            mActivityTestRule.loadUrlAsync(mAwContents, mainPageUrl);

            Assert.assertTrue(
                    serverImageUrlLatch.await(
                            SCALED_WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
            Assert.assertEquals(0, onPageFinishedHelper.getCallCount());
            // Our load isn't done since we haven't loaded the image - now cancel the load.
            mActivityTestRule.stopLoading(mAwContents);
            onPageFinishedHelper.waitForCallback(0);
            Assert.assertEquals(1, onPageFinishedHelper.getCallCount());
        } finally {
            testDoneLatch.countDown();
            webServer.shutdown();
        }
    }

    /**
     * Ensure the case when max number of redirects is reached using an SXG fallback
     * url does not crash and results in an error page (due to net::ERR_TO_MANY_REDIRECTS).
     */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMaxRedirect_SXG() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        TestWebServer webServer = TestWebServer.startSsl();
        try {
            List<Pair<String, String>> signedExchangeHeaders =
                    new ArrayList<Pair<String, String>>();
            signedExchangeHeaders.add(
                    Pair.create("Content-Type", "application/signed-exchange;v=b3"));
            signedExchangeHeaders.add(Pair.create("X-Content-Type-Options", "nosniff"));
            final String fallbackUrl = webServer.setResponseWithNotFoundStatus("/404.html");
            final String webpageNotAvailable = "Webpage not available";

            StringBuilder sb = new StringBuilder();
            sb.append("sxg1-b3");
            sb.append((char) 0);
            sb.append((char) 0);
            sb.append((char) fallbackUrl.length());
            sb.append(fallbackUrl);
            final String sxgContents = sb.toString();

            final String emptyResp =
                    webServer.setResponse("/foo.sxg", sxgContents, signedExchangeHeaders);
            final String redirectUrl = webServer.setRedirect("/302.html", "/redirect_1");
            for (int i = 1; i < 18; i++) {
                String redirectUrlLoop =
                        webServer.setRedirect("/redirect_" + i, "/redirect_" + (i + 1));
            }

            String finalRedirect = webServer.setRedirect("/redirect_18", "/foo.sxg");

            // Note the current SXG redirect fallback implementation does not
            // result in onPageFinished, onReceivedError callbacks, see crbug.com/1052242.
            mActivityTestRule.loadUrlAsync(mAwContents, redirectUrl);
            mActivityTestRule.waitForVisualStateCallback(mAwContents);
            Assert.assertEquals(webpageNotAvailable, mAwContents.getTitle());
        } finally {
            webServer.shutdown();
        }
    }

    /** Fragment navigation triggered by history APIs can trigger onPageFinished. */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCalledForHistoryApiFragmentNavigation() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        TestWebServer webServer = TestWebServer.start();
        try {
            final String testHtml = "<html><head>Header</head><body>Body</body></html>";
            final String testPath = "/test.html";
            final String fragmentPath = "/test.html#fragment";

            final String testUrl = webServer.setResponse(testPath, testHtml, null);
            final String fragmentUrl = webServer.getResponseUrl(fragmentPath);

            int currentCallCount = onPageFinishedHelper.getCallCount();
            mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, testUrl);
            onPageFinishedHelper.waitForCallback(currentCallCount);
            Assert.assertEquals(testUrl, onPageFinishedHelper.getUrl());
            Assert.assertEquals(1, onPageFinishedHelper.getCallCount());

            currentCallCount = onPageFinishedHelper.getCallCount();
            // History APIs can trigger fragment navigation, and this fragment navigation will
            // trigger onPageFinished, the parameter url carried by onPageFinished will be the
            // parameter url carried by history API.
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents,
                    mContentsClient,
                    "history.pushState(null, null, '" + fragmentPath + "');");
            onPageFinishedHelper.waitForCallback(currentCallCount);
            Assert.assertEquals(fragmentUrl, onPageFinishedHelper.getUrl());
            Assert.assertEquals(2, onPageFinishedHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }
}
