// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test;

import android.annotation.SuppressLint;
import android.util.Pair;
import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

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
import org.chromium.android_webview.test.CookieManagerTest.IframeCookieSupplier;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.NullMarked;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.LinkedBlockingQueue;

/** Tests for the CookieManager with disabled cookie partitioning. */
@DoNotBatch(reason = "The cookie manager is global state")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@NullMarked
public class CookieManagerDisabledPartitioningTest extends AwParameterizedTest {

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwCookieManager mCookieManager;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public CookieManagerDisabledPartitioningTest(AwSettingsMutation param) {
        // Manually start start the browser process after cookie partitioning configuration
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsBrowserProcessStarted() {
                        return false;
                    }
                };
    }

    @SuppressLint("SetJavaScriptEnabled")
    @Before
    public void setUp() {
        // Disable partitioned cookies in runs of this test.
        AwCookieManager.disablePartitionedCookiesGlobal();
        mActivityTestRule.startBrowserProcess();

        mCookieManager = new AwCookieManager();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mAwContents.getSettings().setJavaScriptEnabled(true);
        Assert.assertNotNull(mCookieManager);
    }

    @After
    public void tearDown() {
        try {
            clearCookies();
        } catch (Throwable e) {
            throw new RuntimeException("Could not clear cookies.");
        }
    }

    /** Clears all cookies synchronously. */
    private void clearCookies() throws Throwable {
        CookieUtils.clearCookies(InstrumentationRegistry.getInstrumentation(), mCookieManager);
    }

    /**
     * Allow first-party cookies globally. This affects all {@link AwContents}, and this does not
     * affect the third-party cookie settings for any {@link AwContents}. This checks the return
     * value of {@link AwCookieManager#acceptCookie}.
     */
    private void allowFirstPartyCookies() {
        mCookieManager.setAcceptCookie(true);
        String msg = "acceptCookie() should return true after setAcceptCookie(true)";
        Assert.assertTrue(msg, mCookieManager.acceptCookie());
    }

    /**
     * Block all cookies for all {@link AwContents}. This blocks both first-party and third-party
     * cookies. This checks the return value of {@link AwCookieManager#acceptCookie}.
     */
    private void blockAllCookies() {
        mCookieManager.setAcceptCookie(false);
        String msg = "acceptCookie() should return false after setAcceptCookie(false)";
        Assert.assertFalse(msg, mCookieManager.acceptCookie());
    }

    /**
     * Allow third-party cookies for the given {@link AwContents}. This checks the return value of
     * {@link AwCookieManager#getAcceptThirdPartyCookies}. This also checks the value of {@link
     * AwCookieManager#acceptCookie}, since it doesn't make sense to turn on third-party cookies if
     * all cookies have been blocked.
     *
     * @param awContents the AwContents for which to allow third-party cookies.
     */
    private void allowThirdPartyCookies(AwContents awContents) {
        if (!mCookieManager.acceptCookie()) {
            throw new IllegalStateException(
                    "It doesn't make sense to allow third-party cookies if "
                            + "cookies have already been globally blocked.");
        }
        awContents.getSettings().setAcceptThirdPartyCookies(true);
        String msg =
                "getAcceptThirdPartyCookies() should return true after "
                        + "setAcceptThirdPartyCookies(true)";
        Assert.assertTrue(msg, awContents.getSettings().getAcceptThirdPartyCookies());
    }

    /**
     * Block third-party cookies for the given {@link AwContents}. This checks the return value of
     * {@link AwCookieManager#getAcceptThirdPartyCookies}.
     *
     * @param awContents the AwContents for which to block third-party cookies.
     */
    private void blockThirdPartyCookies(AwContents awContents) {
        awContents.getSettings().setAcceptThirdPartyCookies(false);
        String msg =
                "getAcceptThirdPartyCookies() should return false after "
                        + "setAcceptThirdPartyCookies(false)";
        Assert.assertFalse(msg, awContents.getSettings().getAcceptThirdPartyCookies());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void setDisabledPartitionedCookieWithCookieManager() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = "https://www.example.com";
            mCookieManager.setCookie(
                    url, "partitioned=foo;Path=/;Secure;Partitioned;SameSite=None");

            final String expected =
                    "partitioned=foo; domain=www.example.com; path=/; secure; samesite=none";
            List<String> cookieInfo = mCookieManager.getCookieInfo(url);
            Assert.assertNotNull(
                    "CookieManager should never return a null list of cookies", cookieInfo);
            Assert.assertFalse("cookieInfo should not be empty", cookieInfo.isEmpty());
            Assert.assertEquals(expected, cookieInfo.get(0));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testDisabledPartitionedNetCookies() throws Throwable {
        TestWebServer webServer = TestWebServer.startSsl();

        // This test suite relies on an image to force a network request that has cookies attached.
        // The AwParameterizedTest will disable this setting so force enabling it again so that
        // we can still test the rest of the parameterized test settings.
        mAwContents.getSettings().setImagesEnabled(true);

        try {
            String[] cookies = {
                "partitioned_cookie=foo; SameSite=None; Secure; Partitioned",
                "unpartitioned_cookie=bar; SameSite=None; Secure"
            };
            List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
            for (String cookie : cookies) {
                responseHeaders.add(Pair.create("Set-Cookie", cookie));
            }

            String iframeWithNetRequest =
                    """
                    <html>
                    <body>
                    <!-- Force a network request to happen from the iframe with a navigation so -->
                    <!-- that we can intercept it and see which cookies were attached -->
                    <img src="/path_to_intercept" >
                    </body>
                    </html>
                    """;
            String iframeUrl = webServer.setResponse("/", iframeWithNetRequest, responseHeaders);
            // We don't need this to do anything fancy, we just need the path to exist
            webServer.setResponse("/path_to_intercept", "hello", responseHeaders);

            String url =
                    CookieManagerTest.toThirdPartyUrl(
                            CookieManagerTest.makeIframeUrl(webServer, "/parent.html", iframeUrl));

            allowFirstPartyCookies();
            allowThirdPartyCookies(mAwContents);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(
                    "All cookies should be returned when 3PCs are enabled",
                    "partitioned_cookie=foo; unpartitioned_cookie=bar",
                    webServer.getLastRequest("/path_to_intercept").headerValue("Cookie"));

            blockThirdPartyCookies(mAwContents);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(
                    "Partitioned cookies should not be returned while CHIPS is disabled",
                    "",
                    webServer.getLastRequest("/path_to_intercept").headerValue("Cookie"));

            blockAllCookies();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(
                    "No cookies should be returned when all cookies are disabled",
                    "",
                    webServer.getLastRequest("/path_to_intercept").headerValue("Cookie"));

        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testDisabledPartitionedJSCookies() throws Throwable {
        String partitionedCookie = "partitioned-cookie=123";
        String unpartitionedCookie = "regular-cookie=456";

        TestWebServer webServer = TestWebServer.start();
        CookieManagerTest.addServerAssetLinks(webServer);

        try {
            // TODO(https://crbug.com/41496912): The WebView cookie manager API does not currently
            // provide access to
            // third party partitioned urls so we need to retrieve these cookies from the iframe
            // itself to validate this
            // behavior. We should refactor this test once support has been added to just use the
            // CookieManager.
            final LinkedBlockingQueue<String> javascriptInterfaceQueue =
                    new LinkedBlockingQueue<>();
            AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                    mAwContents,
                    new Object() {
                        @JavascriptInterface
                        public void report(String cookies) {
                            javascriptInterfaceQueue.add(cookies);
                        }
                    },
                    "cookieResults");

            IframeCookieSupplier iframeCookiesSupplier =
                    () -> {
                        String iframeUrl =
                                CookieManagerTest.toThirdPartyUrl(
                                        CookieManagerTest.makeCookieScriptResultsUrl(
                                                webServer,
                                                "/iframe.html",
                                                partitionedCookie
                                                        + "; Secure; Path=/; SameSite=None;"
                                                        + " Partitioned;",
                                                unpartitionedCookie
                                                        + "; Secure; Path=/; SameSite=None;"));

                        String url =
                                CookieManagerTest.makeIframeUrl(
                                        webServer, "/parent.html", iframeUrl);

                        try {
                            mActivityTestRule.loadUrlSync(
                                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

                            return AwActivityTestRule.waitForNextQueueElement(
                                    javascriptInterfaceQueue);
                        } catch (Exception e) {
                            // Failed to retrieve so we can treat this as "no-data" - this in turn
                            // will fail equality checks
                            return "Failed to retrieve data";
                        }
                    };

            allowFirstPartyCookies();
            blockThirdPartyCookies(mAwContents);
            Assert.assertEquals(
                    "Partitioned cookies should not be returned while CHIPS is disabled",
                    "",
                    iframeCookiesSupplier.get());

            allowThirdPartyCookies(mAwContents);
            Assert.assertEquals(
                    "All cookies should be returned when 3PCs are enabled",
                    partitionedCookie + "; " + unpartitionedCookie,
                    iframeCookiesSupplier.get());

            blockAllCookies();
            Assert.assertEquals(
                    "No cookies should ever be returned if all cookies are disabled",
                    "",
                    iframeCookiesSupplier.get());
        } finally {
            webServer.shutdown();
        }
    }
}
