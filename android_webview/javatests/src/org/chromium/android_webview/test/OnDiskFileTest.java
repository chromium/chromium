// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.base.FileUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

import java.io.File;

/**
 * Test suite for files WebView creates on disk. This includes HTTP cache and the cookies file.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class OnDiskFileTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {
        @Override
        public boolean needsBrowserProcessStarted() {
            // We need to control when the browser process starts, so that we can delete the
            // file-under-test before the test starts up.
            return false;
        }
    };

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpCacheIsInsideCacheDir() throws Exception {
        File webViewCacheDir = new File(InstrumentationRegistry.getInstrumentation()
                                                .getTargetContext()
                                                .getCacheDir()
                                                .getPath(),
                "WebView/Default/HTTP Cache");
        FileUtils.recursivelyDeleteFile(webViewCacheDir);

        mActivityTestRule.startBrowserProcess();
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();

        TestWebServer httpServer = null;
        try {
            httpServer = TestWebServer.start();
            final String pageUrl = "/page.html";
            final String pageHtml = "<body>Hello, World!</body>";
            final String fullPageUrl = httpServer.setResponse(pageUrl, pageHtml, null);
            mActivityTestRule.loadUrlSync(
                    awContents, contentClient.getOnPageFinishedHelper(), fullPageUrl);
            Assert.assertEquals(1, httpServer.getRequestCount(pageUrl));
        } finally {
            if (httpServer != null) {
                httpServer.shutdown();
            }
        }

        Assert.assertTrue(webViewCacheDir.isDirectory());
        Assert.assertTrue(webViewCacheDir.list().length > 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCookiePathIsInsideDataDir() {
        File webViewCookiePath = new File(InstrumentationRegistry.getInstrumentation()
                                                  .getTargetContext()
                                                  .getDir("webview", Context.MODE_PRIVATE)
                                                  .getPath(),
                "Default/Cookies");
        webViewCookiePath.delete();

        // Set a cookie and flush it to disk. This should guarantee the cookie file is created.
        final AwCookieManager cookieManager = new AwCookieManager();
        final String url = "http://www.example.com";
        cookieManager.setCookie(url, "key=value");
        cookieManager.flushCookieStore();

        Assert.assertTrue(webViewCookiePath.isFile());
    }
}
