// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.MatcherAssert.assertThat;

import android.util.Pair;
import android.webkit.JavascriptInterface;

import androidx.annotation.IntDef;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

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
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.android_webview.test.util.CookieUtils.TestCallback;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.TimeZone;
import java.util.concurrent.LinkedBlockingQueue;

/** Tests for the CookieManager. */
@DoNotBatch(reason = "The cookie manager is global state")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class CookieManagerTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    @IntDef({
        CookieLifetime.OUTLIVE_THE_TEST_SEC,
        CookieLifetime.EXPIRE_DURING_TEST_SEC,
        CookieLifetime.ALREADY_EXPIRED_SEC
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface CookieLifetime {
        /** Longer than the limit of tests, so cookies will not expire during the test. */
        final int OUTLIVE_THE_TEST_SEC = 10 * 60; // 10 minutes

        /**
         * Shorter than the limit of tests, so cookies may expire during the test. Be sure to wait
         * at least this duration after <b>setting</b> the cookie (ex. via {@link
         * AwCookieManager#setCookie(String)}).
         */
        final int EXPIRE_DURING_TEST_SEC = 1;

        /** Guarantees the cookie is expired, immediately when set. */
        final int ALREADY_EXPIRED_SEC = -1;
    }

    private AwCookieManager mCookieManager;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private static final String SECURE_COOKIE_HISTOGRAM_NAME = "Android.WebView.SecureCookieAction";

    private static final String ASSET_STATEMENT_TEMPLATE =
            """
                [{
                        "relation": ["delegate_permission/common.handle_all_urls"],
                        "target": {
                                "namespace": "android_app",
                                "package_name": "%s",
                                "sha256_cert_fingerprints": ["%s"]
                        }
                }]
        """;

    public CookieManagerTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
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

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAcceptCookie_default() {
        Assert.assertTrue(
                "Expected CookieManager to accept cookies by default",
                mCookieManager.acceptCookie());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAcceptCookie_setterGetterFunctionality() {
        mCookieManager.setAcceptCookie(false);
        Assert.assertFalse(
                "Expected #acceptCookie() to return false after setAcceptCookie(false)",
                mCookieManager.acceptCookie());
        mCookieManager.setAcceptCookie(true);
        Assert.assertTrue(
                "Expected #acceptCookie() to return true after setAcceptCookie(true)",
                mCookieManager.acceptCookie());
    }

    /**
     * @param acceptCookieValue the value passed into {@link AwCookieManager#setAcceptCookie}.
     * @param cookieSuffix a suffix to use for the cookie name, should be unique to avoid
     *        side-effects from other tests.
     */
    private void testAcceptCookieHelper(boolean acceptCookieValue, String cookieSuffix)
            throws Throwable {
        mCookieManager.setAcceptCookie(acceptCookieValue);

        // Using SSL server here since CookieStore API requires a secure schema.
        TestWebServer webServer = TestWebServer.startSsl();
        try {
            String path = "/cookie_test.html";
            String responseStr =
                    "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
            String url = webServer.setResponse(path, responseStr, null);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            final String jsCookieName = "js-test" + cookieSuffix;
            setCookieWithDocumentCookieAPI(jsCookieName, "value");
            if (acceptCookieValue) {
                waitForCookie(url);
                assertHasCookies(url);
                validateCookies(url, jsCookieName);
            } else {
                assertNoCookies(url);
            }

            final String cookieStoreCookieName = "cookiestore-test" + cookieSuffix;
            setCookieWithCookieStoreAPI(cookieStoreCookieName, "value");
            if (acceptCookieValue) {
                waitForCookie(url);
                assertHasCookies(url);
                validateCookies(url, jsCookieName, cookieStoreCookieName);
            } else {
                assertNoCookies(url);
            }

            final List<Pair<String, String>> responseHeaders =
                    new ArrayList<Pair<String, String>>();
            final String headerCookieName = "header-test" + cookieSuffix;
            responseHeaders.add(
                    Pair.create("Set-Cookie", headerCookieName + "=header-value path=" + path));
            url = webServer.setResponse(path, responseStr, responseHeaders);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            if (acceptCookieValue) {
                waitForCookie(url);
                assertHasCookies(url);
                validateCookies(url, jsCookieName, cookieStoreCookieName, headerCookieName);
            } else {
                assertNoCookies(url);
            }
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAcceptCookie_falseWontSetCookies() throws Throwable {
        testAcceptCookieHelper(false, "-disabled");
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAcceptCookie_trueWillSetCookies() throws Throwable {
        testAcceptCookieHelper(true, "-enabled");
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAcceptCookie_falseDoNotSendCookies() throws Throwable {
        blockAllCookies();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        EmbeddedTestServer embeddedTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String url = embeddedTestServer.getURL("/echoheader?Cookie");
        String cookieName = "java-test";
        mCookieManager.setCookie(url, cookieName + "=should-not-work");
        // Setting cookies should still affect the CookieManager itself
        assertHasCookies(url);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        String jsValue = getCookieWithJavaScript(cookieName);
        String message =
                "WebView should not expose cookies to JavaScript (with setAcceptCookie "
                        + "disabled)";
        Assert.assertEquals(message, "\"\"", jsValue);
        final String cookieHeader =
                mActivityTestRule.getJavaScriptResultBodyTextContent(mAwContents, mContentsClient);
        message =
                "WebView should not expose cookies via the Cookie header (with "
                        + "setAcceptCookie disabled)";
        Assert.assertEquals(message, "None", cookieHeader);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testEmbedderCanSeeRestrictedCookies() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            // Set a cookie with the httponly flag, one with samesite=Strict, and one with
            // samesite=Lax, to ensure that they are all visible to CookieManager in the app.
            String[] cookies = {
                "httponly=foo1; HttpOnly",
                "strictsamesite=foo2; SameSite=Strict",
                "laxsamesite=foo3; SameSite=Lax"
            };
            List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
            for (String cookie : cookies) {
                responseHeaders.add(Pair.create("Set-Cookie", cookie));
            }
            String url = webServer.setResponse("/", "test", responseHeaders);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            waitForCookie(url);
            assertHasCookies(url);
            validateCookies(url, "httponly", "strictsamesite", "laxsamesite");
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testEmbedderCanSeePartitionedCookies() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            // Set a partitioned cookie and an unpartitioned cookie to ensure that they are all
            // visible to CookieManager in the app.
            String[] cookies = {
                "partitioned_cookie=foo; SameSite=None; Secure; Partitioned",
                "unpartitioned_cookie=bar; SameSite=None; Secure"
            };
            List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
            for (String cookie : cookies) {
                responseHeaders.add(Pair.create("Set-Cookie", cookie));
            }
            String url = webServer.setResponse("/", "test", responseHeaders);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            waitForCookie(url);
            assertHasCookies(url);
            validateCookies(url, "partitioned_cookie", "unpartitioned_cookie");
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void setPartitionedCookieWithCookieManager() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = "https://www.example.com";
            mCookieManager.setCookie(
                    url, "partitioned=foo;Path=/;Secure;Partitioned;SameSite=None");

            final String expected =
                    "partitioned=foo; domain=www.example.com; path=/; "
                            + "secure; partitioned; samesite=none";
            List<String> cookieInfo = mCookieManager.getCookieInfo(url);
            Assert.assertNotNull(cookieInfo);
            Assert.assertFalse("cookieInfo should not be empty", cookieInfo.isEmpty());
            Assert.assertEquals(expected, cookieInfo.get(0));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("disable-partitioned-cookies")
    public void setDisabledPartitionedCookieWithCookieManager() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = "https://www.example.com";
            mCookieManager.setCookie(
                    url, "partitioned=foo;Path=/;Secure;Partitioned;SameSite=None");

            final String expected =
                    "partitioned=foo; domain=www.example.com; path=/; secure; samesite=none";
            List<String> cookieInfo = mCookieManager.getCookieInfo(url);
            Assert.assertNotNull(cookieInfo);
            Assert.assertFalse("cookieInfo should not be empty", cookieInfo.isEmpty());
            Assert.assertEquals(expected, cookieInfo.get(0));
        } finally {
            webServer.shutdown();
        }
    }

    private void setCookieWithDocumentCookieAPI(final String name, final String value)
            throws Throwable {
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "var expirationDate = new Date();"
                        + "expirationDate.setDate(expirationDate.getDate() + 5);"
                        + "document.cookie='"
                        + name
                        + "="
                        + value
                        + "; expires=' + expirationDate.toUTCString();");
    }

    private void setCookieWithCookieStoreAPI(final String name, final String value)
            throws Throwable {
        JavaScriptUtils.runJavascriptWithAsyncResult(
                mAwContents.getWebContents(),
                "async function doSet() {"
                        + makeCookieStoreSetFragment(
                                "'" + name + "'",
                                "'" + value + "'",
                                "window.domAutomationController.send(true);")
                        + "}\n"
                        + "doSet()");
    }

    private String getCookieWithJavaScript(final String name) throws Throwable {
        return JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "document.cookie");
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testRemoveAllCookies() {
        final String cookieUrl = "http://www.example.com";
        mCookieManager.setCookie(cookieUrl, "name=test");
        assertHasCookies(cookieUrl);
        mCookieManager.removeAllCookies();
        assertNoCookies();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add({AwSwitches.WEBVIEW_FORCE_DISABLE3PCS})
    public void testForceDisable3pcs() {
        mAwContents.getSettings().setAcceptThirdPartyCookies(true);
        Assert.assertFalse(
                "Third party cookies should stay disabled if they were forced disabled",
                mAwContents.getSettings().getAcceptThirdPartyCookies());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testRemoveSessionCookies() {
        final String url = "http://www.example.com";
        final String sessionCookie = "cookie1=peter";
        final String normalCookie = "cookie2=sue";

        mCookieManager.setCookie(url, sessionCookie);
        mCookieManager.setCookie(
                url, makeExpiringCookie(normalCookie, CookieLifetime.OUTLIVE_THE_TEST_SEC));

        mCookieManager.removeSessionCookies();

        String allCookies = mCookieManager.getCookie(url);
        Assert.assertFalse(allCookies.contains(sessionCookie));
        Assert.assertTrue(allCookies.contains(normalCookie));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testGetCookieInfo_singleCookie() {
        final String url = "http://www.example.com";
        final String formattedDate = getHttpCookieExpiryDate();

        final String cookieString =
                "cookie=test; Domain=.example.com; Path=/; Expires=" + formattedDate;
        final String expected =
                "cookie=test; domain=.example.com; path=/; expires=" + formattedDate;

        allowThirdPartyCookies(mAwContents);
        mCookieManager.setCookie(url, cookieString);
        List<String> cookieInfo = mCookieManager.getCookieInfo(url);

        Assert.assertNotNull(cookieInfo);
        Assert.assertEquals(expected, cookieInfo.get(0));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testGetCookieInfo_twoCookies() {
        final String url = "http://www.example.com";
        final String formattedDate = getHttpCookieExpiryDate();

        final String cookie1String =
                "cookie1=test1; Domain=example.com; Path=/; Expires=" + formattedDate;
        final String cookie2String =
                "cookie2=test2; SameSite=Lax; HttpOnly; Expires=" + formattedDate;
        final String expected1 =
                "cookie1=test1; domain=.example.com; path=/; expires=" + formattedDate;
        final String expected2 =
                "cookie2=test2; domain=www.example.com; path=/; expires="
                        + formattedDate
                        + "; httponly; samesite=lax";

        allowThirdPartyCookies(mAwContents);
        mCookieManager.setCookie(url, cookie1String);
        mCookieManager.setCookie(url, cookie2String);
        List<String> cookieInfo = mCookieManager.getCookieInfo(url);

        Assert.assertNotNull(cookieInfo);
        Assert.assertEquals(expected1, cookieInfo.get(0));
        Assert.assertEquals(expected2, cookieInfo.get(1));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testGetCookieInfo_emptyCookie() {
        final String url = "http://www.example.com";

        final String cookieString = "cookie1=test1";
        final String expected = "cookie1=test1; domain=www.example.com; path=/";

        allowThirdPartyCookies(mAwContents);
        mCookieManager.setCookie(url, cookieString);
        List<String> cookieInfo = mCookieManager.getCookieInfo(url);

        Assert.assertNotNull(cookieInfo);
        Assert.assertEquals(expected, cookieInfo.get(0));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookie() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SECURE_COOKIE_HISTOGRAM_NAME, /* kNotASecureCookie= */ 3);
        String url = "http://www.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(url, cookie);
        assertCookieEquals(cookie, url);
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookieSameSite() {
        String url = "http://www.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(url, cookie + "; SameSite=Lax");
        assertCookieEquals(cookie, url);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookieWithDomainForUrl() {
        // If the app passes ".www.example.com" or "http://.www.example.com", the glue layer "fixes"
        // this to "http:///.www.example.com"
        String url = "http:///.www.example.com";
        String sameSubdomainUrl = "http://a.www.example.com";
        String differentSubdomainUrl = "http://different.sub.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(url, cookie);
        assertCookieEquals(cookie, sameSubdomainUrl);
        assertNoCookies(differentSubdomainUrl);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookieWithDomainForUrlAndExistingDomainAttribute() {
        String url = "http:///.www.example.com";
        String differentSubdomainUrl = "http://different.sub.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(url, cookie + "; doMaIN \t  =.example.com");
        assertCookieEquals(cookie, url);
        assertCookieEquals(cookie, differentSubdomainUrl);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookieWithDomainForUrlWithTrailingSemicolonInCookie() {
        String url = "http:///.www.example.com";
        String sameSubdomainUrl = "http://a.www.example.com";
        String differentSubdomainUrl = "http://different.sub.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(url, cookie + ";");
        assertCookieEquals(cookie, sameSubdomainUrl);
        assertNoCookies(differentSubdomainUrl);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetSecureCookieForHttpUrlNotTargetingAndroidR() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SECURE_COOKIE_HISTOGRAM_NAME, /* kFixedUp= */ 4);

        mCookieManager.setWorkaroundHttpSecureCookiesForTesting(true);
        String url = "http://www.example.com";
        String secureUrl = "https://www.example.com";
        String cookie = "name=test";
        boolean success = setCookieOnUiThreadSync(url, cookie + ";secure");

        Assert.assertTrue("Setting the cookie should succeed", success);
        assertCookieEquals(cookie, secureUrl);
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetSecureCookieForHttpUrlTargetingAndroidR() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SECURE_COOKIE_HISTOGRAM_NAME, /* kDisallowedAndroidR= */ 5);

        mCookieManager.setWorkaroundHttpSecureCookiesForTesting(false);
        String url = "http://www.example.com";
        String secureUrl = "https://www.example.com";
        String cookie = "name=test";
        boolean success = setCookieOnUiThreadSync(url, cookie + ";secure");

        Assert.assertFalse("Setting the cookie should fail", success);
        assertNoCookies(url);
        assertNoCookies(secureUrl);
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetSecureCookieForHttpsUrl() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SECURE_COOKIE_HISTOGRAM_NAME, /* kAlreadySecureScheme= */ 1);

        String secureUrl = "https://www.example.com";
        String cookie = "name=test";
        mCookieManager.setCookie(secureUrl, cookie + ";secure");
        assertCookieEquals(cookie, secureUrl);
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testHasCookie() {
        Assert.assertFalse(mCookieManager.hasCookies());
        mCookieManager.setCookie("http://www.example.com", "name=test");
        Assert.assertTrue(mCookieManager.hasCookies());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookieCallback_goodUrl() throws Throwable {
        final String url = "http://www.example.com";
        final String cookie = "name=test";

        final TestCallback<Boolean> callback = new TestCallback<Boolean>();
        int callCount = callback.getOnResultHelper().getCallCount();

        setCookieOnUiThread(url, cookie, callback);
        callback.getOnResultHelper().waitForCallback(callCount);
        Assert.assertTrue(callback.getValue());
        assertCookieEquals(cookie, url);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookieCallback_badUrl() throws Throwable {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SECURE_COOKIE_HISTOGRAM_NAME, /* kInvalidUrl= */ 0);
        final String cookie = "name=test";
        final String brokenUrl = "foo";

        final TestCallback<Boolean> callback = new TestCallback<Boolean>();
        int callCount = callback.getOnResultHelper().getCallCount();

        setCookieOnUiThread(brokenUrl, cookie, callback);
        callback.getOnResultHelper().waitForCallback(callCount);
        Assert.assertFalse("Cookie should not be set for bad URLs", callback.getValue());
        assertNoCookies(brokenUrl);
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testSetCookieNullCallback() {
        allowFirstPartyCookies();

        final String url = "http://www.example.com";
        final String cookie = "name=test";

        mCookieManager.setCookie(url, cookie, null);

        AwActivityTestRule.pollInstrumentationThread(() -> mCookieManager.hasCookies());
        assertCookieEquals(cookie, url);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testRemoveAllCookiesCallback() throws Throwable {
        TestCallback<Boolean> callback = new TestCallback<Boolean>();
        int callCount = callback.getOnResultHelper().getCallCount();

        mCookieManager.setCookie("http://www.example.com", "name=test");

        // When we remove all cookies the first time some cookies are removed.
        removeAllCookiesOnUiThread(callback);
        callback.getOnResultHelper().waitForCallback(callCount);
        Assert.assertTrue(callback.getValue());
        Assert.assertFalse(mCookieManager.hasCookies());

        callCount = callback.getOnResultHelper().getCallCount();

        // The second time none are removed.
        removeAllCookiesOnUiThread(callback);
        callback.getOnResultHelper().waitForCallback(callCount);
        Assert.assertFalse(callback.getValue());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testRemoveAllCookiesNullCallback() {
        mCookieManager.setCookie("http://www.example.com", "name=test");

        mCookieManager.removeAllCookies(null);

        // Eventually the cookies are removed.
        AwActivityTestRule.pollInstrumentationThread(() -> !mCookieManager.hasCookies());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testRemoveSessionCookiesCallback() throws Throwable {
        final String url = "http://www.example.com";
        final String sessionCookie = "cookie1=peter";
        final String normalCookie = "cookie2=sue";

        TestCallback<Boolean> callback = new TestCallback<Boolean>();
        int callCount = callback.getOnResultHelper().getCallCount();

        mCookieManager.setCookie(url, sessionCookie);
        mCookieManager.setCookie(
                url, makeExpiringCookie(normalCookie, CookieLifetime.OUTLIVE_THE_TEST_SEC));

        // When there is a session cookie then it is removed.
        removeSessionCookiesOnUiThread(callback);
        callback.getOnResultHelper().waitForCallback(callCount);
        Assert.assertTrue(callback.getValue());
        String allCookies = mCookieManager.getCookie(url);
        Assert.assertTrue(!allCookies.contains(sessionCookie));
        Assert.assertTrue(allCookies.contains(normalCookie));

        callCount = callback.getOnResultHelper().getCallCount();

        // If there are no session cookies then none are removed.
        removeSessionCookiesOnUiThread(callback);
        callback.getOnResultHelper().waitForCallback(callCount);
        Assert.assertFalse(callback.getValue());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testRemoveSessionCookiesNullCallback() {
        final String url = "http://www.example.com";
        final String sessionCookie = "cookie1=peter";
        final String normalCookie = "cookie2=sue";

        mCookieManager.setCookie(url, sessionCookie);
        mCookieManager.setCookie(
                url, makeExpiringCookie(normalCookie, CookieLifetime.OUTLIVE_THE_TEST_SEC));
        String allCookies = mCookieManager.getCookie(url);
        Assert.assertTrue(allCookies.contains(sessionCookie));
        Assert.assertTrue(allCookies.contains(normalCookie));

        mCookieManager.removeSessionCookies(null);

        // Eventually the session cookie is removed.
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    String c = mCookieManager.getCookie(url);
                    return !c.contains(sessionCookie) && c.contains(normalCookie);
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testExpiredCookiesAreNotSet() {
        final String url = "http://www.example.com";
        final String cookie = "cookie1=peter";

        mCookieManager.setCookie(
                url, makeExpiringCookie(cookie, CookieLifetime.ALREADY_EXPIRED_SEC));
        assertNoCookies(url);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testCookiesExpire() {
        final String url = "http://www.example.com";
        final String cookie = "cookie1=peter";

        mCookieManager.setCookie(
                url, makeExpiringCookie(cookie, CookieLifetime.EXPIRE_DURING_TEST_SEC));

        Assert.assertTrue("Cookie should exist before expiration", mCookieManager.hasCookies());

        // But eventually expires:
        AwActivityTestRule.pollInstrumentationThread(() -> !mCookieManager.hasCookies());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testCookieExpiration() {
        final String url = "http://www.example.com";
        final String sessionCookie = "cookie1=peter";
        final String longCookie = "cookie2=marc";

        mCookieManager.setCookie(url, sessionCookie);
        mCookieManager.setCookie(
                url, makeExpiringCookie(longCookie, CookieLifetime.OUTLIVE_THE_TEST_SEC));

        String allCookies = mCookieManager.getCookie(url);
        Assert.assertTrue(allCookies.contains(sessionCookie));
        Assert.assertTrue(allCookies.contains(longCookie));

        // Removing expired cookies doesn't have an observable effect but since people will still
        // be calling it for a while it shouldn't break anything either.
        mCookieManager.removeExpiredCookies();

        allCookies = mCookieManager.getCookie(url);
        Assert.assertTrue(allCookies.contains(sessionCookie));
        Assert.assertTrue(allCookies.contains(longCookie));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyCookie() throws Throwable {
        // In theory we need two servers to test this, one server ('the first
        // party') which returns a response with a link to a second server ('the third party') at
        // different origin. This second server attempts to set a cookie which should fail if
        // AcceptThirdPartyCookie() is false. Strictly according to the letter of RFC6454 it should
        // be possible to set this situation up with two TestServers on different ports (these count
        // as having different origins) but Chrome is not strict about this and does not check the
        // port. Instead we cheat making some of the urls come from localhost and some from
        // 127.0.0.1 which count (both in theory and pratice) as having different origins.
        TestWebServer webServer = TestWebServer.start();
        try {
            allowFirstPartyCookies();
            blockThirdPartyCookies(mAwContents);

            // We can't set third party cookies.
            // First on the third party server we create a url which tries to set a cookie.
            String cookieUrl =
                    toThirdPartyUrl(makeCookieUrl(webServer, "/cookie_1.js", "test1", "value1"));
            // Then we create a url on the first party server which links to the first url.
            String url = makeScriptLinkUrl(webServer, "/content_1.html", cookieUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            assertNoCookies(cookieUrl);

            allowThirdPartyCookies(mAwContents);

            // We can set third party cookies.
            cookieUrl =
                    toThirdPartyUrl(makeCookieUrl(webServer, "/cookie_2.js", "test2", "value2"));
            url = makeScriptLinkUrl(webServer, "/content_2.html", cookieUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            waitForCookie(cookieUrl);
            assertHasCookies(cookieUrl);
            validateCookies(cookieUrl, "test2");
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1323719")
    @Feature({"AndroidWebView", "Privacy"})
    public void testCookieStoreListener() throws Throwable {
        TestWebServer webServer = TestWebServer.startSsl();
        try {
            allowFirstPartyCookies();

            String url = makeCookieScriptUrl(webServer, "/cookie_1.html", "test1", "value1");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

            // Add a listener...
            JSUtils.executeJavaScriptAndWaitForResult(
                    InstrumentationRegistry.getInstrumentation(),
                    mAwContents,
                    mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                    "window.events = [];"
                            + "cookieStore.addEventListener('change', (event) => {"
                            + "  for (let d of event.deleted)"
                            + "    window.events.push({'del': d.name});"
                            + "  for (let c of event.changed)"
                            + "    window.events.push({'change': c.name});"
                            + "})");

            // Clearing all cookies with cookies disabled shouldn't report anything.
            blockAllCookies();
            clearCookies();

            // Re-enable cookies, set one.
            allowFirstPartyCookies();
            setCookieWithDocumentCookieAPI("test2", "value2");

            // Look up the result. Should see the second set, but not the
            // delete, based on whether cookie access was permitted or not
            // at the time.
            String reported =
                    JSUtils.executeJavaScriptAndWaitForResult(
                            InstrumentationRegistry.getInstrumentation(),
                            mAwContents,
                            mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                            "window.events");
            Assert.assertEquals("[{\"change\":\"test2\"}]", reported);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyCookie_redirectFromThirdPartyToFirst() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            allowFirstPartyCookies();
            blockThirdPartyCookies(mAwContents);

            // Load a page with a third-party resource. The resource URL redirects to a new URL
            // (which is first-party relative to the main frame). The final resource URL should
            // successfully set its cookies (because it's first-party).
            String resourcePath = "/cookie_1.js";
            String firstPartyCookieUrl = makeCookieUrl(webServer, resourcePath, "test1", "value1");
            String thirdPartyRedirectUrl =
                    toThirdPartyUrl(
                            webServer.setRedirect("/redirect_cookie_1.js", firstPartyCookieUrl));
            String contentUrl =
                    makeScriptLinkUrl(webServer, "/content_1.html", thirdPartyRedirectUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), contentUrl);
            assertCookieEquals("test1=value1", firstPartyCookieUrl);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyCookie_redirectFromFirstPartyToThird() throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            allowFirstPartyCookies();
            blockThirdPartyCookies(mAwContents);

            // Load a page with a first-party resource. The resource URL redirects to a new URL
            // (which is third-party relative to the main frame). The final resource URL should be
            // unable to set cookies (because it's third-party).
            String resourcePath = "/cookie_2.js";
            String thirdPartyCookieUrl =
                    toThirdPartyUrl(makeCookieUrl(webServer, resourcePath, "test2", "value2"));
            String firstPartyRedirectUrl =
                    webServer.setRedirect("/redirect_cookie_2.js", thirdPartyCookieUrl);
            String contentUrl =
                    makeScriptLinkUrl(webServer, "/content_2.html", firstPartyRedirectUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), contentUrl);
            assertNoCookies(thirdPartyCookieUrl);
        } finally {
            webServer.shutdown();
        }
    }

    private String webSocketCookieHelper(
            boolean shouldUseThirdPartyUrl, String cookieKey, String cookieValue) throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            // |cookieUrl| sets a cookie on response.
            String cookieUrl =
                    makeCookieWebSocketUrl(webServer, "/cookie_1", cookieKey, cookieValue);
            if (shouldUseThirdPartyUrl) {
                // Let |cookieUrl| be a third-party url to test third-party cookies.
                cookieUrl = toThirdPartyUrl(cookieUrl);
            }
            // This html file includes a script establishing a WebSocket connection to |cookieUrl|.
            String url = makeWebSocketScriptUrl(webServer, "/content_1.html", cookieUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            final String connecting = "0"; // WebSocket.CONNECTING
            final String closed = "3"; // WebSocket.CLOSED
            String readyState = connecting;
            WebContents webContents = mAwContents.getWebContents();
            while (!readyState.equals(closed)) {
                readyState =
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                webContents, "ws.readyState");
            }
            Assert.assertEquals(
                    "true",
                    JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "hasOpened"));
            return mCookieManager.getCookie(cookieUrl);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testCookieForWebSocketHandshake_thirdParty_enabled() throws Throwable {
        allowFirstPartyCookies();
        allowThirdPartyCookies(mAwContents);
        String cookieKey = "test1";
        String cookieValue = "value1";
        Assert.assertEquals(
                cookieKey + "=" + cookieValue,
                webSocketCookieHelper(/* shouldUseThirdPartyUrl= */ true, cookieKey, cookieValue));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testCookieForWebSocketHandshake_thirdParty_disabled() throws Throwable {
        allowFirstPartyCookies();
        blockThirdPartyCookies(mAwContents);
        String cookieKey = "test1";
        String cookieValue = "value1";
        Assert.assertNull(
                "Should not set 3P cookie when 3P cookie settings are disabled",
                webSocketCookieHelper(/* shouldUseThirdPartyUrl= */ true, cookieKey, cookieValue));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testCookieForWebSocketHandshake_firstParty_enabled() throws Throwable {
        allowFirstPartyCookies();
        allowThirdPartyCookies(mAwContents);
        String cookieKey = "test1";
        String cookieValue = "value1";
        Assert.assertEquals(
                cookieKey + "=" + cookieValue,
                webSocketCookieHelper(/* shouldUseThirdPartyUrl= */ false, cookieKey, cookieValue));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testCookieForWebSocketHandshake_firstParty_disabled() throws Throwable {
        blockAllCookies();
        String cookieKey = "test1";
        String cookieValue = "value1";
        Assert.assertNull(
                "Should not set 1P cookie when 1P cookie settings are disabled",
                webSocketCookieHelper(/* shouldUseThirdPartyUrl= */ false, cookieKey, cookieValue));
    }

    // Tests websockets inside third party frame --- the socket is first party to the frame,
    // but the frame itself is third-party to the main document.
    private String webSocketThirdPartyFrameCookieHelper(String cookieKey, String cookieValue)
            throws Throwable {
        TestWebServer webServer = TestWebServer.startSsl();
        try {
            // |cookieUrl| sets a cookie on response.
            String cookieUrl =
                    toThirdPartyUrl(
                            makeCookieWebSocketUrl(webServer, "/cookie_1", cookieKey, cookieValue));

            // This html file includes a script establishing a WebSocket connection to |cookieUrl|,
            // with wrappers to talk to parent frame.
            String childFrameUrl =
                    toThirdPartyUrl(
                            makeFrameableWebSocketScriptUrl(
                                    webServer, "/frame_with_websocket.html", cookieUrl));

            // Wrap that in an iframe on the default domain to make it be third-party, and load it.
            String url = makeIframeUrl(webServer, "/parent.html", childFrameUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

            // Make sure websocket has completed.
            JavaScriptUtils.runJavascriptWithAsyncResult(
                    mAwContents.getWebContents(), "callIframe()");

            return mCookieManager.getCookie(cookieUrl);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyIframeCookieForWebSocketHandshake_thirdParty_disabled()
            throws Throwable {
        allowFirstPartyCookies();
        blockThirdPartyCookies(mAwContents);

        String cookieKey = "test3PFrame";
        String cookieValue = "value3PFrame";

        Assert.assertNull(
                "Should not set cookie in 3P frame when 3P cookies are disabled",
                webSocketThirdPartyFrameCookieHelper(cookieKey, cookieValue));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyIframeCookieForWebSocketHandshake_thirdParty_enabled()
            throws Throwable {
        allowFirstPartyCookies();
        allowThirdPartyCookies(mAwContents);

        String cookieKey = "test3PFrame";
        String cookieValue = "value3PFrame";

        Assert.assertEquals(
                cookieKey + "=" + cookieValue,
                webSocketThirdPartyFrameCookieHelper(cookieKey, cookieValue));
    }

    /**
     * Creates a response on the TestWebServer which attempts to set a cookie when fetched.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/cookie_test.html")
     * @param  key the key of the cookie
     * @param  value the value of the cookie
     * @return  the url which gets the response
     */
    private String makeCookieUrl(TestWebServer webServer, String path, String key, String value) {
        String response = "";
        List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
        responseHeaders.add(Pair.create("Set-Cookie", key + "=" + value + "; path=" + path));
        return webServer.setResponse(path, response, responseHeaders);
    }

    /**
     * Creates a response on the TestWebServer which attempts to set a cookie when establishing a
     * WebSocket connection.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/cookie_test.html")
     * @param  key the key of the cookie
     * @param  value the value of the cookie
     * @return  the url which gets the response
     */
    private String makeCookieWebSocketUrl(
            TestWebServer webServer, String path, String key, String value) {
        List<Pair<String, String>> responseHeaders = new ArrayList<Pair<String, String>>();
        responseHeaders.add(Pair.create("Set-Cookie", key + "=" + value + "; path=" + path));
        return webServer.setResponseForWebSocket(path, responseHeaders);
    }

    /**
     * Creates a response on the TestWebServer which contains a script tag with an external src.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/my_thing_with_script.html")
     * @param  url the url which which should appear as the src of the script tag.
     * @return  the url which gets the response
     */
    private String makeScriptLinkUrl(TestWebServer webServer, String path, String url) {
        String responseStr =
                "<html><head><title>Content!</title></head>"
                        + "<body><script src="
                        + url
                        + "></script></body></html>";
        return webServer.setResponse(path, responseStr, null);
    }

    /**
     * Creates a response on the TestWebServer which contains a script establishing a WebSocket
     * connection.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/my_thing_with_script.html")
     * @param  url the url which which should appear as the src of the script tag.
     * @return  the url which gets the response
     */
    private String makeWebSocketScriptUrl(TestWebServer webServer, String path, String url) {
        String responseStr =
                "<html><head><title>Content!</title></head>"
                        + "<body><script>\n"
                        + "let ws = new WebSocket('"
                        + url.replaceAll("^http", "ws")
                        + "');\n"
                        + "let hasOpened = false;\n"
                        + "ws.onopen = () => hasOpened = true;\n"
                        + "</script></body></html>";
        return webServer.setResponse(path, responseStr, null);
    }

    /**
     * Creates a response on the TestWebServer which contains a script establishing a WebSocket
     * connection in response to a postMessage, and replies when established.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/my_thing_with_script.html")
     * @param  url the url to pass to websocket.
     * @return  the url which gets the response
     */
    private String makeFrameableWebSocketScriptUrl(
            TestWebServer webServer, String path, String url) {
        String responseStr =
                "<html><head><title>Content!</title></head>"
                        + "<body><script>\n"
                        + "window.onmessage = function(ev) {"
                        + "  let ws = new WebSocket('"
                        + url.replaceAll("^http", "ws")
                        + "');\n"
                        + "  ws.onopen = () => ev.source.postMessage(true, '*');\n"
                        + "}\n"
                        + "</script></body></html>";
        return webServer.setResponse(path, responseStr, null);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyJavascriptCookie() throws Throwable {
        // Using SSL server here since CookieStore API requires a secure schema.
        TestWebServer webServer = TestWebServer.startSsl();
        try {
            // This test again uses 127.0.0.1/localhost trick to simulate a third party.
            ThirdPartyCookiesTestHelper thirdParty = new ThirdPartyCookiesTestHelper(webServer);

            allowFirstPartyCookies();
            blockThirdPartyCookies(thirdParty.getAwContents());

            // We can't set third party cookies.
            thirdParty.assertThirdPartyIFrameCookieResult("1", false);

            allowThirdPartyCookies(thirdParty.getAwContents());

            // We can set third party cookies.
            thirdParty.assertThirdPartyIFrameCookieResult("2", true);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testThirdPartyCookiesArePerWebview() throws Throwable {
        // Using SSL server here since CookieStore API requires a secure schema.
        TestWebServer webServer = TestWebServer.startSsl();
        try {
            allowFirstPartyCookies();
            mCookieManager.removeAllCookies();
            Assert.assertFalse(mCookieManager.hasCookies());

            ThirdPartyCookiesTestHelper helperOne = new ThirdPartyCookiesTestHelper(webServer);
            ThirdPartyCookiesTestHelper helperTwo = new ThirdPartyCookiesTestHelper(webServer);

            blockThirdPartyCookies(helperOne.getAwContents());
            blockThirdPartyCookies(helperTwo.getAwContents());
            helperOne.assertThirdPartyIFrameCookieResult("1", false);
            helperTwo.assertThirdPartyIFrameCookieResult("2", false);

            allowThirdPartyCookies(helperTwo.getAwContents());
            Assert.assertFalse(
                    "helperOne's third-party cookie setting should be unaffected",
                    helperOne.getSettings().getAcceptThirdPartyCookies());
            helperOne.assertThirdPartyIFrameCookieResult("3", false);
            helperTwo.assertThirdPartyIFrameCookieResult("4", true);

            allowThirdPartyCookies(helperOne.getAwContents());
            Assert.assertTrue(
                    "helperTwo's third-party cookie setting shoudl be unaffected",
                    helperTwo.getSettings().getAcceptThirdPartyCookies());
            helperOne.assertThirdPartyIFrameCookieResult("5", true);
            helperTwo.assertThirdPartyIFrameCookieResult("6", true);

            blockThirdPartyCookies(helperTwo.getAwContents());
            Assert.assertTrue(
                    "helperOne's third-party cookie setting should be unaffected",
                    helperOne.getSettings().getAcceptThirdPartyCookies());
            helperOne.assertThirdPartyIFrameCookieResult("7", true);
            helperTwo.assertThirdPartyIFrameCookieResult("8", false);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add("webview-intercepted-cookie-header")
    public void testPartitionedNetCookies() throws Throwable {
        TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();

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

            String url = toThirdPartyUrl(makeIframeUrl(webServer, "/parent.html", iframeUrl));

            allowFirstPartyCookies();
            allowThirdPartyCookies(mAwContents);

            String expectedCookies = "partitioned_cookie=foo; unpartitioned_cookie=bar";
            String failureMessage = "All cookies should be returned when 3PCs are enabled";
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(
                    failureMessage,
                    expectedCookies,
                    webServer.getLastRequest("/path_to_intercept").headerValue("Cookie"));

            var interceptedRequest =
                    shouldInterceptRequestHelper.getRequestsForUrl(iframeUrl + "path_to_intercept");
            Assert.assertEquals(
                    failureMessage,
                    expectedCookies,
                    interceptedRequest.requestHeaders.get("Cookie"));

            expectedCookies = "partitioned_cookie=foo";
            failureMessage = "Partitioned cookies should be returned when 3PCs are disabled";
            blockThirdPartyCookies(mAwContents);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(
                    failureMessage,
                    expectedCookies,
                    webServer.getLastRequest("/path_to_intercept").headerValue("Cookie"));

            interceptedRequest =
                    shouldInterceptRequestHelper.getRequestsForUrl(iframeUrl + "path_to_intercept");
            Assert.assertEquals(
                    failureMessage,
                    expectedCookies,
                    interceptedRequest.requestHeaders.get("Cookie"));

            failureMessage = "No cookies should be returned when all cookies are disabled";
            blockAllCookies();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(
                    failureMessage,
                    "",
                    webServer.getLastRequest("/path_to_intercept").headerValue("Cookie"));

            interceptedRequest =
                    shouldInterceptRequestHelper.getRequestsForUrl(iframeUrl + "path_to_intercept");
            Assert.assertEquals(
                    failureMessage, false, interceptedRequest.requestHeaders.containsKey("Cookie"));

        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add("disable-partitioned-cookies")
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

            String url = toThirdPartyUrl(makeIframeUrl(webServer, "/parent.html", iframeUrl));

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
    @Features.EnableFeatures({AwFeatures.WEBVIEW_AUTO_SAA})
    public void testPartitionedJSCookies() throws Throwable {
        String partitionedCookie = "partitioned-cookie=123";
        String unpartitionedCookie = "regular-cookie=456";

        TestWebServer webServer = TestWebServer.start();
        // Add an asset statement to test storage access since we can auto grant
        // under these circumstances.
        webServer.setResponse(
                "/.well-known/assetlinks.json",
                String.format(
                        ASSET_STATEMENT_TEMPLATE,
                        BuildInfo.getInstance().hostPackageName,
                        BuildInfo.getInstance().getHostSigningCertSha256()),
                null);

        try {
            // TODO(crbug.com/41496912): The WebView cookie manager API does not currently
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
                    (boolean requestStorageAccess) -> {
                        String iframeUrl =
                                toThirdPartyUrl(
                                        makeCookieScriptResultsUrl(
                                                webServer,
                                                "/iframe.html",
                                                requestStorageAccess,
                                                partitionedCookie
                                                        + "; Secure; Path=/; SameSite=None;"
                                                        + " Partitioned;",
                                                unpartitionedCookie
                                                        + "; Secure; Path=/; SameSite=None;"));

                        String url = makeIframeUrl(webServer, "/parent.html", iframeUrl);

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
                    "Only partitioned cookies should be returned when 3PCs are disabled",
                    partitionedCookie,
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ false));

            Assert.assertEquals(
                    "All cookies should be returned when SAA requested",
                    partitionedCookie + "; " + unpartitionedCookie,
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ true));

            allowThirdPartyCookies(mAwContents);
            Assert.assertEquals(
                    "All cookies should be returned when 3PCs are enabled",
                    partitionedCookie + "; " + unpartitionedCookie,
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ false));

            blockAllCookies();
            Assert.assertEquals(
                    "No cookies should ever be returned if all cookies are disabled",
                    "",
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ false));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add("disable-partitioned-cookies")
    @Features.EnableFeatures({AwFeatures.WEBVIEW_AUTO_SAA})
    public void testDisabledPartitionedJSCookies() throws Throwable {
        String partitionedCookie = "partitioned-cookie=123";
        String unpartitionedCookie = "regular-cookie=456";

        TestWebServer webServer = TestWebServer.start();
        // Add an asset statement to test storage access since we can auto grant
        // under these circumstances.
        webServer.setResponse(
                "/.well-known/assetlinks.json",
                String.format(
                        ASSET_STATEMENT_TEMPLATE,
                        BuildInfo.getInstance().hostPackageName,
                        BuildInfo.getInstance().getHostSigningCertSha256()),
                null);

        try {
            // TODO(https://crbug.com/1523964): The WebView cookie manager API does not currently
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
                    (boolean requestStorageAccess) -> {
                        String iframeUrl =
                                toThirdPartyUrl(
                                        makeCookieScriptResultsUrl(
                                                webServer,
                                                "/iframe.html",
                                                requestStorageAccess,
                                                partitionedCookie
                                                        + "; Secure; Path=/; SameSite=None;"
                                                        + " Partitioned;",
                                                unpartitionedCookie
                                                        + "; Secure; Path=/; SameSite=None;"));

                        String url = makeIframeUrl(webServer, "/parent.html", iframeUrl);

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
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ false));

            Assert.assertEquals(
                    "All cookies should be returned when SAA is requested.",
                    partitionedCookie + "; " + unpartitionedCookie,
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ true));

            allowThirdPartyCookies(mAwContents);
            Assert.assertEquals(
                    "All cookies should be returned when 3PCs are enabled",
                    partitionedCookie + "; " + unpartitionedCookie,
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ false));

            blockAllCookies();
            Assert.assertEquals(
                    "No cookies should ever be returned if all cookies are disabled",
                    "",
                    iframeCookiesSupplier.get(/* requestStorageAccess= */ false));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testModernCookieSameSite_Disabled() throws Throwable {
        // Tests that the legacy behavior is active when "modern" SameSite behavior is not specified
        // via command-line flag.
        TestWebServer httpWebServer = TestWebServer.start();
        TestWebServer httpsWebServer = TestWebServer.startSsl();
        try {
            ModernCookieSameSiteTestHelper httpHelper =
                    new ModernCookieSameSiteTestHelper(httpWebServer, httpsWebServer);
            ModernCookieSameSiteTestHelper httpsHelper =
                    new ModernCookieSameSiteTestHelper(httpsWebServer, httpWebServer);

            httpHelper.assertModernCookieSameSiteResult("-disabled-http", false);
            httpsHelper.assertModernCookieSameSiteResult("-disabled-https", false);
        } finally {
            httpWebServer.shutdown();
            httpsWebServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_ENABLE_MODERN_COOKIE_SAME_SITE)
    public void testModernCookieSameSite_Enabled() throws Throwable {
        TestWebServer httpWebServer = TestWebServer.start();
        TestWebServer httpsWebServer = TestWebServer.startSsl();
        try {
            ModernCookieSameSiteTestHelper httpHelper =
                    new ModernCookieSameSiteTestHelper(httpWebServer, httpsWebServer);
            ModernCookieSameSiteTestHelper httpsHelper =
                    new ModernCookieSameSiteTestHelper(httpsWebServer, httpWebServer);

            httpHelper.assertModernCookieSameSiteResult("-enabled-http", true);
            httpsHelper.assertModernCookieSameSiteResult("-enabled-https", true);
        } finally {
            httpWebServer.shutdown();
            httpsWebServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAcceptFileSchemeCookies() throws Throwable {
        mCookieManager.setAcceptFileSchemeCookies(true);
        Assert.assertTrue(
                "allowFileSchemeCookies() should return true after "
                        + "setAcceptFileSchemeCookies(true)",
                mCookieManager.allowFileSchemeCookies());
        mAwContents.getSettings().setAllowFileAccess(true);

        mAwContents.getSettings().setAcceptThirdPartyCookies(true);
        Assert.assertTrue(fileURLCanSetCookie("1", ""));
        mAwContents.getSettings().setAcceptThirdPartyCookies(false);
        Assert.assertTrue(fileURLCanSetCookie("2", ""));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testRejectFileSchemeCookies() throws Throwable {
        mCookieManager.setAcceptFileSchemeCookies(false);
        Assert.assertFalse(
                "allowFileSchemeCookies() should return false after "
                        + "setAcceptFileSchemeCookies(false)",
                mCookieManager.allowFileSchemeCookies());
        mAwContents.getSettings().setAllowFileAccess(true);

        mAwContents.getSettings().setAcceptThirdPartyCookies(true);
        Assert.assertFalse(fileURLCanSetCookie("3", ""));
        mAwContents.getSettings().setAcceptThirdPartyCookies(false);
        Assert.assertFalse(fileURLCanSetCookie("4", ""));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testInvokeAcceptFileSchemeCookiesTooLate() throws Throwable {
        // AwCookieManager only respects calls to setAcceptFileSchemeCookies() which happen *before*
        // the underlying cookie store is first used. Here we call into the cookie store with
        // placeholder values to trigger this case, so we can test the CookieManager's observable
        // state (mainly, that allowFileSchemeCookies() is consistent with the actual behavior of
        // rejecting/accepting file scheme cookies).
        mCookieManager.setCookie("https://www.any.url.will.work/", "any-key=any-value");

        // Now try to enable file scheme cookies.
        mCookieManager.setAcceptFileSchemeCookies(true);
        Assert.assertFalse(
                "allowFileSchemeCookies() should return false if "
                        + "setAcceptFileSchemeCookies was called too late",
                mCookieManager.allowFileSchemeCookies());
        mAwContents.getSettings().setAllowFileAccess(true);

        mAwContents.getSettings().setAcceptThirdPartyCookies(true);
        Assert.assertFalse(fileURLCanSetCookie("5", ""));
        mAwContents.getSettings().setAcceptThirdPartyCookies(false);
        Assert.assertFalse(fileURLCanSetCookie("6", ""));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAcceptFileSchemeCookiesExplicitSameSite() throws Throwable {
        mCookieManager.setAcceptFileSchemeCookies(true);
        Assert.assertTrue(
                "allowFileSchemeCookies() should return true after "
                        + "setAcceptFileSchemeCookies(true)",
                mCookieManager.allowFileSchemeCookies());
        mAwContents.getSettings().setAllowFileAccess(true);
        mAwContents.getSettings().setAcceptThirdPartyCookies(false);
        Assert.assertTrue(fileURLCanSetCookie("7", ";SameSite=Lax"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testFileSchemeCookies_treatedAsSameSite() throws Throwable {
        mCookieManager.setAcceptFileSchemeCookies(true);
        mCookieManager.setCookie("file:///android_asset/first_url.html", "testCookie=value");
        String cookie = mCookieManager.getCookie("file:///android_asset/second_url.html");
        assertThat(cookie, containsString("testCookie"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testFileSchemeCookies_canBeAccessedFromChildPath() throws Throwable {
        mCookieManager.setAcceptFileSchemeCookies(true);
        mCookieManager.setCookie(
                "file:///android_asset/first_url.html",
                "testCookie=value;path=file:///android_asset/");
        String cookie = mCookieManager.getCookie("file:///android_asset/child/second_url.html");
        assertThat(cookie, containsString("testCookie"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testFileSchemeCookies_cannotBeAccessedFromParentPath() throws Throwable {
        mCookieManager.setAcceptFileSchemeCookies(true);
        mCookieManager.setCookie(
                "file:///android_asset/child/first_url.html",
                "testCookie=value;path=file:///android_asset/child/");
        String cookie = mCookieManager.getCookie("file:///android_asset/second_url.html");
        assertThat(cookie, not(containsString("testCookie")));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testFileSchemeCookies_cannotBeAccessedFromDifferentPath() throws Throwable {
        mCookieManager.setAcceptFileSchemeCookies(true);
        mCookieManager.setCookie(
                "file:///android_asset/first/first_url.html",
                "testCookie=value;path=file:///android_asset/first/");
        String cookie = mCookieManager.getCookie("file:///android_asset/second/second_url.html");
        assertThat(cookie, not(containsString("testCookie")));
    }

    private boolean fileURLCanSetCookie(String valueSuffix, String settings) throws Throwable {
        String value = "value" + valueSuffix;
        String url = "file:///android_asset/cookie_test.html?value=" + value + settings;
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        String cookie = mCookieManager.getCookie(url);
        return cookie != null && cookie.contains("test=" + value);
    }

    class ThirdPartyCookiesTestHelper {
        protected final AwContents mAwContents;
        protected final TestAwContentsClient mContentsClient;
        protected final TestWebServer mWebServer;

        ThirdPartyCookiesTestHelper(TestWebServer webServer) {
            mWebServer = webServer;
            mContentsClient = new TestAwContentsClient();
            final AwTestContainerView containerView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
            mAwContents = containerView.getAwContents();
            mAwContents.getSettings().setJavaScriptEnabled(true);
        }

        AwContents getAwContents() {
            return mAwContents;
        }

        AwSettings getSettings() {
            return mAwContents.getSettings();
        }

        TestWebServer getWebServer() {
            return mWebServer;
        }

        void assertThirdPartyIFrameCookieResult(String suffix, boolean expectedResult)
                throws Throwable {
            String key = "test" + suffix;
            String cookieStoreKey = "cookieStoreTest" + suffix;
            String value = "value" + suffix;
            String iframePath = "/iframe_" + suffix + ".html";
            String pagePath = "/content_" + suffix + ".html";

            // We create a script which tries to set a cookie on a third party.
            String cookieUrl =
                    toThirdPartyUrl(makeCookieScriptUrl(getWebServer(), iframePath, key, value));

            // Then we load it as an iframe.
            String url = makeIframeUrl(getWebServer(), pagePath, cookieUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

            if (expectedResult) {
                assertHasCookies(cookieUrl);
                validateCookies(cookieUrl, key);
            } else {
                assertNoCookies(cookieUrl);
            }

            // Try to set via CookieStore API as well.
            JavaScriptUtils.runJavascriptWithAsyncResult(
                    mAwContents.getWebContents(), "callIframe('" + cookieStoreKey + "')");

            if (expectedResult) {
                assertHasCookies(cookieUrl);
                validateCookies(cookieUrl, key, cookieStoreKey);
            } else {
                assertNoCookies(cookieUrl);
            }

            // Clear the cookies.
            clearCookies();
            Assert.assertFalse(mCookieManager.hasCookies());
        }
    }

    class ModernCookieSameSiteTestHelper {
        protected final AwContents mAwContents;
        protected final TestWebServer mWebServer;
        protected final TestWebServer mCrossSchemeWebServer;

        ModernCookieSameSiteTestHelper(
                TestWebServer webServer, TestWebServer crossSchemeWebServer) {
            mWebServer = webServer;
            mCrossSchemeWebServer = crossSchemeWebServer;
            final AwTestContainerView containerView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
            mAwContents = containerView.getAwContents();
            mAwContents.getSettings().setJavaScriptEnabled(true);

            allowFirstPartyCookies();
            allowThirdPartyCookies(mAwContents);
        }

        void assertModernCookieSameSiteResult(
                String suffix, boolean expectedIsSameSiteBehaviorModern) throws Throwable {
            assertSameSiteLaxByDefaultResult(suffix, expectedIsSameSiteBehaviorModern);
            assertSameSiteNoneRequiresSecureResult(suffix, expectedIsSameSiteBehaviorModern);
            assertSchemefulSameSiteResult(suffix, expectedIsSameSiteBehaviorModern);
        }

        private void assertSameSiteLaxByDefaultResult(String suffix, boolean expectedIsLaxByDefault)
                throws Throwable {
            final String key = "test-lax-by-default" + suffix;
            final String value = "value" + suffix;
            final String iframePath = "/iframe_" + suffix + ".html";
            final String pagePath = "/content_" + suffix + ".html";

            // We create a script which tries to set a cookie on a cross-site URL. The cookie does
            // not specify a SameSite attribute, so its SameSite mode is whatever the default is.
            final String cookieUrl =
                    toThirdPartyUrl(makeCookieScriptUrl(mWebServer, iframePath, key, value));

            // Then we load it as an iframe, to attempt to set a default cookie in a cross-site
            // context. It should be rejected if SameSite=Lax by Default is active.
            final String url = makeIframeUrl(mWebServer, pagePath, cookieUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

            if (expectedIsLaxByDefault) {
                assertNoCookies(cookieUrl);
            } else {
                assertHasCookies(cookieUrl);
                validateCookies(cookieUrl, key);
            }

            // Clear the cookies.
            clearCookies();
            Assert.assertFalse(mCookieManager.hasCookies());
        }

        private void assertSameSiteNoneRequiresSecureResult(
                String suffix, boolean expectedDoesNoneRequireSecure) throws Throwable {
            final String path = "/cookie_test.html";
            final String responseStr =
                    "<html><head><title>TEST!</title></head><body>HELLO!</body></html>";
            final List<Pair<String, String>> responseHeaders =
                    new ArrayList<Pair<String, String>>();
            final String headerCookieName = "test-none-requires-secure" + suffix;
            final String headerCookieValue = "value" + suffix;

            // Attempt to set a SameSite=None cookie without Secure. It should be rejected if
            // SameSite=None Requires Secure is active.
            responseHeaders.add(
                    Pair.create(
                            "Set-Cookie",
                            headerCookieName + "=" + headerCookieValue + "; SameSite=None"));
            String url = mWebServer.setResponse(path, responseStr, responseHeaders);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

            if (expectedDoesNoneRequireSecure) {
                assertNoCookies(url);
            } else {
                waitForCookie(url);
                assertHasCookies(url);
                validateCookies(url, headerCookieName);
            }

            // Clear the cookies.
            clearCookies();
            Assert.assertFalse(mCookieManager.hasCookies());
        }

        private void assertSchemefulSameSiteResult(
                String suffix, boolean expectedIsSameSiteSchemeful) throws Throwable {
            final String key = "test-schemeful-same-site" + suffix;
            final String value = "value" + suffix;
            final String iframePath = "/iframe_" + suffix + ".html";
            final String pagePath = "/content_" + suffix + ".html";

            // We create a script which tries to set a Lax cookie on a cross-scheme URL.
            final String cookieUrl =
                    makeSameSiteLaxCookieScriptUrl(mCrossSchemeWebServer, iframePath, key, value);

            // Then we load it as an iframe, to attempt to set a Lax cookie in a cross-scheme
            // context. It should be rejected if same-site includes scheme.
            final String url = makeIframeUrl(mWebServer, pagePath, cookieUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

            if (expectedIsSameSiteSchemeful) {
                assertNoCookies(cookieUrl);
            } else {
                assertHasCookies(cookieUrl);
                validateCookies(cookieUrl, key);
            }

            // Clear the cookies.
            clearCookies();
            Assert.assertFalse(mCookieManager.hasCookies());
        }
    }

    /**
     * Creates a response on the TestWebServer which load a given URL in an iframe,
     * and provides helpers for forwarding JavaScript calls to that iframe via postMessage.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/my_thing_with_iframe.html")
     * @param  url the url which which should appear as the src of the iframe.
     * @return  the url which gets the response
     */
    private String makeIframeUrl(TestWebServer webServer, String path, String url) {
        String responseStr =
                "<html><head><title>Content!</title>"
                        + "<script>"
                        + "window.onmessage = function(ev) { "
                        + "  window.domAutomationController.send(ev.data); "
                        + "}\n"
                        + "function callIframe(data) { "
                        + "  document.getElementById('if').contentWindow.postMessage("
                        + "      data, '*'); "
                        + "}"
                        + "</script>"
                        + "</head><body><iframe id=if src="
                        + url
                        + "></iframe></body></html>";
        return webServer.setResponse(path, responseStr, null);
    }

    /**
     * Creates a response on the TestWebServer with a script that attempts to set a cookie.
     *
     * @param webServer the webServer on which to create the response
     * @param path the path component of the url (e.g "/cookie_test.html")
     * @param key the key of the cookie
     * @param value the value of the cookie
     * @return the url which gets the response
     */
    private String makeCookieScriptUrl(
            TestWebServer webServer, String path, String key, String value) {
        String response =
                "<html><head></head><body>"
                        + "<script>document.cookie = \""
                        + key
                        + "="
                        + value
                        + "\";"
                        + "window.onmessage = async function(ev) {"
                        + makeCookieStoreSetFragment(
                                "ev.data", "'" + value + "'", "ev.source.postMessage(true, '*');")
                        + "}"
                        + "</script></body></html>";
        return webServer.setResponse(path, response, null);
    }

    /**
     * Creates a response on the TestWebServer with a script that attempts to set a SameSite=Lax
     * cookie.
     * @param  webServer  the webServer on which to create the response
     * @param  path the path component of the url (e.g "/cookie_test.html")
     * @param  key the key of the cookie
     * @param  value the value of the cookie
     * @return  the url which gets the response
     */
    private String makeSameSiteLaxCookieScriptUrl(
            TestWebServer webServer, String path, String key, String value) {
        String response =
                "<html><head></head><body>"
                        + "<script>document.cookie = \""
                        + key
                        + "="
                        + value
                        + "; SameSite=Lax\";"
                        + "</script></body></html>";
        return webServer.setResponse(path, response, null);
    }

    /**
     * Returns code fragment to be embedded into an async function to set a cookie with CookieStore
     * API
     * @param name name of cookie to set
     * @param value value to set the cookie to
     * @param finallyAction code to run once set finishes, regardless of success or failure
     */
    private String makeCookieStoreSetFragment(String name, String value, String finallyAction) {
        return "try {"
                + "  await window.cookieStore.set("
                + "      { name: "
                + name
                + ","
                + "        value: "
                + value
                + ","
                + "        expires: Date.now() + 3600*1000,"
                + "        sameSite: 'none' });"
                + "} finally {"
                + "  "
                + finallyAction
                + "}\n";
    }

    /**
     * Creates a response on the TestWebServer with a script that attempts to set a list of cookies
     * and then reports them back to a java bridge.
     *
     * @param webServer the webServer on which to create the response
     * @param path the path component of the url (e.g "/cookie_test.html")
     * @param cookies A list of cookies to set
     * @return the url which gets the response
     */
    private String makeCookieScriptResultsUrl(
            TestWebServer webServer, String path, boolean requestStorageAccess, String... cookies) {
        String response = "<html><body><script>";

        if (requestStorageAccess) {
            response += "document.requestStorageAccess().then(() => {";
        }

        for (String cookie : cookies) {
            response += String.format("document.cookie='%s';", cookie);
        }

        response += "cookieResults.report(document.cookie);";

        if (requestStorageAccess) {
            response += "}).catch((e) => cookieResults.report('Failed to retrieve ' + e));";
        }

        response += "</script></body></html>";

        return webServer.setResponse(path, response, null);
    }

    /**
     * Makes a url look as if it comes from a different host.
     * @param  url the url to fake.
     * @return  the resulting url after faking.
     */
    private String toThirdPartyUrl(String url) {
        return url.replace("localhost", "127.0.0.1");
    }

    private void setCookieOnUiThread(
            final String url, final String cookie, final Callback<Boolean> callback) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mCookieManager.setCookie(url, cookie, callback));
    }

    private boolean setCookieOnUiThreadSync(final String url, final String cookie) {
        final SettableFuture<Boolean> cookieResultFuture = SettableFuture.create();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> mCookieManager.setCookie(url, cookie, cookieResultFuture::set));
        Boolean success = AwActivityTestRule.waitForFuture(cookieResultFuture);
        if (success == null) {
            throw new RuntimeException("setCookie() should never return null in its callback");
        }
        return success;
    }

    private void removeSessionCookiesOnUiThread(final Callback<Boolean> callback) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mCookieManager.removeSessionCookies(callback));
    }

    private void removeAllCookiesOnUiThread(final Callback<Boolean> callback) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mCookieManager.removeAllCookies(callback));
    }

    /** Clears all cookies synchronously. */
    private void clearCookies() throws Throwable {
        CookieUtils.clearCookies(InstrumentationRegistry.getInstrumentation(), mCookieManager);
    }

    private void waitForCookie(final String url) {
        AwActivityTestRule.pollInstrumentationThread(() -> mCookieManager.getCookie(url) != null);
    }

    private void validateCookies(String url, String... expectedCookieNames) {
        final String responseCookie = mCookieManager.getCookie(url);
        String[] cookies = responseCookie.split(";");
        // Convert to sets, since Set#equals() hooks in nicely with assertEquals()
        Set<String> foundCookieNamesSet = new HashSet<String>();
        for (String cookie : cookies) {
            foundCookieNamesSet.add(cookie.substring(0, cookie.indexOf("=")).trim());
        }
        Set<String> expectedCookieNamesSet =
                new HashSet<String>(Arrays.asList(expectedCookieNames));
        Assert.assertEquals(
                "Found cookies list differs from expected list",
                expectedCookieNamesSet,
                foundCookieNamesSet);
    }

    /**
     * Makes a cookie which expires {@code secondsTillExpiry} seconds after the cookie is set. Note:
     * cookie expiration can only be specified to a precisiion of seconds, not to the millisecond.
     * See https://tools.ietf.org/html/rfc6265#section-4.1 and
     * https://tools.ietf.org/html/rfc7231#section-7.1.1.2 for details.
     */
    @SuppressWarnings("deprecation")
    private String makeExpiringCookie(String cookie, @CookieLifetime int secondsTillExpiry) {
        // Use "Max-Age" instead of "Expires", since "Max-Age" is relative to the time the cookie is
        // set, rather than a call to the Date constructor when building this cookie string.
        return cookie + "; Max-Age=" + secondsTillExpiry;
    }

    /**
     * @return an expiry date in the standard IMF-fixdate format defined by RFC 7231. The expiry
     * date will outlive the test so that it can be read during the test.
     */
    private String getHttpCookieExpiryDate() {
        final DateFormat format = new SimpleDateFormat("E, dd MMM yyyy HH:mm:ss z");
        format.setTimeZone(TimeZone.getTimeZone("GMT"));
        Date expiry = new Date();
        expiry.setTime(expiry.getTime() + CookieLifetime.OUTLIVE_THE_TEST_SEC * 1000);
        String formattedDate = format.format(expiry);
        // On some platforms, getting the date string includes '+00:00' at the end but the cookie
        // API does not return this so we want to remove it if it is present.
        if (formattedDate.endsWith("+00:00")) {
            formattedDate = formattedDate.substring(0, formattedDate.length() - 6);
        }
        return formattedDate;
    }

    /**
     * Asserts there are no cookies set for the given URL. This makes no assertions about other
     * URLs.
     *
     * @param cookieUrl the URL for which we expect no cookies to be set.
     */
    private void assertNoCookies(final String cookieUrl) {
        String msg = "Expected to not see cookies for '" + cookieUrl + "'";
        Assert.assertNull(msg, mCookieManager.getCookie(cookieUrl));
    }

    /** Asserts there are no cookies set at all. */
    private void assertNoCookies() {
        String msg = "Expected to CookieManager to have no cookies";
        Assert.assertFalse(msg, mCookieManager.hasCookies());
    }

    /**
     * Asserts there are cookies set for the given URL.
     *
     * @param cookieUrl the URL for which to check for cookies.
     */
    private void assertHasCookies(final String cookieUrl) {
        String msg =
                "Expected CookieManager to have cookies for '"
                        + cookieUrl
                        + "' but it has no cookies";
        Assert.assertTrue(msg, mCookieManager.hasCookies());
        msg = "Expected getCookie to return non-null for '" + cookieUrl + "'";
        Assert.assertNotNull(msg, mCookieManager.getCookie(cookieUrl));
    }

    /**
     * Asserts the cookie key/value pair for a given URL. Note: {@code cookieKeyValuePair} must
     * exactly match the expected {@link AwCookieManager#getCookie()} output, which may return
     * multiple key-value pairs.
     *
     * @param cookieKeyValuePair the expected key/value pair.
     * @param cookieUrl the URL to check cookies for.
     */
    private void assertCookieEquals(final String cookieKeyValuePair, final String cookieUrl) {
        assertHasCookies(cookieUrl);
        String msg = "Unexpected cookie key/value pair";
        Assert.assertEquals(msg, cookieKeyValuePair, mCookieManager.getCookie(cookieUrl));
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

    interface IframeCookieSupplier {
        String get(boolean requestStorageAccess);
    }
}
