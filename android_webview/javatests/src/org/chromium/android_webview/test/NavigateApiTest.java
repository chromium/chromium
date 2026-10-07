// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;
import android.webkit.WebSettings;

import androidx.test.InstrumentationRegistry;
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
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwNavigationParams;
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.test.util.NavigationControllerUtil;
import org.chromium.content_public.browser.test.util.NavigationEntrySimple;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer.HTTPHeader;
import org.chromium.net.test.util.WebServer.HTTPRequest;

import java.util.Collections;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for AwContents#navigate. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class NavigateApiTest extends AwParameterizedTest {
    private static final String PAGE1_PATH = "/index.html";
    private static final String PAGE2_PATH = "/example.html";

    private static final String HEADER_NAME = "myheader";
    private static final String HEADER_VALUE = "myvalue";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwContents mAwContents;

    private TestWebServer mWebServer;
    private String mPage1Url;
    private String mPage2Url;

    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();
    private final TestAwNavigationListener mNavigationClient =
            new TestAwNavigationListener(new CallbackHelper());
    private CallbackHelper mOnPageLoadFinished;

    public NavigateApiTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        AwTestContainerView container =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = container.getAwContents();
        mAwContents.getNavigationClient().addListener(mNavigationClient);

        mOnPageLoadFinished = mContentsClient.getOnPageFinishedHelper();

        mWebServer = TestWebServer.start();
        mPage1Url = mWebServer.setResponse(PAGE1_PATH, "<html><body>foo</body></html>", null);
        mPage2Url = mWebServer.setResponse(PAGE2_PATH, "<html><body>bar</body></html>", null);
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void navigates() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate...
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));

        // ... results in a page load.
        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void navigate_failure_invalidUrl() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IllegalArgumentException thrown =
                            Assert.assertThrows(
                                    IllegalArgumentException.class,
                                    () -> mAwContents.navigate("http://\u0000badurl.com"));
                    Assert.assertTrue(
                            "Message should mention invalid URL: " + thrown.getMessage(),
                            thrown.getMessage().contains("Invalid URL"));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void navigate_failure_rendererDebugUrl() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IllegalArgumentException thrown =
                            Assert.assertThrows(
                                    IllegalArgumentException.class,
                                    () -> mAwContents.navigate("chrome://hang"));
                    Assert.assertTrue(
                            "Message should mention debug URL: " + thrown.getMessage(),
                            thrown.getMessage().contains("URL does not result in a navigation"));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(
            "enable-features=WebViewNavigate,IgnoreDuplicateNavs,"
                    + "WebViewIgnoreDuplicateNavs:duplicate_nav_threshold/3s")
    public void navigate_duplicateReturnsOngoingNavigation() throws Throwable {
        // Load an empty page to exit the initial empty document state. Without this, the initial
        // navigation would have 'should_replace_current_entry' set to true, causing a mismatch with
        // the second navigation's flag and making those navs non-duplicate.
        final String emptyUrl = mWebServer.setResponse("/empty.html", "empty", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), emptyUrl);

        final String htmlPath = "/testDuplicateNav.html";
        final CountDownLatch latch1 = new CountDownLatch(1);
        final CountDownLatch latch2 = new CountDownLatch(1);
        final String url =
                mWebServer.setResponseWithRunnableAction(
                        htmlPath,
                        "response",
                        null,
                        () -> {
                            latch1.countDown();
                            try {
                                latch2.await();
                            } catch (InterruptedException e) {
                            }
                        });

        AtomicReference<AwNavigation> nav1Ref = new AtomicReference<>();
        AtomicReference<AwNavigation> nav2Ref = new AtomicReference<>();

        int currentCallCount = mOnPageLoadFinished.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    nav1Ref.set(mAwContents.navigate(url));
                });
        latch1.await();

        // Second navigation to the same URL within threshold should return the ongoing navigation.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    nav2Ref.set(mAwContents.navigate(url));
                });
        latch2.countDown();

        mOnPageLoadFinished.waitForCallback(currentCallCount);

        Assert.assertNotNull(nav1Ref.get());
        Assert.assertSame(nav1Ref.get(), nav2Ref.get());
        Assert.assertEquals(1, mWebServer.getRequestCount(htmlPath));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void navigationCallback() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();
        AtomicReference<AwNavigation> navigationRef = new AtomicReference<>();

        // Tests that the AwNavigation object returned by navigate...
        ThreadUtils.runOnUiThreadBlocking(() -> navigationRef.set(mAwContents.navigate(mPage1Url)));

        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));

        // ... is the same as the one provided to the navigation callbacks.
        AwNavigation navigation = navigationRef.get();
        Assert.assertNotNull(navigation);
        Assert.assertSame(navigation, mNavigationClient.getLastStartedNavigation());
        Assert.assertSame(navigation, mNavigationClient.getLastCompletedNavigation());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void shouldReplaceCurrentEntry_false() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Tests that calling navigate twice with default arguments...
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage2Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount + 1);

        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE2_PATH));

        // ... results in two history entries.
        NavigationHistory history = mAwContents.getNavigationHistory();
        Assert.assertEquals(2, history.getEntryCount());

        Assert.assertTrue(mAwContents.canGoBack());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void shouldReplaceCurrentEntry_true() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate twice (with shouldReplaceCurrentEntry = true)...
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.navigate(paramsWithReplaceCurrentEntry(mPage1Url)));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.navigate(paramsWithReplaceCurrentEntry(mPage2Url)));
        mOnPageLoadFinished.waitForCallback(currentCallCount + 1);

        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE2_PATH));

        // ... results in only one history entry.
        NavigationHistory history = mAwContents.getNavigationHistory();
        Assert.assertEquals(1, history.getEntryCount());

        Assert.assertFalse(mAwContents.canGoBack());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void extraHeaders() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate with headers...
        AwNavigationParams params = paramsWithExtraHeader(mPage1Url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));

        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));

        // ... sends them to the server.
        String actualValue = getHeader(mWebServer.getLastRequest(PAGE1_PATH), HEADER_NAME);
        Assert.assertEquals(HEADER_VALUE, actualValue);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void extraHeaders_reusedOnBack() throws Exception {
        // Disable caches so going back will trigger a network request.
        mActivityTestRule
                .getAwSettingsOnUiThread(mAwContents)
                .setCacheMode(WebSettings.LOAD_NO_CACHE);

        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate with headers, navigating away and then going back...
        AwNavigationParams params = paramsWithExtraHeader(mPage1Url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage2Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount + 1);

        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.goBack());
        mOnPageLoadFinished.waitForCallback(currentCallCount + 2);

        // ... results in the headers being sent again.
        Assert.assertEquals(2, mWebServer.getRequestCount(PAGE1_PATH));

        String actualValue = getHeader(mWebServer.getLastRequest(PAGE1_PATH), HEADER_NAME);
        Assert.assertEquals(HEADER_VALUE, actualValue);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void extraHeaders_notReusedOnSubsequentNavigation() throws Exception {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate with headers and then without header (but same URL)...
        AwNavigationParams params = paramsWithExtraHeader(mPage1Url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount + 1);

        // ... results in the headers not being sent on the 2nd navigation.
        Assert.assertEquals(2, mWebServer.getRequestCount(PAGE1_PATH));

        Assert.assertNull(getHeader(mWebServer.getLastRequest(PAGE1_PATH), HEADER_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void extraHeaders_handlingRedirects() throws Exception {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        String startPath = "/start_attach.html";
        String redirectToFirstPartyPath = "/redirect_to_first_party_attach.html";
        String redirectToThirdPartyPath = "/redirect_to_third_party_no_attach.html";
        String finishPath = "/finish_no_attach.html";

        try (TestWebServer serverB = TestWebServer.startAdditional()) {
            // Set up an A -> A -> B -> A redirect chain
            String finishUrl = mWebServer.setResponse(finishPath, "Done!", Collections.emptyList());
            String redirectToThirdPartyUrl =
                    serverB.setRedirect(redirectToThirdPartyPath, finishUrl);
            String redirectToFirstPartyUrl =
                    mWebServer.setRedirect(redirectToFirstPartyPath, redirectToThirdPartyUrl);
            String startUrl = mWebServer.setRedirect(startPath, redirectToFirstPartyUrl);

            // Test that calling navigate on a url that will redirect...
            AwNavigationParams params = paramsWithExtraHeader(startUrl, HEADER_NAME, HEADER_VALUE);
            ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));

            mOnPageLoadFinished.waitForCallback(currentCallCount);

            Assert.assertEquals(1, mWebServer.getRequestCount(startPath));
            Assert.assertEquals(1, mWebServer.getRequestCount(redirectToFirstPartyPath));
            Assert.assertEquals(1, serverB.getRequestCount(redirectToThirdPartyPath));
            Assert.assertEquals(1, mWebServer.getRequestCount(finishPath));

            // results in headers being dropped if sent to third parties, i.e.
            // headers should be present for the initial A -> A, but not for A -> B -> A
            Assert.assertEquals(
                    HEADER_VALUE, getHeader(mWebServer.getLastRequest(startPath), HEADER_NAME));
            Assert.assertEquals(
                    HEADER_VALUE,
                    getHeader(mWebServer.getLastRequest(redirectToFirstPartyPath), HEADER_NAME));
            Assert.assertNull(
                    getHeader(serverB.getLastRequest(redirectToThirdPartyPath), HEADER_NAME));
            Assert.assertNull(getHeader(mWebServer.getLastRequest(finishPath), HEADER_NAME));
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void extraHeaders_notSentToSubResources() throws Exception {
        String cssPath = "/style.css";
        mWebServer.setResponse(cssPath, "body { color: red; }", null);
        String htmlPath = "/index_with_style.html";
        String html =
                """
                <html>
                  <head>
                  <link rel="stylesheet" href="style.css">
                  </head>
                  <body>
                    Foo
                  </body>
                </html>
                """;
        String url = mWebServer.setResponse(htmlPath, html, null);

        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate with headers...
        AwNavigationParams params = paramsWithExtraHeader(url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));

        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(htmlPath));

        // ... doesn't send them to sub resources.
        Assert.assertNull(getHeader(mWebServer.getLastRequest(cssPath), HEADER_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void extraHeaders_notSentToIframes() throws Exception {
        String iframePath = "/iframe.html";
        mWebServer.setResponse(iframePath, "<html></html>", null);
        String htmlPath = "/index_with_iframe.html";
        String html =
                """
                <html>
                  <body>
                    <iframe src="iframe.html" />
                  </body>
                </html>
                """;
        String url = mWebServer.setResponse(htmlPath, html, null);

        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate with headers...
        AwNavigationParams params = paramsWithExtraHeader(url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));

        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(htmlPath));

        // ... doesn't send them to iframes.
        Assert.assertNull(getHeader(mWebServer.getLastRequest(iframePath), HEADER_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void extraHeaders_sentToShouldInterceptRequest() throws Exception {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Test that calling navigate with headers...
        AwNavigationParams params = paramsWithExtraHeader(mPage1Url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));

        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));

        // ... sends them to shouldInterceptRequest.
        AwWebResourceRequest request =
                mContentsClient.getShouldInterceptRequestHelper().getRequestsForUrl(mPage1Url);
        Assert.assertEquals(HEADER_VALUE, getHeader(request, HEADER_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate, WebViewSaveStateIncludeHeaders"})
    public void extraHeaders_saveRestoreState() throws Throwable {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Load the page with headers via navigate.
        AwNavigationParams params = paramsWithExtraHeader(mPage1Url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        // Create variables to restore state.
        TestAwContentsClient restoredStateContentsClient = new TestAwContentsClient();
        AwTestContainerView restoredStateTestView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(restoredStateContentsClient);

        // Save and restore state
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            Bundle bundle = new Bundle();
                            boolean result = mAwContents.saveState(bundle);
                            Assert.assertTrue(result);
                            result = restoredStateTestView.getAwContents().restoreState(bundle);
                            Assert.assertTrue(result);

                            NavigationEntrySimple[] navHistory =
                                    NavigationControllerUtil.getNavigationHistorySimple(
                                            restoredStateTestView.getAwContents().getWebContents());
                            Assert.assertEquals(1, navHistory.length);
                            NavigationEntrySimple restoredEntry = navHistory[0];
                            Assert.assertNotNull(restoredEntry);
                            Assert.assertEquals(mPage1Url, restoredEntry.getUrl());
                            Assert.assertEquals(
                                    params.extraHeaders, restoredEntry.getExtraHeaders());
                        });
    }

    // crbug:513520883 - Ensure that save/restore state doesn't leak headers
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate, WebViewSaveStateIncludeHeaders"})
    public void extraHeaders_saveRestoreState_redirects() throws Throwable {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // Load the page with headers via navigate.
        AwNavigationParams params = paramsWithExtraHeader(mPage1Url, HEADER_NAME, HEADER_VALUE);
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(params));
        mOnPageLoadFinished.waitForCallback(currentCallCount);

        // Create variables to restore state.
        TestAwContentsClient restoredStateContentsClient = new TestAwContentsClient();
        AwTestContainerView restoredStateTestView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(restoredStateContentsClient);
        CallbackHelper restoredStateOnPageFinishedHelper =
                restoredStateContentsClient.getOnPageFinishedHelper();
        int restoredStateCurrentCallCount = restoredStateOnPageFinishedHelper.getCallCount();

        // Disable caches so restore will trigger a network request.
        mActivityTestRule
                .getAwSettingsOnUiThread(restoredStateTestView.getAwContents())
                .setCacheMode(WebSettings.LOAD_NO_CACHE);

        try (TestWebServer serverB = TestWebServer.startAdditional()) {
            // Save and restore state
            InstrumentationRegistry.getInstrumentation()
                    .runOnMainSync(
                            () -> {
                                Bundle bundle = new Bundle();
                                boolean result = mAwContents.saveState(bundle);
                                Assert.assertTrue(result);

                                // Change the saved page to redirect to a cross origin url
                                String crossOriginUrl =
                                        serverB.setResponse(
                                                PAGE2_PATH, "Done!", Collections.emptyList());
                                mWebServer.setRedirect(PAGE1_PATH, crossOriginUrl);

                                result = restoredStateTestView.getAwContents().restoreState(bundle);
                                Assert.assertTrue(result);
                            });

            restoredStateOnPageFinishedHelper.waitForCallback(restoredStateCurrentCallCount);

            // Check for redirect
            Assert.assertEquals(1, serverB.getRequestCount(PAGE2_PATH));
            // Check restored network request excluded headers
            Assert.assertNull(
                    "Header leaked to cross-origin redirect target after saveState/restoreState",
                    getHeader(serverB.getLastRequest(PAGE2_PATH), HEADER_NAME));
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void secFetchHeaders() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));

        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));

        HTTPRequest request = mWebServer.getLastRequest(PAGE1_PATH);

        // Sec-Fetch-Site should be "none" because the navigation lacks an initiator (API
        // initiated).
        String secFetchSite = getHeader(request, "Sec-Fetch-Site");
        Assert.assertEquals("none", secFetchSite);

        // Sec-Fetch-User should be "?1" because TYPED transition implies user gesture.
        String secFetchUser = getHeader(request, "Sec-Fetch-User");
        Assert.assertEquals("?1", secFetchUser);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void navigateMultipleTimesNoCacheControl() throws TimeoutException {
        int currentCallCount = mOnPageLoadFinished.getCallCount();

        // First navigation:
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));
        HTTPRequest request1 = mWebServer.getLastRequest(PAGE1_PATH);
        Assert.assertNull(getHeader(request1, "Cache-Control"));

        // Second navigation to same URL:
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(mPage1Url));
        mOnPageLoadFinished.waitForCallback(currentCallCount + 1);
        Assert.assertEquals(2, mWebServer.getRequestCount(PAGE1_PATH));
        HTTPRequest request2 = mWebServer.getLastRequest(PAGE1_PATH);

        // Should NOT have Cache-Control header, unlike a regular reload.
        Assert.assertNull(getHeader(request2, "Cache-Control"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void sameSiteStrictCookies_CrossOriginNavigation() throws Exception {
        try (TestWebServer serverB = TestWebServer.startAdditional()) {
            String urlA = mPage1Url;
            String urlB = serverB.setResponse("/pageB.html", "<html><body>B</body></html>", null);

            final AwCookieManager cookieManager = new AwCookieManager();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        cookieManager.setAcceptCookie(true);
                        cookieManager.setCookie(urlA, "strict_cookie=val; SameSite=Strict");
                    });

            // Navigate to B:
            int currentCallCount = mOnPageLoadFinished.getCallCount();
            ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(urlB));
            mOnPageLoadFinished.waitForCallback(currentCallCount);
            Assert.assertEquals(1, serverB.getRequestCount("/pageB.html"));

            // Navigate to A using navigate() API.
            // This is cross-origin, but since it's via API it should send SameSite=Strict.
            ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(urlA));
            mOnPageLoadFinished.waitForCallback(currentCallCount + 1);
            Assert.assertEquals(1, mWebServer.getRequestCount(PAGE1_PATH));

            HTTPRequest request = mWebServer.getLastRequest(PAGE1_PATH);
            String cookieHeader = getHeader(request, "Cookie");
            Assert.assertNotNull(cookieHeader);
            Assert.assertTrue(cookieHeader.contains("strict_cookie=val"));
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewNavigate"})
    public void sameSiteCookies_CrossOriginRedirect() throws Exception {
        try (TestWebServer serverB = TestWebServer.startAdditional()) {
            String urlB = serverB.setResponse("/target.html", "<html><body>B</body></html>", null);
            String redirectUrlA = mWebServer.setRedirect("/redirect.html", urlB);

            final AwCookieManager cookieManager = new AwCookieManager();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        cookieManager.setAcceptCookie(true);
                        cookieManager.setCookie(urlB, "strict_cookie=val; SameSite=Strict");
                        cookieManager.setCookie(urlB, "lax_cookie=val; SameSite=Lax");
                    });

            // Navigate to redirectUrlA (Origin A) which redirects to urlB (Origin B)
            int currentCallCount = mOnPageLoadFinished.getCallCount();
            ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.navigate(redirectUrlA));
            mOnPageLoadFinished.waitForCallback(currentCallCount);

            Assert.assertEquals(1, mWebServer.getRequestCount("/redirect.html"));
            Assert.assertEquals(1, serverB.getRequestCount("/target.html"));

            HTTPRequest requestB = serverB.getLastRequest("/target.html");
            String cookieHeader = getHeader(requestB, "Cookie");

            Assert.assertNotNull(cookieHeader);

            // Lax cookie should be sent
            Assert.assertTrue(cookieHeader.contains("lax_cookie=val"));

            // Strict cookie SHOULD be sent because the navigation has no initiator (behaves like
            // address bar).
            Assert.assertTrue(cookieHeader.contains("strict_cookie=val"));
        }
    }

    @Nullable
    private static String getHeader(HTTPRequest request, String key) {
        HTTPHeader[] headers = request.getHeaders();

        String keyLowerCase = key.toLowerCase(Locale.US);
        for (HTTPHeader header : headers) {
            if (header.key.toLowerCase(Locale.US).equals(keyLowerCase)) {
                return header.value;
            }
        }
        return null;
    }

    @Nullable
    private static String getHeader(AwWebResourceRequest request, String key) {
        String keyLowerCase = key.toLowerCase(Locale.US);

        for (var header : request.getRequestHeaders().entrySet()) {
            if (header.getKey().toLowerCase(Locale.US).equals(keyLowerCase)) {
                return header.getValue();
            }
        }

        return null;
    }

    private static AwNavigationParams paramsWithReplaceCurrentEntry(String url) {
        boolean shouldReplaceCurrentEntry = true;
        return new AwNavigationParams(url, shouldReplaceCurrentEntry, null);
    }

    private static AwNavigationParams paramsWithExtraHeader(
            String url, String header, String headerValue) {
        boolean replace = false;
        Map<String, String> headers = Map.of(header, headerValue);
        return new AwNavigationParams(url, replace, headers);
    }
}
