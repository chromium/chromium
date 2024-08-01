// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import com.google.common.util.concurrent.SettableFuture;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.client_hints.AwUserAgentMetadata;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageCommitVisibleHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.TimeUnit;

@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwBackForwardCacheTest extends AwParameterizedTest {

    private static final String TAG = "AwBackForwardCacheTest";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    private static final String INITIAL_URL = "/android_webview/test/data/verify_bfcache.html";
    private static final String FORWARD_URL = "/android_webview/test/data/verify_bfcache2.html";
    private static final String THIRD_URL = "/android_webview/test/data/green.html";

    private String mInitialUrl;
    private String mForwardUrl;
    private String mThirdUrl;

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private EmbeddedTestServer mTestServer;

    private TestPageLoadedNotifier mLoadedNotifier;

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
        mThirdUrl = mTestServer.getURL(THIRD_URL);

        // The future is for waiting until page fully loaded.
        // We use this future instead of `DidFinishLoad` since this callback
        // will not get called if a page is restored from BFCache.
        mLoadedNotifier = new TestPageLoadedNotifier();
        mLoadedNotifier.setFuture(SettableFuture.create());
        String name = "awFullyLoadedFuture";
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(mAwContents, mLoadedNotifier, name);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void navigateForward() throws Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mForwardUrl);
    }

    private void navigateBack() throws Throwable {
        navigateBackToUrl(mInitialUrl);
    }

    private void navigateBackToUrl(String url) throws Throwable {
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
        OnPageStartedHelper startHelper = mContentsClient.getOnPageStartedHelper();
        int originalCallCount = startHelper.getCallCount();
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents.getWebContents(),
                startHelper);
        Assert.assertEquals(startHelper.getUrl(), url);
        Assert.assertEquals(startHelper.getCallCount(), originalCallCount + 1);
        // Wait for the page to be fully loaded
        Assert.assertEquals(
                true, pageFullyLoadedFuture.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private void navigateForwardAndBack() throws Throwable {
        navigateForward();
        navigateBack();
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

    private String extractSimpleReasonString(String notRestoredReasons) throws Exception {
        // Remove the escape character and the beginning and trailing quotes
        notRestoredReasons = notRestoredReasons.replace("\\", "");
        notRestoredReasons = notRestoredReasons.substring(1, notRestoredReasons.length() - 1);
        JSONObject json_obj = new JSONObject(notRestoredReasons);
        return json_obj.getJSONArray("reasons").getJSONObject(0).getString("reason");
    }

    private HistogramWatcher getNotRestoredReasonsHistogramWatcher(int reason) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord(
                        "BackForwardCache.HistoryNavigationOutcome.NotRestoredReason", reason)
                .build();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewBackForwardCache"})
    public void testBFCacheEnabledWithFeatureFlag() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(false);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForwardAndBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testBFCacheWithMultiplePages() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mForwardUrl);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mThirdUrl);
        navigateBackToUrl(mForwardUrl);
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        navigateBackToUrl(mInitialUrl);
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=WebViewBackForwardCache"})
    public void testBackNavigationFollowsSettings() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForwardAndBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        mAwContents.getSettings().setBackForwardCacheEnabled(false);
        navigateForwardAndBack();
        String notRestoredReasons = getNotRestoredReasons();
        Assert.assertEquals(extractSimpleReasonString(notRestoredReasons), "masked");
        Assert.assertFalse(isPageShowPersisted());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPageEvictedWhenModifyingJSInterface() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);

        // Test adding javascript interface
        navigateForward();
        HistogramWatcher histogramWatcher =
                getNotRestoredReasonsHistogramWatcher(/*kWebViewJavaScriptObjectChanged*/ 65);
        Object testInjectedObject =
                new Object() {
                    @JavascriptInterface
                    public void mock() {}
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                mAwContents, testInjectedObject, "testInjectedObject");
        navigateBack();
        String notRestoredReasons = getNotRestoredReasons();
        Assert.assertEquals(extractSimpleReasonString(notRestoredReasons), "masked");
        Assert.assertFalse(isPageShowPersisted());
        histogramWatcher.assertExpected();

        // Test removing javascript interface
        histogramWatcher =
                getNotRestoredReasonsHistogramWatcher(/*kWebViewJavaScriptObjectChanged*/ 65);
        navigateForward();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.removeJavascriptInterface("testInjectedObject"));
        navigateBack();
        notRestoredReasons = getNotRestoredReasons();
        Assert.assertEquals(extractSimpleReasonString(notRestoredReasons), "masked");
        Assert.assertFalse(isPageShowPersisted());
        histogramWatcher.assertExpected();

        // Test BFCache can still work for future navigations
        navigateForwardAndBack();
        Assert.assertTrue(isPageShowPersisted());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPageEvictedWhenAddingWebMessageListener() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        HistogramWatcher histogramWatcher =
                getNotRestoredReasonsHistogramWatcher(/*kWebViewMessageListenerInjected*/ 66);
        navigateForward();
        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "awMessagePort", new String[] {"*"}, listener);
        navigateBack();
        String notRestoredReasons = getNotRestoredReasons();
        Assert.assertTrue(notRestoredReasons.indexOf("reasons") >= 0);
        Assert.assertFalse(isPageShowPersisted());
        histogramWatcher.assertExpected();

        // Test BFCache can still work for future navigations
        navigateForwardAndBack();
        Assert.assertTrue(isPageShowPersisted());
    }

    // TODO(crbug.com/335767367): Consider calling onPageFinished for BFCache restores.
    // For now `onPageFinished` callback will not be called. The clients
    // shall listen for the web messages for BFCache related events.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPageFinishEventNotCalled() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForward();
        final OnPageFinishedHelper finishHelper = mContentsClient.getOnPageFinishedHelper();
        int originalCallCount = finishHelper.getCallCount();
        navigateBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        Assert.assertEquals(finishHelper.getCallCount(), originalCallCount);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testShouldInterceptRequestNotCalled() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForward();
        final TestAwContentsClient.ShouldInterceptRequestHelper helper =
                mContentsClient.getShouldInterceptRequestHelper();
        int originalCallCount = helper.getCallCount();
        navigateBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        Assert.assertEquals(helper.getCallCount(), originalCallCount);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testShouldOverrideUrlLoadingNotCalled() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForward();
        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper helper =
                mContentsClient.getShouldOverrideUrlLoadingHelper();
        int originalCallCount = helper.getCallCount();
        navigateBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        Assert.assertEquals(helper.getCallCount(), originalCallCount);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testOnLoadResourceNotCalled() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForward();
        final TestAwContentsClient.OnLoadResourceHelper helper =
                mContentsClient.getOnLoadResourceHelper();
        int originalCallCount = helper.getCallCount();
        navigateBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        Assert.assertEquals(helper.getCallCount(), originalCallCount);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testManualFlushCache() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        HistogramWatcher histogramWatcher =
                getNotRestoredReasonsHistogramWatcher(/*kCacheFlushed*/ 21);
        navigateForward();
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.flushBackForwardCache());
        navigateBack();
        String notRestoredReasons = getNotRestoredReasons();
        Assert.assertTrue(notRestoredReasons.indexOf("reasons") >= 0);
        Assert.assertFalse(isPageShowPersisted());
        histogramWatcher.assertExpected();

        // Test BFCache can still work for future navigations
        navigateForwardAndBack();
        Assert.assertTrue(isPageShowPersisted());
    }

    private void verifyPageEvictedWithSettingsChange(Runnable r) throws Exception, Throwable {
        HistogramWatcher histogramWatcher =
                getNotRestoredReasonsHistogramWatcher(/*kWebViewSettingsChanged*/ 64);
        navigateForward();
        r.run();
        // wait for the page finished callback to avoid interfering with the next forward
        // navigation.
        final OnPageFinishedHelper finishHelper = mContentsClient.getOnPageFinishedHelper();
        int callCount = finishHelper.getCallCount();
        navigateBack();
        finishHelper.waitForCallback(callCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertFalse(isPageShowPersisted());
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPageEvictedWhenSettingsChanged() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        // Set some options before the test to ensure changes are triggered.
        AwSettings settings = mAwContents.getSettings();
        settings.setSafeBrowsingEnabled(false);
        settings.setAllowContentAccess(false);
        settings.setCSSHexAlphaColorEnabled(false);
        settings.setScrollTopLeftInteropEnabled(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
        settings.setAttributionBehavior(AwSettings.ATTRIBUTION_DISABLED);
        settings.setForceDarkMode(AwSettings.FORCE_DARK_OFF);
        settings.setForceDarkBehavior(AwSettings.FORCE_DARK_ONLY);
        settings.setShouldFocusFirstNode(true);
        settings.setSpatialNavigationEnabled(false);
        settings.setEnableSupportedHardwareAcceleratedFeatures(false);
        settings.setFullscreenSupported(false);
        settings.setGeolocationEnabled(false);
        settings.setBlockSpecialFileUrls(false);
        settings.setDisabledActionModeMenuItems(WebSettings.MENU_ITEM_NONE);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setBlockNetworkLoads(true);
                    settings.setBlockNetworkLoads(false);
                });
        // Test not restored reasons only for the first navigation
        String notRestoredReasons = getNotRestoredReasons();
        Assert.assertTrue(notRestoredReasons.indexOf("reasons") >= 0);
        verifyPageEvictedWithSettingsChange(() -> settings.setAcceptThirdPartyCookies(false));
        verifyPageEvictedWithSettingsChange(() -> settings.setSafeBrowsingEnabled(true));
        verifyPageEvictedWithSettingsChange(() -> settings.setAllowFileAccess(true));
        verifyPageEvictedWithSettingsChange(() -> settings.setAllowContentAccess(true));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setCacheMode(WebSettings.LOAD_CACHE_ELSE_NETWORK));
        verifyPageEvictedWithSettingsChange(() -> settings.setShouldFocusFirstNode(false));
        verifyPageEvictedWithSettingsChange(() -> settings.setInitialPageScale(50));
        verifyPageEvictedWithSettingsChange(() -> settings.setSpatialNavigationEnabled(true));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setEnableSupportedHardwareAcceleratedFeatures(true));
        verifyPageEvictedWithSettingsChange(() -> settings.setFullscreenSupported(true));
        verifyPageEvictedWithSettingsChange(() -> settings.setGeolocationEnabled(true));
        verifyPageEvictedWithSettingsChange(() -> settings.setUserAgentString("testUserAgent"));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setUserAgentMetadataFromMap(
                            Map.of(AwUserAgentMetadata.MetadataKeys.PLATFORM, "fake_platform"));
                });
        verifyPageEvictedWithSettingsChange(
                () -> settings.setLoadWithOverviewMode(!settings.getLoadWithOverviewMode()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setTextZoom(settings.getTextZoom() + 100));
        verifyPageEvictedWithSettingsChange(() -> settings.setFixedFontFamily("cursive"));
        verifyPageEvictedWithSettingsChange(() -> settings.setSansSerifFontFamily("cursive"));
        verifyPageEvictedWithSettingsChange(() -> settings.setSerifFontFamily("cursive"));
        verifyPageEvictedWithSettingsChange(() -> settings.setCursiveFontFamily("serif"));
        verifyPageEvictedWithSettingsChange(() -> settings.setFantasyFontFamily("cursive"));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setMinimumFontSize(settings.getMinimumFontSize() + 1));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setMinimumLogicalFontSize(settings.getMinimumLogicalFontSize() + 1);
                });
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setDefaultFontSize(settings.getDefaultFontSize() + 1);
                });
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setDefaultFixedFontSize(settings.getDefaultFixedFontSize() + 1);
                });
        // Make sure javascript is enabled when navigating back so that we can
        // receive the web message.
        settings.setJavaScriptEnabled(false);
        verifyPageEvictedWithSettingsChange(() -> settings.setJavaScriptEnabled(true));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setAllowUniversalAccessFromFileURLs(
                            !settings.getAllowUniversalAccessFromFileURLs());
                });
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setAllowFileAccessFromFileURLs(
                            !settings.getAllowFileAccessFromFileURLs());
                });
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setLoadsImagesAutomatically(!settings.getLoadsImagesAutomatically());
                });
        verifyPageEvictedWithSettingsChange(
                () -> settings.setImagesEnabled(!settings.getImagesEnabled()));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setJavaScriptCanOpenWindowsAutomatically(
                            !settings.getJavaScriptCanOpenWindowsAutomatically());
                });
        verifyPageEvictedWithSettingsChange(
                () -> settings.setLayoutAlgorithm(AwSettings.LAYOUT_ALGORITHM_SINGLE_COLUMN));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setRequestedWithHeaderOriginAllowList(null));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setSupportMultipleWindows(!settings.supportMultipleWindows()));
        verifyPageEvictedWithSettingsChange(() -> settings.setBlockSpecialFileUrls(true));
        verifyPageEvictedWithSettingsChange(() -> settings.setCSSHexAlphaColorEnabled(true));
        verifyPageEvictedWithSettingsChange(() -> settings.setScrollTopLeftInteropEnabled(true));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setUseWideViewPort(!settings.getUseWideViewPort()));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setZeroLayoutHeightDisablesViewportQuirk(
                            !settings.getZeroLayoutHeightDisablesViewportQuirk());
                });
        verifyPageEvictedWithSettingsChange(
                () -> settings.setForceZeroLayoutHeight(!settings.getForceZeroLayoutHeight()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setDomStorageEnabled(!settings.getDomStorageEnabled()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setDatabaseEnabled(!settings.getDatabaseEnabled()));
        verifyPageEvictedWithSettingsChange(() -> settings.setDefaultTextEncodingName("Latin-1"));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setMediaPlaybackRequiresUserGesture(
                            !settings.getMediaPlaybackRequiresUserGesture());
                });
        verifyPageEvictedWithSettingsChange(() -> settings.setSupportZoom(!settings.supportZoom()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setBuiltInZoomControls(!settings.getBuiltInZoomControls()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setDisplayZoomControls(!settings.getDisplayZoomControls()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setMixedContentMode(WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setAttributionBehavior(
                            AwSettings.ATTRIBUTION_WEB_SOURCE_AND_WEB_TRIGGER);
                });
        verifyPageEvictedWithSettingsChange(
                () -> settings.setForceDarkMode(AwSettings.FORCE_DARK_AUTO));
        verifyPageEvictedWithSettingsChange(
                () -> {
                    settings.setAlgorithmicDarkeningAllowed(
                            !settings.isAlgorithmicDarkeningAllowed());
                });
        verifyPageEvictedWithSettingsChange(
                () -> settings.setForceDarkBehavior(AwSettings.MEDIA_QUERY_ONLY));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setOffscreenPreRaster(!settings.getOffscreenPreRaster()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setDisabledActionModeMenuItems(WebSettings.MENU_ITEM_SHARE));
        verifyPageEvictedWithSettingsChange(() -> settings.updateAcceptLanguages());
        verifyPageEvictedWithSettingsChange(
                () -> settings.setWillSuppressErrorPage(!settings.getWillSuppressErrorPage()));
        verifyPageEvictedWithSettingsChange(
                () -> settings.setDefaultVideoPosterURL("http://test_url"));
        // Test BFCache can still work for future navigations
        navigateForwardAndBack();
        Assert.assertTrue(isPageShowPersisted());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testDoUpdateVisitedHistory() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        navigateForward();
        final TestAwContentsClient.DoUpdateVisitedHistoryHelper helper =
                mContentsClient.getDoUpdateVisitedHistoryHelper();
        int originalCallCount = helper.getCallCount();
        navigateBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        helper.waitForCallback(originalCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(helper.getUrl(), mInitialUrl);
        Assert.assertEquals(helper.getIsReload(), false);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testOnPageCommitVisible() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        final OnPageCommitVisibleHelper helper = mContentsClient.getOnPageCommitVisibleHelper();
        int originalCallCount = helper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        helper.waitForCallback(originalCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(helper.getUrl(), mInitialUrl);

        originalCallCount = helper.getCallCount();
        navigateForward();
        helper.waitForCallback(originalCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(helper.getUrl(), mForwardUrl);

        originalCallCount = helper.getCallCount();
        navigateBack();
        Assert.assertEquals("\"null\"", getNotRestoredReasons());
        Assert.assertTrue(isPageShowPersisted());
        helper.waitForCallback(originalCallCount, 1, SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(helper.getUrl(), mInitialUrl);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPageEvictedWhenSafeBrowsingAllowlistSet() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        HistogramWatcher histogramWatcher =
                getNotRestoredReasonsHistogramWatcher(/*kWebViewSafeBrowsingAllowlistChanged*/ 67);
        navigateForward();
        ArrayList<String> allowlist = new ArrayList<>();
        allowlist.add("google.com");
        SettableFuture<Boolean> allowlistSetFuture = SettableFuture.create();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AwContentsStatics.setSafeBrowsingAllowlist(
                                allowlist,
                                result -> {
                                    allowlistSetFuture.set(true);
                                }));
        Assert.assertTrue(allowlistSetFuture.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        navigateBack();
        String notRestoredReasons = getNotRestoredReasons();
        Assert.assertEquals(extractSimpleReasonString(notRestoredReasons), "masked");
        Assert.assertFalse(isPageShowPersisted());
        histogramWatcher.assertExpected();

        // Test BFCache can still work for future navigations
        navigateForwardAndBack();
        Assert.assertTrue(isPageShowPersisted());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPageEvictedWhenAddingDocumentStartJavascript() throws Exception, Throwable {
        mAwContents.getSettings().setBackForwardCacheEnabled(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mInitialUrl);
        HistogramWatcher histogramWatcher =
                getNotRestoredReasonsHistogramWatcher(
                        /*kWebViewDocumentStartJavascriptChanged */ 68);
        navigateForward();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.addDocumentStartJavaScript(
                                "console.log(\"hello world\");", new String[] {"*"}));
        navigateBack();
        String notRestoredReasons = getNotRestoredReasons();
        Assert.assertEquals(extractSimpleReasonString(notRestoredReasons), "masked");
        Assert.assertFalse(isPageShowPersisted());
        histogramWatcher.assertExpected();

        // Test BFCache can still work for future navigations
        navigateForwardAndBack();
        Assert.assertTrue(isPageShowPersisted());
    }
}
