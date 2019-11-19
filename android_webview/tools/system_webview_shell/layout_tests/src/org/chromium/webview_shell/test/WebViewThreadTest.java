// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Intent;
import android.support.test.filters.SmallTest;
import android.support.test.rule.ActivityTestRule;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.WebStorage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.webview_shell.WebViewThreadTestActivity;

/**
 * Tests running WebView on different threads.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewThreadTest {
    private static final long TIMEOUT = scaleTimeout(4000);
    private static final String DATA = "<html><body>Testing<script>"
            + "console.log(\"testing\")</script></body></html>";
    private static final String URL_DATA = "javascript:console.log(\"testing\")";
    private WebViewThreadTestActivity mActivity;

    @Rule
    public ActivityTestRule<WebViewThreadTestActivity> mActivityTestRule =
            new ActivityTestRule<>(WebViewThreadTestActivity.class, false, false);

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchActivity(new Intent());
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    /**
     * Create webview then loadData, on non-ui thread
     */
    @Test
    @SmallTest
    public void testLoadDataNonUiThread() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        Assert.assertTrue(loadDataWebViewNonUiThread(DATA));
    }

    /**
     * Create webview then loadUrl, on non-ui thread
     */
    @Test
    @SmallTest
    public void testLoadUrlNonUiThread() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        Assert.assertTrue(loadUrlWebViewNonUiThread(URL_DATA));
    }

    /**
     * Run getWebViewDatabase on a non-ui thread before creating webview on ui thread
     */
    @Test
    @SmallTest
    public void testWebViewDatabaseBeforeCreateWebView() throws InterruptedException {
        mActivity.getWebViewDatabase();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Create webview on ui-thread, then getWebViewDatabase on non-ui thread
     */
    @Test
    @SmallTest
    public void testWebViewDatabaseAfterCreateWebView() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        mActivity.getWebViewDatabase();
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Run CookieManager.getInstance on a non-ui thread before creating webview on ui thread
     */
    @Test
    @SmallTest
    public void testCookieManagerBeforeCreateWebView() throws InterruptedException {
        CookieManager.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Create webview on ui-thread, then run CookieManager.getInstance on non-ui thread
     */
    @Test
    @SmallTest
    public void testCookieManagerAfterCreateWebView() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        CookieManager.getInstance();
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }


    /**
     * Run GeolocationPermissions.getInstance on a non-ui thread before creating
     * webview on ui thread
     */
    @Test
    @SmallTest
    public void testGeolocationPermissionsBeforeCreateWebView() throws InterruptedException {
        GeolocationPermissions.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Create webview on ui-thread, then run GeolocationPermissions.getInstance on non-ui thread
     */
    @Test
    @SmallTest
    public void testGelolocationPermissionsAfterCreateWebView() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        GeolocationPermissions.getInstance();
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }


    /**
     * Run WebStorage.getInstance on a non-ui thread before creating webview on ui thread
     */
    @Test
    @SmallTest
    public void testWebStorageBeforeCreateWebView() throws InterruptedException {
        WebStorage.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }


    /**
     * Create webview on ui-thread, then run WebStorage.getInstance on non-ui thread
     */
    @Test
    @SmallTest
    public void testWebStorageAfterCreateWebView() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        WebStorage.getInstance();
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * LoadData for webview created in non-ui thread
     */
    private boolean loadDataWebViewNonUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInNonUiThread(data, "text/html", null, TIMEOUT);
    }

    /**
     * LoadUrl for webview created in non-ui thread
     */
    private boolean loadUrlWebViewNonUiThread(final String url) throws InterruptedException {
        return mActivity.loadUrlInNonUiThread(url, TIMEOUT);
    }

    /**
     * LoadData for webview created in ui thread
     */
    private boolean loadDataWebViewInUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInUiThread(data, "text/html", null, TIMEOUT);
    }
}
