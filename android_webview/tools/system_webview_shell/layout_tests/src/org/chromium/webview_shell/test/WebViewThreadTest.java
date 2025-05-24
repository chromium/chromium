// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.Process;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.WebSettings;
import android.webkit.WebStorage;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.webview_shell.WebViewThreadTestActivity;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Tests running WebView on different threads. */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewThreadTest {
    private static final long TIMEOUT = scaleTimeout(4000);
    private static final String DATA =
            "<html><body>Testing<script>" + "console.log(\"testing\")</script></body></html>";
    private static final String URL_DATA = "javascript:console.log(\"testing\")";
    private WebViewThreadTestActivity mActivity;

    @Rule
    public BaseActivityTestRule<WebViewThreadTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(WebViewThreadTestActivity.class);

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    /** Create webview then loadData, on non-ui thread */
    @Test
    @SmallTest
    public void testLoadDataNonUiThread() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        Assert.assertTrue(loadDataWebViewNonUiThread(DATA));
    }

    /** Create webview then loadUrl, on non-ui thread */
    @Test
    @SmallTest
    public void testLoadUrlNonUiThread() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnNonUiThread(TIMEOUT));
        Assert.assertTrue(loadUrlWebViewNonUiThread(URL_DATA));
    }

    /** Run getWebViewDatabase on a non-ui thread before creating webview on ui thread */
    @Test
    @SmallTest
    public void testWebViewDatabaseBeforeCreateWebView() throws InterruptedException {
        mActivity.getWebViewDatabase();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /** Create webview on ui-thread, then getWebViewDatabase on non-ui thread */
    @Test
    @SmallTest
    public void testWebViewDatabaseAfterCreateWebView() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        mActivity.getWebViewDatabase();
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /** Run CookieManager.getInstance on a non-ui thread before creating webview on ui thread */
    @Test
    @SmallTest
    public void testCookieManagerBeforeCreateWebView() throws InterruptedException {
        CookieManager.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /** Create webview on ui-thread, then run CookieManager.getInstance on non-ui thread */
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

    /** Create webview on ui-thread, then run GeolocationPermissions.getInstance on non-ui thread */
    @Test
    @SmallTest
    public void testGelolocationPermissionsAfterCreateWebView() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        GeolocationPermissions.getInstance();
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /** Run WebStorage.getInstance on a non-ui thread before creating webview on ui thread */
    @Test
    @SmallTest
    public void testWebStorageBeforeCreateWebView() throws InterruptedException {
        WebStorage.getInstance();
        mActivity.createWebViewOnUiThread(TIMEOUT);
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /** Create webview on ui-thread, then run WebStorage.getInstance on non-ui thread */
    @Test
    @SmallTest
    public void testWebStorageAfterCreateWebView() throws InterruptedException {
        Assert.assertTrue(mActivity.createWebViewOnUiThread(TIMEOUT));
        WebStorage.getInstance();
        Assert.assertTrue(loadDataWebViewInUiThread(DATA));
    }

    /**
     * Initialize webview via a threadsafe API concurrently on both background and UI thread. This
     * test attempts to catch any deadlocks that may be introduced in the Chromium WebView startup
     * code when initializing WebView via a threadsafe API. Note: If this test starts timing out,
     * there may be a deadlock somewhere.
     */
    @Test
    @SmallTest
    public void testConcurrentChromiumWebViewStartup() throws InterruptedException {
        // There's a couple of tricks here to try to make the background thread's
        // getDefaultUserAgent call run first.

        // Trick: Trigger part of webview initialization that doesn't start chromium.
        CookieManager.getInstance();

        final CountDownLatch latch = new CountDownLatch(1);
        Thread bg =
                new Thread(
                        () -> {
                            Process.setThreadPriority(Process.THREAD_PRIORITY_DISPLAY); // Trick
                            // Trick: Posting to the UI thread within this background thread to try
                            // to ensure that the background task starts executing first.
                            mActivity.runOnUiThread(
                                    () -> {
                                        try {
                                            // Wait for a portion of the test duration to allow the
                                            // bg thread to execute getDefaultUserAgent first.
                                            // Dividing by 4 so as not to timeout due to waiting.
                                            Thread.sleep(TIMEOUT / 4);
                                        } catch (InterruptedException e) {
                                            throw new RuntimeException(
                                                    "Thread.sleep() was interrupted, so we can't"
                                                            + " trust the result of this test.",
                                                    e);
                                        }
                                        WebSettings.getDefaultUserAgent(mActivity);
                                        latch.countDown();
                                    });
                            WebSettings.getDefaultUserAgent(mActivity);
                        });
        bg.start();
        Assert.assertTrue(latch.await(TIMEOUT, TimeUnit.MILLISECONDS));
        bg.join();
    }

    /** LoadData for webview created in non-ui thread */
    private boolean loadDataWebViewNonUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInNonUiThread(data, "text/html", null, TIMEOUT);
    }

    /** LoadUrl for webview created in non-ui thread */
    private boolean loadUrlWebViewNonUiThread(final String url) throws InterruptedException {
        return mActivity.loadUrlInNonUiThread(url, TIMEOUT);
    }

    /** LoadData for webview created in ui thread */
    private boolean loadDataWebViewInUiThread(final String data) throws InterruptedException {
        return mActivity.loadDataInUiThread(data, "text/html", null, TIMEOUT);
    }
}
