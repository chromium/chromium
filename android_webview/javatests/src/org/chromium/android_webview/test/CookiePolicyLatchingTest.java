// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

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
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for the latched cookie policy feature (kWebViewLatchedCookiePolicy).
 *
 * <p>When enabled, cookie policy settings are captured at RestrictedCookieManager creation time
 * (which happens per-navigation) and used throughout the RCM's lifetime. This enables shared memory
 * cookie versioning to reduce IPC overhead.
 *
 * <p>These tests use document.cookie to verify RCM behavior, since CookieManager.setCookie()
 * bypasses the RCM and goes directly to the cookie store.
 */
@DoNotBatch(reason = "CookieManager is global state, so we use a fresh process out of caution.")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class CookiePolicyLatchingTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private AwCookieManager mCookieManager;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    public CookiePolicyLatchingTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mCookieManager = new AwCookieManager();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mAwContents.getSettings().setJavaScriptEnabled(true);
        mWebServer = TestWebServer.start();
        Assert.assertNotNull(mCookieManager);
    }

    @After
    public void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        // Even though we use a fresh process, we still need to clear cookie state off of disk so
        // that it's not read in for the next test case.
        try {
            CookieUtils.clearCookies(InstrumentationRegistry.getInstrumentation(), mCookieManager);
        } catch (Throwable e) {
            throw new RuntimeException("Could not clear cookies.", e);
        }
    }

    /** Sets a cookie via JavaScript document.cookie. */
    private void setCookieViaJs(String name, String value) throws Throwable {
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "document.cookie = '" + name + "=" + value + "'");
    }

    /** Gets document.cookie via JavaScript. Returns the raw cookie string. */
    private String getCookiesViaJs() throws Throwable {
        String result =
                JSUtils.executeJavaScriptAndWaitForResult(
                        InstrumentationRegistry.getInstrumentation(),
                        mAwContents,
                        mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                        "document.cookie");
        // Result is quoted, e.g., "\"foo=bar\"", so strip the quotes.
        if (result.startsWith("\"") && result.endsWith("\"")) {
            result = result.substring(1, result.length() - 1);
        }
        return result;
    }

    /**
     * Tests that when the feature is ENABLED, cookie policy is latched at navigation time.
     *
     * <p>When cookies are enabled at navigation time, JavaScript should be able to set and read
     * cookies via document.cookie, even if the policy is changed after navigation.
     */
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add({"enable-features=WebViewLatchedCookiePolicy"})
    public void testCookiesAccessibleWhenEnabledAtNavigation() throws Throwable {
        // Enable cookies before navigation.
        mCookieManager.setAcceptCookie(true);

        String pageUrl =
                mWebServer.setResponse("/test.html", "<html><body>Test</body></html>", null);

        // Navigate - RCM is created with cookies enabled.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        // Set a cookie via JavaScript - should work.
        setCookieViaJs("testcookie", "value1");

        // Verify the cookie is readable via JavaScript.
        String cookies = getCookiesViaJs();
        Assert.assertTrue(
                "Cookie should be accessible via document.cookie",
                cookies.contains("testcookie=value1"));

        // Now disable cookies AFTER navigation.
        mCookieManager.setAcceptCookie(false);

        // With latching, the cookie should STILL be accessible because the RCM was created
        // when cookies were enabled.
        cookies = getCookiesViaJs();
        Assert.assertTrue(
                "Cookie should still be accessible after policy change (latched)",
                cookies.contains("testcookie=value1"));

        // Setting a new cookie should also still work on the current page.
        setCookieViaJs("anothercookie", "value2");
        cookies = getCookiesViaJs();
        Assert.assertTrue(
                "New cookie should be settable after policy change (latched)",
                cookies.contains("anothercookie=value2"));
    }

    /**
     * Tests that when the feature is ENABLED, navigating after disabling cookies blocks JS access.
     *
     * <p>When cookies are disabled at navigation time, the new page's RCM should block JavaScript
     * cookie access, even if cookies were enabled for the previous page.
     */
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add({"enable-features=WebViewLatchedCookiePolicy"})
    public void testCookiesBlockedWhenDisabledAtNavigation() throws Throwable {
        // First, set a cookie while cookies are enabled.
        mCookieManager.setAcceptCookie(true);

        String pageUrl1 =
                mWebServer.setResponse("/page1.html", "<html><body>Page 1</body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl1);

        setCookieViaJs("existingcookie", "exists");
        Assert.assertTrue(
                "Cookie should be set on page1",
                getCookiesViaJs().contains("existingcookie=exists"));

        // Now disable cookies and navigate to a new page.
        mCookieManager.setAcceptCookie(false);

        String pageUrl2 =
                mWebServer.setResponse("/page2.html", "<html><body>Page 2</body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl2);

        // On this new page, the RCM was created with cookies disabled.
        // JavaScript should NOT be able to read the existing cookie.
        String cookies = getCookiesViaJs();
        Assert.assertFalse(
                "Cookie should NOT be accessible on page with cookies disabled",
                cookies.contains("existingcookie"));

        // JavaScript should NOT be able to set new cookies either.
        setCookieViaJs("blockedcookie", "shouldfail");
        cookies = getCookiesViaJs();
        Assert.assertFalse(
                "New cookie should NOT be settable on page with cookies disabled",
                cookies.contains("blockedcookie"));
    }

    /**
     * Tests that re-enabling cookies takes effect on subsequent navigations.
     *
     * <p>This verifies that the latching mechanism correctly captures the policy state at each
     * navigation, allowing cookies to work again after being re-enabled.
     */
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Privacy"})
    @CommandLineFlags.Add({"enable-features=WebViewLatchedCookiePolicy"})
    public void testCookiesWorkAfterReenabling() throws Throwable {
        // Start with cookies enabled.
        mCookieManager.setAcceptCookie(true);

        String pageUrl1 =
                mWebServer.setResponse("/page1.html", "<html><body>Page 1</body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl1);
        setCookieViaJs("cookie1", "enabled");
        Assert.assertTrue("Cookie1 should be set", getCookiesViaJs().contains("cookie1=enabled"));

        // Disable cookies and navigate.
        mCookieManager.setAcceptCookie(false);

        String pageUrl2 =
                mWebServer.setResponse("/page2.html", "<html><body>Page 2</body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl2);

        // Cookies should be blocked on this page.
        Assert.assertFalse(
                "Cookie1 should NOT be accessible on page2", getCookiesViaJs().contains("cookie1"));

        // Re-enable cookies and navigate to a third page.
        mCookieManager.setAcceptCookie(true);

        String pageUrl3 =
                mWebServer.setResponse("/page3.html", "<html><body>Page 3</body></html>", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl3);

        // Now cookies should work again - can read the existing cookie.
        Assert.assertTrue(
                "Cookie1 should be accessible again on page3",
                getCookiesViaJs().contains("cookie1=enabled"));

        // And can set new cookies.
        setCookieViaJs("cookie3", "works");
        Assert.assertTrue(
                "Cookie3 should be settable on page3", getCookiesViaJs().contains("cookie3=works"));
    }
}
