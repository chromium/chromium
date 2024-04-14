// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeUnit;

@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwBackForwardCacheTest extends AwParameterizedTest {

    static class PageLoadedNotifier {
        @JavascriptInterface
        public void done() {
            mPageFullyLoadedFuture.set(true);
        }

        public void setFuture(SettableFuture<Boolean> future) {
            mPageFullyLoadedFuture = future;
        }

        private SettableFuture<Boolean> mPageFullyLoadedFuture;
    }
    ;

    private static final String TAG = "AwBackForwardCacheTest";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    private static final String INITIAL_URL = "/android_webview/test/data/verify_bfcache.html";
    private static final String FORWARD_URL = "/android_webview/test/data/green.html";

    private String mInitialUrl;
    private String mForwardUrl;

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private EmbeddedTestServer mTestServer;

    private PageLoadedNotifier mLoadedNotifier;

    public AwBackForwardCacheTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);

        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        mInitialUrl = mTestServer.getURL(INITIAL_URL);
        mForwardUrl = mTestServer.getURL(FORWARD_URL);

        // The future is for waiting until page fully loaded.
        // We use this future instead of `DidFinishLoad` since this callback
        // will not get called if a page is restored from BFCache.
        mLoadedNotifier = new PageLoadedNotifier();
        mLoadedNotifier.setFuture(SettableFuture.create());
        String name = "awFullyLoadedFuture";
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(mAwContents, mLoadedNotifier, name);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void navigateForwardAndBack() throws Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mForwardUrl);

        // Create a new future to avoid the future set in the initial load.
        SettableFuture<Boolean> pageFullyLoadedFuture = SettableFuture.create();
        mLoadedNotifier.setFuture(pageFullyLoadedFuture);
        // Traditionally we use onPageFinishedHelper which is no longer
        // valid with BFCache working.
        // The onPageFinishedHelper is called in `DidFinishLoad` callback
        // in the web contents observer. If the page is restored from the
        // BFCache, this function will not get called since the onload event
        // is already fired when the page was navigated into for the first time.
        // We use onPageStartedHelper instead. This function correspond to
        // `didFinishNavigationInPrimaryMainFrame`.
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents.getWebContents(),
                mContentsClient.getOnPageStartedHelper());
        // Wait for the page to be fully loaded
        Assert.assertEquals(
                true, pageFullyLoadedFuture.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private boolean isPageShowPersisted() throws Exception {
        String isPersisted =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "isPageShowPersisted");
        return isPersisted.equals("true");
    }

    private String getNotRestoredReasons() throws Exception {
        // https://github.com/WICG/bfcache-not-restored-reason/blob/main/NotRestoredReason.md
        // If a page is not restored from the BFCache. The notRestoredReasons will contain a
        // detailed description about the reason. Otherwise it will be null (i.e. it's
        // restored from the BFCache).
        return mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents,
                mContentsClient,
                "JSON.stringify(performance.getEntriesByType('navigation')[0].notRestoredReasons);");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=WebViewBackForwardCache"
    }) // TODO: replace with AwFeatures
    public void testBackNavigationUsesBFCache() throws Exception, Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForwardAndBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "disable-features=WebViewBackForwardCache"
    }) // TODO: replace with AwFeatures
    public void testBackNavigationFollowsFeatureFlags() throws Exception, Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForwardAndBack();
        String not_restored_reasons = getNotRestoredReasons();
        Assert.assertTrue(not_restored_reasons.indexOf("reasons") >= 0);
        Assert.assertFalse(isPageShowPersisted());
    }

    // TODO: Add more cases (e.g. page eviction when in BFCache) to test
}
