// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the AwQuotaManagerBridge.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwQuotaManagerBridgeTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private String mOrigin;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
        mWebServer = TestWebServer.start();
        mOrigin = mWebServer.getBaseUrl();

        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAppCacheEnabled(true);
        settings.setAppCachePath("whatever");  // Enables AppCache.
    }

    @After
    public void tearDown() {
        deleteAllData();
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    private void deleteAllData() {
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> bridge.deleteAllData());
    }

    private void deleteOrigin(final String origin) {
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> bridge.deleteOrigin(origin));
    }

    private static class LongValueCallbackHelper extends CallbackHelper {
        private long mValue;

        public void notifyCalled(long value) {
            mValue = value;
            notifyCalled();
        }

        public long getValue() {
            assert getCallCount() > 0;
            return mValue;
        }
    }

    private long getQuotaForOrigin() throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();

        int callCount = callbackHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> bridge.getQuotaForOrigin("foo.com",
                        quota -> callbackHelper.notifyCalled(quota)));
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getValue();
    }

    private long getUsageForOrigin(final String origin) throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();

        int callCount = callbackHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> bridge.getUsageForOrigin(origin,
                        usage -> callbackHelper.notifyCalled(usage)));
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getValue();
    }

    private void useAppCache() throws Exception {
        final String cachedFilePath = "/foo.js";
        final String cachedFileContents = "1 + 1;";
        mWebServer.setResponse(cachedFilePath, cachedFileContents, null);

        final String manifestPath = "/foo.manifest";
        final String manifestContents = "CACHE MANIFEST\nCACHE:\n" + cachedFilePath;
        List<Pair<String, String>> manifestHeaders = new ArrayList<Pair<String, String>>();
        manifestHeaders.add(Pair.create("Content-Disposition", "text/cache-manifest"));
        mWebServer.setResponse(manifestPath, manifestContents, manifestHeaders);

        final String pagePath = "/appcache.html";
        final String pageContents = "<html manifest=\"" + manifestPath + "\">"
                + "<head><script src=\"" + cachedFilePath + "\"></script></head></html>";
        String url = mWebServer.setResponse(pagePath, pageContents, null);

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "window.applicationCache.update();");
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609977")
    public void testDeleteAllWithAppCache() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        useAppCache();
        AwActivityTestRule.pollInstrumentationThread(
                () -> getUsageForOrigin(mOrigin) > initialUsage);

        deleteAllData();
        AwActivityTestRule.pollInstrumentationThread(() -> getUsageForOrigin(mOrigin) == 0);
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609977")
    public void testDeleteOriginWithAppCache() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        useAppCache();
        AwActivityTestRule.pollInstrumentationThread(
                () -> getUsageForOrigin(mOrigin) > initialUsage);

        deleteOrigin(mOrigin);
        AwActivityTestRule.pollInstrumentationThread(() -> getUsageForOrigin(mOrigin) == 0);
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609977")
    public void testGetResultsMatch() throws Exception {
        useAppCache();
        AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();
        AwActivityTestRule.pollInstrumentationThread(
                () -> AwQuotaManagerBridgeTestUtil.getOrigins(bridge).mOrigins.length > 0);

        AwQuotaManagerBridge.Origins origins = AwQuotaManagerBridgeTestUtil.getOrigins(bridge);
        Assert.assertEquals(origins.mOrigins.length, origins.mUsages.length);
        Assert.assertEquals(origins.mOrigins.length, origins.mQuotas.length);

        for (int i = 0; i < origins.mOrigins.length; ++i) {
            Assert.assertEquals(origins.mUsages[i], getUsageForOrigin(origins.mOrigins[i]));
            Assert.assertEquals(origins.mQuotas[i], getQuotaForOrigin());
        }
    }
}
