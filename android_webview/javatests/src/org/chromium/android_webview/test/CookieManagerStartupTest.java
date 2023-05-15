// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Looper;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for CookieManager/Chromium startup ordering weirdness.
 *
 * This tests various cases around ordering of calls to CookieManager at startup, and thus is
 * separate from the normal CookieManager tests so it can control call ordering carefully.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class CookieManagerStartupTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {
        @Override
        public boolean needsAwBrowserContextCreated() {
            return false;
        }

        @Override
        public boolean needsBrowserProcessStarted() {
            return false;
        }
    };

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    @Before
    public void setUp() {
        // CookieManager assumes that native is loaded, but webview browser should not be loaded for
        // these tests as webview is not necessarily loaded when CookieManager is called.
        AwBrowserProcess.loadLibrary(null);
    }

    /**
     * Called when a test wants to initiate normal Chromium process startup, after
     * doing any CookieManager calls that are supposed to happen before the UI thread
     * is committed.
     */
    private void startChromium() {
        ThreadUtils.setUiThread(Looper.getMainLooper());
        startChromiumWithClient(new TestAwContentsClient());
    }

    /**
     * Called when a test wants to initiate normal Chromium process startup, after
     * doing any CookieManager calls that are supposed to happen before the UI thread
     * is committed.
     */
    private void startChromiumWithClient(TestAwContentsClient contentsClient) {
        mActivityTestRule.createAwBrowserContext();
        mActivityTestRule.startBrowserProcess();
        mContentsClient = contentsClient;
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mAwContents.getSettings().setJavaScriptEnabled(true);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testStartup() throws Throwable {
        ThreadUtils.setWillOverrideUiThread();
        TestWebServer webServer = TestWebServer.start();
        try {
            String path = "/cookie_test.html";
            String url = webServer.setResponse(path, CommonResources.ABOUT_HTML, null);

            // Verify that we can use AwCookieManager successfully before having started Chromium.
            AwCookieManager cookieManager = new AwCookieManager();
            Assert.assertNotNull(cookieManager);

            CookieUtils.clearCookies(InstrumentationRegistry.getInstrumentation(), cookieManager);
            Assert.assertFalse(cookieManager.hasCookies());

            cookieManager.setAcceptCookie(true);
            Assert.assertTrue(cookieManager.acceptCookie());

            cookieManager.setCookie(url, "count=41");

            // Now start Chromium to cause the switch from the temporary cookie store to the real
            // Mojo store.
            startChromium();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                    "var c=document.cookie.split('=');document.cookie=c[0]+'='+(1+(+c[1]));");

            // Verify that the cookie value we set before was successfully passed through to the
            // Mojo store.
            Assert.assertEquals("count=42", cookieManager.getCookie(url));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAllowFileSchemeCookies() {
        AwCookieManager cookieManager = new AwCookieManager();
        Assert.assertFalse(cookieManager.allowFileSchemeCookies());
        cookieManager.setAcceptFileSchemeCookies(true);
        Assert.assertTrue(cookieManager.allowFileSchemeCookies());
        cookieManager.setAcceptFileSchemeCookies(false);
        Assert.assertFalse(cookieManager.allowFileSchemeCookies());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testAllowCookies() {
        AwCookieManager cookieManager = new AwCookieManager();
        Assert.assertTrue(cookieManager.acceptCookie());
        cookieManager.setAcceptCookie(false);
        Assert.assertFalse(cookieManager.acceptCookie());
        cookieManager.setAcceptCookie(true);
        Assert.assertTrue(cookieManager.acceptCookie());
    }

    // https://code.google.com/p/chromium/issues/detail?id=374203
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testShouldInterceptRequestDeadlock() throws Throwable {
        ThreadUtils.setWillOverrideUiThread();
        ThreadUtils.setUiThread(Looper.getMainLooper());
        String url = "http://www.example.com";
        TestAwContentsClient contentsClient = new TestAwContentsClient() {
            @Override
            public WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request) {
                (new AwCookieManager()).getCookie("www.example.com");
                return null;
            }
        };
        startChromiumWithClient(contentsClient);
        mActivityTestRule.loadUrlSync(mAwContents, contentsClient.getOnPageFinishedHelper(), url);
    }
}
