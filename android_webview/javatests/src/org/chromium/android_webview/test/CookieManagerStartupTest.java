// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for CookieManager/Chromium startup ordering weirdness.
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
        ThreadUtils.setUiThread(null);
        ThreadUtils.setWillOverrideUiThread(true);

        // CookieManager assumes that native is loaded, but webview browser should not be loaded for
        // these tests as webview is not necessarily loaded when CookieManager is called.
        AwBrowserProcess.loadLibrary(null);
    }

    @After
    public void tearDown() {
        ThreadUtils.setWillOverrideUiThread(false);
    }

    private void startChromium() {
        ThreadUtils.setUiThread(Looper.getMainLooper());
        startChromiumWithClient(new TestAwContentsClient());
    }

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
        TestWebServer webServer = TestWebServer.start();
        try {
            String path = "/cookie_test.html";
            String url = webServer.setResponse(path, CommonResources.ABOUT_HTML, null);

            AwCookieManager cookieManager = new AwCookieManager();
            Assert.assertNotNull(cookieManager);

            CookieUtils.clearCookies(InstrumentationRegistry.getInstrumentation(), cookieManager);
            Assert.assertFalse(cookieManager.hasCookies());

            cookieManager.setAcceptCookie(true);
            Assert.assertTrue(cookieManager.acceptCookie());

            cookieManager.setCookie(url, "count=41");

            startChromium();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient,
                    "var c=document.cookie.split('=');document.cookie=c[0]+'='+(1+(+c[1]));");

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
        ThreadUtils.setUiThread(Looper.getMainLooper());
        String url = "http://www.example.com";
        TestAwContentsClient contentsClient = new TestAwContentsClient() {
            @Override
            public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
                (new AwCookieManager()).getCookie("www.example.com");
                return null;
            }
        };
        startChromiumWithClient(contentsClient);
        mActivityTestRule.loadUrlSync(mAwContents, contentsClient.getOnPageFinishedHelper(), url);
    }
}
