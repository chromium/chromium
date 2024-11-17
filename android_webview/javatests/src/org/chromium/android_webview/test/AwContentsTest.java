// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;
import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;
import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.annotation.SuppressLint;
import android.content.ComponentCallbacks2;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableMap;
import com.google.common.util.concurrent.SettableFuture;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.android_webview.test.TestAwContentsClient.OnDownloadStartHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.BaseFeatures;
import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.RenderProcessHostUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;

import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BiFunction;
import java.util.function.Predicate;

/** AwContents tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwContentsTest extends AwParameterizedTest {
    private static final String TAG = "AwContentsTest";

    @Rule public AwActivityTestRule mActivityTestRule;

    public AwContentsTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    // Allow specific tests to use vulkan.
                    @Override
                    public boolean needsBrowserProcessStarted() {
                        return false;
                    }
                };
    }

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateDestroy() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        // NOTE this test runs on UI thread, so we cannot call any async methods.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .createAwTestContainerView(mContentsClient)
                                .getAwContents()
                                .destroy());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateLoadPageDestroy() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mActivityTestRule.loadDataSync(
                awTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.ABOUT_HTML,
                "text/html",
                false);

        mActivityTestRule.destroyAwContentsOnMainSync(awTestContainerView.getAwContents());
        // It should be safe to call destroy multiple times.
        mActivityTestRule.destroyAwContentsOnMainSync(awTestContainerView.getAwContents());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testCreateLoadDestroyManyTimes() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        for (int i = 0; i < 10; ++i) {
            AwTestContainerView testView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
            AwContents awContents = testView.getAwContents();

            mActivityTestRule.loadUrlSync(
                    awContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
            mActivityTestRule.destroyAwContentsOnMainSync(awContents);
        }
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testCreateLoadDestroyManyAtOnce() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView[] views = new AwTestContainerView[10];

        for (int i = 0; i < views.length; ++i) {
            views[i] = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
            mActivityTestRule.loadUrlSync(
                    views[i].getAwContents(),
                    mContentsClient.getOnPageFinishedHelper(),
                    ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        }

        for (int i = 0; i < views.length; ++i) {
            mActivityTestRule.destroyAwContentsOnMainSync(views[i].getAwContents());
            views[i] = null;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWebViewApisFailGracefullyAfterDestruction() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContents awContents =
                            mActivityTestRule
                                    .createAwTestContainerView(mContentsClient)
                                    .getAwContents();
                    awContents.destroy();

                    // The documentation for WebView#destroy() reads "This method should be called
                    // after this WebView has been removed from the view system. No other methods
                    // may be called on this WebView after destroy".
                    // However, some apps do not respect that restriction so we need to ensure that
                    // we fail gracefully and do not crash when APIs are invoked after destruction.
                    // Due to the large number of APIs we only test a representative selection here.
                    awContents.clearHistory();
                    Assert.assertNull(awContents.getOriginalUrl());
                    Assert.assertNull(awContents.getNavigationHistory());
                    awContents.loadUrl("http://www.google.com");
                    awContents.findAllAsync("search");
                    Assert.assertNull(awContents.getUrl());
                    Assert.assertFalse(awContents.canGoBack());
                    awContents.disableJavascriptInterfacesInspection();
                    awContents.invokeZoomPicker();
                    awContents.onResume();
                    awContents.stopLoading();
                    awContents.onWindowVisibilityChanged(View.VISIBLE);
                    awContents.requestFocus();
                    awContents.isMultiTouchZoomSupported();
                    awContents.setOverScrollMode(View.OVER_SCROLL_NEVER);
                    awContents.pauseTimers();
                    awContents.onContainerViewScrollChanged(200, 200, 100, 100);
                    awContents.computeScroll();
                    awContents.onMeasure(100, 100);
                    awContents.onDraw(new Canvas());
                    awContents.getMostRecentProgress();
                    Assert.assertEquals(0, awContents.computeHorizontalScrollOffset());
                    Assert.assertEquals(0, awContents.getContentWidthCss());
                    awContents.onKeyUp(
                            KeyEvent.KEYCODE_BACK,
                            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUseAwSettingsAfterDestroy() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwSettings awSettings =
                mActivityTestRule.getAwSettingsOnUiThread(awTestContainerView.getAwContents());
        mActivityTestRule.loadDataSync(
                awTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.ABOUT_HTML,
                "text/html",
                false);
        mActivityTestRule.destroyAwContentsOnMainSync(awTestContainerView.getAwContents());

        // AwSettings should still be usable even after native side is destroyed.
        String newFontFamily = "serif";
        awSettings.setStandardFontFamily(newFontFamily);
        Assert.assertEquals(newFontFamily, awSettings.getStandardFontFamily());
        boolean newBlockNetworkLoads = !awSettings.getBlockNetworkLoads();
        awSettings.setBlockNetworkLoads(newBlockNetworkLoads);
        Assert.assertEquals(newBlockNetworkLoads, awSettings.getBlockNetworkLoads());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGoBackGoForwardWithoutSessionHistory() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContents awContents =
                            mActivityTestRule
                                    .createAwTestContainerView(mContentsClient)
                                    .getAwContents();

                    Assert.assertFalse(awContents.canGoBack());
                    Assert.assertFalse(awContents.canGoForward());
                    // If no back/forward entries exist, then calling these should do nothing and
                    // not crash or fail asserts.
                    awContents.goBack();
                    awContents.goForward();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testBackgroundColorInDarkMode() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContents awContents =
                            mActivityTestRule
                                    .createAwTestContainerView(mContentsClient)
                                    .getAwContents();
                    AwSettings awSettings = awContents.getSettings();

                    Assert.assertEquals(
                            awContents.getEffectiveBackgroundColorForTesting(), Color.WHITE);

                    awSettings.setForceDarkMode(AwSettings.FORCE_DARK_ON);
                    Assert.assertTrue(awSettings.isForceDarkApplied());
                    Assert.assertEquals(
                            awContents.getEffectiveBackgroundColorForTesting(), Color.BLACK);

                    awContents.setBackgroundColor(Color.RED);
                    Assert.assertEquals(
                            awContents.getEffectiveBackgroundColorForTesting(), Color.RED);

                    awContents.destroy();
                    Assert.assertEquals(
                            awContents.getEffectiveBackgroundColorForTesting(), Color.RED);
                });
    }

    private int callDocumentHasImagesSync(final AwContents awContents)
            throws Throwable, InterruptedException {
        // Set up a container to hold the result object and a semaphore to
        // make the test wait for the result.
        final AtomicInteger val = new AtomicInteger();
        final Semaphore s = new Semaphore(0);
        final Message msg =
                Message.obtain(
                        new Handler(Looper.getMainLooper()) {
                            @Override
                            public void handleMessage(Message msg) {
                                val.set(msg.arg1);
                                s.release();
                            }
                        });
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> awContents.documentHasImages(msg));
        Assert.assertTrue(
                s.tryAcquire(AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        int result = val.get();
        return result;
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDocumentHasImages() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        final CallbackHelper loadHelper = mContentsClient.getOnPageFinishedHelper();

        final String mime = "text/html";
        final String emptyDoc = "<head/><body/>";
        final String imageDoc = "<head/><body><img/><img/></body>";

        // Make sure a document that does not have images returns 0
        mActivityTestRule.loadDataSync(awContents, loadHelper, emptyDoc, mime, false);
        int result = callDocumentHasImagesSync(awContents);
        Assert.assertEquals(0, result);

        // Make sure a document that does have images returns 1
        mActivityTestRule.loadDataSync(awContents, loadHelper, imageDoc, mime, false);
        result = callDocumentHasImagesSync(awContents);
        Assert.assertEquals(1, result);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setCacheMode()")
    public void testClearCacheMemoryAndDisk() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pagePath = "/clear_cache_test.html";
            List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
            // Set Cache-Control headers to cache this request. One century should be long enough.
            headers.add(Pair.create("Cache-Control", "max-age=3153600000"));
            headers.add(Pair.create("Last-Modified", "Wed, 3 Oct 2012 00:00:00 GMT"));
            final String pageUrl =
                    webServer.setResponse(pagePath, "<html><body>foo</body></html>", headers);

            // First load to populate cache.
            mActivityTestRule.clearCacheOnUiThread(awContents, true);
            mActivityTestRule.loadUrlSync(
                    awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
            Assert.assertEquals(1, webServer.getRequestCount(pagePath));

            // Load about:blank so next load is not treated as reload by webkit and force
            // revalidate with the server.
            mActivityTestRule.loadUrlSync(
                    awContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

            // No clearCache call, so should be loaded from cache.
            mActivityTestRule.loadUrlSync(
                    awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
            Assert.assertEquals(1, webServer.getRequestCount(pagePath));

            // Same as above.
            mActivityTestRule.loadUrlSync(
                    awContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

            // Clear cache, so should hit server again.
            mActivityTestRule.clearCacheOnUiThread(awContents, true);
            mActivityTestRule.loadUrlSync(
                    awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
            Assert.assertEquals(2, webServer.getRequestCount(pagePath));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testClearCacheInQuickSuccession() {
        mActivityTestRule.startBrowserProcess();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(new TestAwContentsClient());
        final AwContents awContents = testContainer.getAwContents();

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            for (int i = 0; i < 10; ++i) {
                                awContents.clearCache(true);
                            }
                        });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetFavicon() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwContents.setShouldDownloadFavicons();
        final AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        TestWebServer webServer = TestWebServer.start();
        try {
            final String faviconUrl =
                    webServer.setResponseBase64(
                            "/" + CommonResources.FAVICON_FILENAME,
                            CommonResources.FAVICON_DATA_BASE64,
                            CommonResources.getImagePngHeaders(false));
            final String pageUrl =
                    webServer.setResponse(
                            "/favicon.html", CommonResources.FAVICON_STATIC_HTML, null);

            // The getFavicon will return the right icon a certain time after
            // the page load completes which makes it slightly hard to test.
            final Bitmap defaultFavicon = awContents.getFavicon();

            mActivityTestRule.getAwSettingsOnUiThread(awContents).setImagesEnabled(true);
            mActivityTestRule.loadUrlSync(
                    awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

            mActivityTestRule.pollUiThread(
                    () ->
                            awContents.getFavicon() != null
                                    && !awContents.getFavicon().sameAs(defaultFavicon));

            final Object originalFaviconSource = new URL(faviconUrl).getContent();
            final Bitmap originalFavicon =
                    BitmapFactory.decodeStream((InputStream) originalFaviconSource);
            Assert.assertNotNull(originalFavicon);

            Assert.assertTrue(awContents.getFavicon().sameAs(originalFavicon));

        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @Feature({"AndroidWebView", "Downloads"})
    @SmallTest
    @SkipMutations(reason = "This test depends on AwSettings.setUserAgentString()")
    public void testDownload() throws Throwable {
        downloadAndCheck(null);
    }

    @Test
    @Feature({"AndroidWebView", "Downloads"})
    @SmallTest
    public void testDownloadWithCustomUserAgent() throws Throwable {
        downloadAndCheck("Custom User Agent");
    }

    private void downloadAndCheck(String customUserAgent) throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        if (customUserAgent != null) {
            AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
            awSettings.setUserAgentString(customUserAgent);
        }

        final String data = "download data";
        final String contentDisposition = "attachment;filename=\"download.txt\"";
        final String mimeType = "text/plain";

        List<Pair<String, String>> downloadHeaders = new ArrayList<Pair<String, String>>();
        downloadHeaders.add(Pair.create("Content-Disposition", contentDisposition));
        downloadHeaders.add(Pair.create("Content-Type", mimeType));
        downloadHeaders.add(Pair.create("Content-Length", Integer.toString(data.length())));

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse("/download.txt", data, downloadHeaders);
            final OnDownloadStartHelper downloadStartHelper =
                    mContentsClient.getOnDownloadStartHelper();
            final int callCount = downloadStartHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(awContents, pageUrl);
            downloadStartHelper.waitForCallback(callCount);

            Assert.assertEquals(pageUrl, downloadStartHelper.getUrl());
            Assert.assertEquals(contentDisposition, downloadStartHelper.getContentDisposition());
            Assert.assertEquals(mimeType, downloadStartHelper.getMimeType());
            Assert.assertEquals(data.length(), downloadStartHelper.getContentLength());
            Assert.assertFalse(downloadStartHelper.getUserAgent().isEmpty());
            if (customUserAgent != null) {
                Assert.assertEquals(customUserAgent, downloadStartHelper.getUserAgent());
            } else {
                Assert.assertEquals(
                        downloadStartHelper.getUserAgent(), AwSettings.getDefaultUserAgent());
            }
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @Feature({"AndroidWebView", "setNetworkAvailable"})
    @SmallTest
    public void testSetNetworkAvailable() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        String script = "navigator.onLine";

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Default to "online".
        Assert.assertEquals(
                "true",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, script));

        // Forcing "offline".
        AwActivityTestRule.setNetworkAvailableOnUiThread(awContents, false);
        Assert.assertEquals(
                "false",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, script));

        // Forcing "online".
        AwActivityTestRule.setNetworkAvailableOnUiThread(awContents, true);
        Assert.assertEquals(
                "true",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, script));
    }

    static class JavaScriptObject {

        private CallbackHelper mCallbackHelper;

        public JavaScriptObject(CallbackHelper callbackHelper) {
            mCallbackHelper = callbackHelper;
        }

        @JavascriptInterface
        public void run() {
            mCallbackHelper.notifyCalled();
        }
    }

    @Test
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @SmallTest
    public void testJavaBridge() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final CallbackHelper callback = new CallbackHelper();

        AwContents awContents = testView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                awContents, new JavaScriptObject(callback), "bridge");
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                awContents, mContentsClient, "window.bridge.run();");
        callback.waitForCallback(0, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testEscapingOfErrorPage() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        String script = "window.failed == true";

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(
                awContents,
                "file:///file-that-does-not-exist#<script>window.failed = true;</script>");
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        Assert.assertEquals(
                "false",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, script));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCanInjectHeaders() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        String url = testServer.getURL("/echoheader?X-foo");
        final Map<String, String> extraHeaders = new HashMap<String, String>();
        extraHeaders.put("X-foo", "bar");
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), url, extraHeaders);
        String xfoo =
                mActivityTestRule.getJavaScriptResultBodyTextContent(awContents, mContentsClient);
        Assert.assertEquals("bar", xfoo);
        url = testServer.getURL("/echoheader?Referer");
        mActivityTestRule.loadUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                url,
                ImmutableMap.of("Referer", "http://www.example.com/"));
        String referer =
                mActivityTestRule.getJavaScriptResultBodyTextContent(awContents, mContentsClient);
        Assert.assertEquals("http://www.example.com/", referer);
    }

    // This is a meta test that we don't accidentally turn off hardware
    // acceleration in instrumentation tests without notice. Do not add the
    // @DisableHardwareAcceleration annotation for this test.
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testHardwareModeWorks() {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        Assert.assertTrue(testContainer.isHardwareAccelerated());
        Assert.assertTrue(testContainer.isBackedByHardwareView());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testBasicCookieFunctionality() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        TestWebServer webServer = TestWebServer.start();
        try {
            List<Pair<String, String>> responseHeaders = CommonResources.getTextHtmlHeaders(true);
            final String cookie = "key=value";
            responseHeaders.add(Pair.create("Set-Cookie", cookie));
            final String url =
                    webServer.setResponse(
                            "/" + CommonResources.ABOUT_FILENAME,
                            CommonResources.ABOUT_HTML,
                            responseHeaders);
            AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
            mActivityTestRule.loadUrlSync(
                    awContents, mContentsClient.getOnPageFinishedHelper(), url);

            final String script = "document.cookie";
            Assert.assertEquals(
                    "\"key=value\"",
                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                            awContents, mContentsClient, script));
        } finally {
            webServer.shutdown();
        }
    }

    /** Verifies that Web Notifications and the Push API are not exposed in WebView. */
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPushAndNotificationsDisabled() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        String script = "window.Notification || window.PushManager";

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents, mContentsClient, script));
    }

    private @RendererPriority int getRendererPriorityOnUiThread(final AwContents awContents)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getEffectivePriorityForTesting());
    }

    private void setRendererPriorityOnUiThread(
            final AwContents awContents,
            final @RendererPriority int priority,
            final boolean waivedWhenNotVisible)
            throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.setRendererPriorityPolicy(priority, waivedWhenNotVisible));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add(ContentSwitches.RENDER_PROCESS_LIMIT + "=1")
    public void testForegroundPriorityOneProcess() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwTestContainerView view1 =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents contents1 = view1.getAwContents();
        final AwTestContainerView view2 =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents contents2 = view2.getAwContents();

        mActivityTestRule.loadUrlSync(
                contents1,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        mActivityTestRule.loadUrlSync(
                contents2,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Process should start out high.
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents1));
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents2));

        // Set one to low. Process should take max priority of contents, so still high.
        setRendererPriorityOnUiThread(contents1, RendererPriority.LOW, false);
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents1));
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents2));

        // Set both to low and check.
        setRendererPriorityOnUiThread(contents2, RendererPriority.LOW, false);
        Assert.assertEquals(RendererPriority.LOW, getRendererPriorityOnUiThread(contents1));
        Assert.assertEquals(RendererPriority.LOW, getRendererPriorityOnUiThread(contents2));

        // Set both to waive and check.
        setRendererPriorityOnUiThread(contents1, RendererPriority.WAIVED, false);
        setRendererPriorityOnUiThread(contents2, RendererPriority.WAIVED, false);
        Assert.assertEquals(RendererPriority.WAIVED, getRendererPriorityOnUiThread(contents1));
        Assert.assertEquals(RendererPriority.WAIVED, getRendererPriorityOnUiThread(contents2));

        // Set one to high and check.
        setRendererPriorityOnUiThread(contents1, RendererPriority.HIGH, false);
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents1));
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents2));

        // Destroy contents with high priority, and process should fall back to low.
        // Destroy posts on UI, but getRendererPriorityOnUiThread posts after, so there should
        // be no flakiness and no need for polling.
        mActivityTestRule.destroyAwContentsOnMainSync(contents1);
        Assert.assertEquals(RendererPriority.WAIVED, getRendererPriorityOnUiThread(contents2));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add(ContentSwitches.RENDER_PROCESS_LIMIT + "=2")
    public void testForegroundPriorityTwoProcesses() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwTestContainerView view1 =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents contents1 = view1.getAwContents();
        final AwTestContainerView view2 =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents contents2 = view2.getAwContents();

        mActivityTestRule.loadUrlSync(
                contents1,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        mActivityTestRule.loadUrlSync(
                contents2,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Process should start out high.
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents1));
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents2));

        // Set one to low. Other should not be affected.
        setRendererPriorityOnUiThread(contents1, RendererPriority.LOW, false);
        Assert.assertEquals(RendererPriority.LOW, getRendererPriorityOnUiThread(contents1));
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(contents2));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testBackgroundPriority() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwContents awContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(awContents));

        ThreadUtils.runOnUiThreadBlocking(() -> awContents.onPause());
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(awContents));

        setRendererPriorityOnUiThread(
                awContents, RendererPriority.HIGH, /* waivedWhenNotVisible= */ true);
        Assert.assertEquals(RendererPriority.WAIVED, getRendererPriorityOnUiThread(awContents));

        ThreadUtils.runOnUiThreadBlocking(() -> awContents.onResume());
        Assert.assertEquals(RendererPriority.HIGH, getRendererPriorityOnUiThread(awContents));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testPauseDestroyResume() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContents awContents;
                    awContents =
                            mActivityTestRule
                                    .createAwTestContainerView(mContentsClient)
                                    .getAwContents();
                    awContents.pauseTimers();
                    awContents.pauseTimers();
                    awContents.destroy();
                    awContents =
                            mActivityTestRule
                                    .createAwTestContainerView(mContentsClient)
                                    .getAwContents();
                    awContents.resumeTimers();
                });
    }

    private AwRenderProcess getRenderProcessOnUiThread(final AwContents awContents)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getRenderProcess());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testRenderProcessInMultiProcessMode() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        final AwRenderProcess preLoadRenderProcess = getRenderProcessOnUiThread(awContents);
        Assert.assertNotNull(preLoadRenderProcess);

        mActivityTestRule.loadUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        final AwRenderProcess postLoadRenderProcess = getRenderProcessOnUiThread(awContents);
        Assert.assertEquals(preLoadRenderProcess, postLoadRenderProcess);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(SINGLE_PROCESS)
    public void testNoRenderProcessInSingleProcessMode() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        mActivityTestRule.loadUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        final AwRenderProcess renderProcess = getRenderProcessOnUiThread(awContents);
        Assert.assertEquals(renderProcess, null);
    }

    /**
     * Regression test for https://crbug.com/732976. Load a data URL, then immediately after that
     * load a javascript URL. The data URL navigation shouldn't be blocked.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testJavaScriptUrlAfterLoadData() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Run javascript navigation immediately, without waiting for the completion of
                    // data URL.
                    awContents.loadData("<html>test</html>", "text/html", "utf-8");
                    awContents.loadUrl("javascript: void(0)");
                });

        mContentsClient
                .getOnPageFinishedHelper()
                .waitForCallback(0, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals("data:text/html,<html>test</html>", awContents.getLastCommittedUrl());

        TestAwContentsClient.AddMessageToConsoleHelper consoleHelper =
                mContentsClient.getAddMessageToConsoleHelper();
        Assert.assertEquals(0, consoleHelper.getMessages().size());
    }

    /**
     * Regression test for https://crbug.com/1226748. Call stopLoading() before any page has been
     * loaded, load a page, and then load a JavaScript URL. The JavaScript URL should execute.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testJavaScriptUrlAfterStopLoading() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        // It should always be safe to call stopLoading() even if we haven't loaded anything yet.
        mActivityTestRule.stopLoading(awContents);
        mActivityTestRule.loadUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadUrlAsync(awContents, "javascript:location.reload()");

        // Wait for the page to reload and trigger another onPageFinished()
        mContentsClient
                .getOnPageFinishedHelper()
                .waitForCallback(0, 2, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /**
     * Regression test for https://crbug.com/1231883. Call stopLoading() before any page has been
     * loaded, load a page, and then call evaluateJavaScript. The JavaScript code should execute.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testEvaluateJavaScriptAfterStopLoading() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        // It should always be safe to call stopLoading() even if we haven't loaded anything yet.
        mActivityTestRule.stopLoading(awContents);
        mActivityTestRule.loadUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // We specifically call AwContents.evaluateJavaScript() rather than the
                    // AwActivityTestRule helper methods to make sure we're using the same code path
                    // as production.
                    awContents.evaluateJavaScript("location.reload()", null);
                });

        // Wait for the page to reload and trigger another onPageFinished()
        mContentsClient
                .getOnPageFinishedHelper()
                .waitForCallback(0, 2, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        // Verify the callback actually contains the execution result.
        final SettableFuture<String> jsResult = SettableFuture.create();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // We specifically call AwContents.evaluateJavaScript() rather than the
                    // AwActivityTestRule helper methods to make sure we're using the same code path
                    // as production.
                    awContents.evaluateJavaScript("1 + 2", jsResult::set);
                });
        Assert.assertEquals(
                "JavaScript expression result should be correct",
                "3",
                AwActivityTestRule.waitForFuture(jsResult));
    }

    /**
     * Regression test for https://crbug.com/1145717. Load a URL that requires fixing and verify
     * that the legacy behavior is preserved (i.e. that the URL is fixed + that no crashes happen in
     * the product).
     *
     * <p>The main test verification is that there are no crashes. In particular, this test tries to
     * verify that the `loadUrl` call above won't trigger:
     * <li>NOTREACHED and DwoC in content::NavigationRequest's constructor for about: scheme
     *     navigations that aren't about:blank nor about:srcdoc
     * <li>CHECK in content::NavigationRequest::GetOriginForURLLoaderFactory caused by the mismatch
     *     between the result of this method and the "about:" process lock.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testLoadUrlAboutVersion() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // "about:safe-browsing" will be rewritten by
                    // components.url_formatter.UrlFormatter.fixupUrl into
                    // "chrome://safe-browsing/".
                    //
                    // Note that chrome://safe-browsing/ is one of very few chrome://... URLs that
                    // work in Android WebView.  In particular, chrome://version/ wouldn't work.
                    awContents.loadUrl("about:safe-browsing");
                });

        mContentsClient
                .getOnPageFinishedHelper()
                .waitForCallback(0, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals("chrome://safe-browsing/", awContents.getLastCommittedUrl());
    }

    private void doHardwareRenderingSmokeTest() throws Throwable {
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        doHardwareRenderingSmokeTest(testView);
    }

    private void doHardwareRenderingSmokeTest(AwTestContainerView testView) throws Throwable {
        doHardwareRenderingSmokeTest(testView, 128, 128, 128);
    }

    private void doHardwareRenderingSmokeTest(AwTestContainerView testView, int r, int g, int b)
            throws Throwable {
        String html =
                String.format(
                        "<html>"
                                + "  <body style=\""
                                + "       padding: 0;"
                                + "       margin: 0;"
                                + "       display: grid;"
                                + "       display: grid;"
                                + "       grid-template-columns: 50%% 50%%;"
                                + "       grid-template-rows: 50%% 50%%;\">"
                                + "   <div style=\"background-color: rgb(255, 0, 0);\"></div>"
                                + "   <div style=\"background-color: rgb(0, 255, 0);\"></div>"
                                + "   <div style=\"background-color: rgb(0, 0, 255);\"></div>"
                                + "   <div style=\"background-color: rgb(%d, %d, %d);\"></div>"
                                + "  </body>"
                                + "</html>",
                        r, g, b);
        mActivityTestRule.loadDataSync(
                testView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false);
        mActivityTestRule.waitForVisualStateCallback(testView.getAwContents());

        int[] expectedQuadrantColors = {
            Color.rgb(255, 0, 0), Color.rgb(0, 255, 0), Color.rgb(0, 0, 255), Color.rgb(r, g, b)
        };

        GraphicsTestUtils.pollForQuadrantColors(testView, expectedQuadrantColors);
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testHardwareRenderingSmokeTest() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        doHardwareRenderingSmokeTest();
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testHardwareRenderingSmokeTestVulkanWhereSupported() throws Throwable {
        // Manually curated list.
        final String[] supportedModels = {
            "Pixel", "Pixel 2", "Pixel 3", "Pixel 4a",
        };
        if (!Arrays.asList(supportedModels).contains(Build.MODEL)) {
            Log.w(TAG, "Skipping vulkan test on unknown device: " + Build.MODEL);
            return;
        }
        mActivityTestRule.startBrowserProcessWithVulkan();
        doHardwareRenderingSmokeTest();
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testFixupOctothorpesInLoadDataContent() {
        mActivityTestRule.startBrowserProcess();
        // If there are no octothorpes the function should have no effect.
        final String noOctothorpeString = "<div id='foo1'>This content has no octothorpe</div>";
        Assert.assertEquals(
                noOctothorpeString,
                AwContents.fixupOctothorpesInLoadDataContent(noOctothorpeString));

        // One '#' followed by a valid DOM id requires us to duplicate it into a real fragment.
        Assert.assertEquals("abc%23A#A", AwContents.fixupOctothorpesInLoadDataContent("abc#A"));
        Assert.assertEquals("abc%23a#a", AwContents.fixupOctothorpesInLoadDataContent("abc#a"));
        Assert.assertEquals("abc%23Aa#Aa", AwContents.fixupOctothorpesInLoadDataContent("abc#Aa"));
        Assert.assertEquals("abc%23aA#aA", AwContents.fixupOctothorpesInLoadDataContent("abc#aA"));
        Assert.assertEquals(
                "abc%23a1-_:.#a1-_:.", AwContents.fixupOctothorpesInLoadDataContent("abc#a1-_:."));

        // One '#' followed by an invalid DOM id just means we encode the '#'.
        Assert.assertEquals("abc%231", AwContents.fixupOctothorpesInLoadDataContent("abc#1"));
        Assert.assertEquals("abc%231a", AwContents.fixupOctothorpesInLoadDataContent("abc#1a"));
        Assert.assertEquals(
                "abc%23not valid", AwContents.fixupOctothorpesInLoadDataContent("abc#not valid"));
        Assert.assertEquals("abc%23a@", AwContents.fixupOctothorpesInLoadDataContent("abc#a@"));

        // Multiple '#', whether or not they have a valid DOM id afterwards, just means we encode
        // the '#'.
        Assert.assertEquals("abc%23%23a", AwContents.fixupOctothorpesInLoadDataContent("abc##a"));
        Assert.assertEquals("abc%23a%23b", AwContents.fixupOctothorpesInLoadDataContent("abc#a#b"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testLoadDataOctothorpeHandling() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        // Before Android Q, the loadData API is expected to handle the encoding for users.
        boolean encodeOctothorpes =
                ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                        < Build.VERSION_CODES.Q;

        // A URL with no '#' character.
        mActivityTestRule.loadDataSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html>test</html>",
                "text/html",
                false);
        Assert.assertEquals("data:text/html,<html>test</html>", awContents.getLastCommittedUrl());

        // A URL with one '#' character.
        mActivityTestRule.loadDataSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html>test#foo</html>",
                "text/html",
                false);
        String expectedUrl =
                encodeOctothorpes
                        ? "data:text/html,<html>test%23foo</html>"
                        : "data:text/html,<html>test#foo%3C/html%3E";
        Assert.assertEquals(expectedUrl, awContents.getLastCommittedUrl());

        // A URL with many '#' characters.
        mActivityTestRule.loadDataSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html>test#foo#bar#</html>",
                "text/html",
                false);
        expectedUrl =
                encodeOctothorpes
                        ? "data:text/html,<html>test%23foo%23bar%23</html>"
                        : "data:text/html,<html>test#foo#bar#%3C/html%3E";
        Assert.assertEquals(expectedUrl, awContents.getLastCommittedUrl());

        // An already encoded '#' character.
        mActivityTestRule.loadDataSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                "<html>test%23foo</html>",
                "text/html",
                false);
        Assert.assertEquals(
                "data:text/html,<html>test%23foo</html>", awContents.getLastCommittedUrl());

        // A URL with a valid fragment. Before Q, this must be manipulated so that it renders the
        // same and still scrolls to the fragment location.
        if (encodeOctothorpes) {
            String contents = "<div style='height: 5000px'></div><a id='target'>Target</a>#target";
            mActivityTestRule.loadDataSync(
                    awContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    contents,
                    "text/html",
                    false);
            Assert.assertEquals(
                    "data:text/html,<div style='height: 5000px'></div><a id='target'>Target</a>"
                            + "%23target#target",
                    awContents.getLastCommittedUrl());
            // TODO(smcgruer): I can physically see that this has scrolled on the test page, and
            // have traced scrolling through PaintLayerScrollableArea, but I don't know how to check
            // it.
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testLoadsJsModule() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);

        // This test is specifically about relative file urls
        awSettings.setAllowFileAccess(true);
        awSettings.setAllowFileAccessFromFileURLs(true);

        // This test runs some javascript to verify if it passes
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        // Using a future to wait to see if the js module was loaded or not.
        // The page in the test will expect this object.
        final SettableFuture<Boolean> fetchResultFuture = SettableFuture.create();
        Object injectedObject =
                new Object() {
                    @JavascriptInterface
                    public void success() {
                        fetchResultFuture.set(true);
                    }

                    @JavascriptInterface
                    public void error() {
                        fetchResultFuture.set(false);
                    }
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                awContents, injectedObject, "injectedObject");

        final String url = "file:///android_asset/page_with_module.html";
        mActivityTestRule.loadUrlAsync(awContents, url);

        Assert.assertTrue(AwActivityTestRule.waitForFuture(fetchResultFuture));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testLoadUrlRecordsScheme_http() {
        // No need to spin up a web server, since we don't care if the load ever succeeds.
        final String httpUrlWithNoRealPage = "http://some.origin.test/some/path.html";
        loadUrlAndCheckScheme(httpUrlWithNoRealPage, AwContents.UrlScheme.HTTP_SCHEME);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testLoadUrlRecordsScheme_javascript() {
        loadUrlAndCheckScheme(
                "javascript:console.log('message')", AwContents.UrlScheme.JAVASCRIPT_SCHEME);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testLoadUrlRecordsScheme_fileAndroidAsset() {
        loadUrlAndCheckScheme(
                "file:///android_asset/some/asset/page.html",
                AwContents.UrlScheme.FILE_ANDROID_ASSET_SCHEME);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testLoadUrlRecordsScheme_fileRegular() {
        loadUrlAndCheckScheme("file:///some/path/on/disk.html", AwContents.UrlScheme.FILE_SCHEME);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testLoadUrlRecordsScheme_data() {
        loadUrlAndCheckScheme(
                "data:text/html,<html><body>foo</body></html>", AwContents.UrlScheme.DATA_SCHEME);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testLoadUrlRecordsScheme_blank() {
        loadUrlAndCheckScheme("about:blank", AwContents.UrlScheme.EMPTY);
    }

    private void loadUrlAndCheckScheme(String url, @AwContents.UrlScheme int expectedSchemeEnum) {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        AwContents.LOAD_URL_SCHEME_HISTOGRAM_NAME, expectedSchemeEnum);

        // Note: we use async because not all loads emit onPageFinished. This relies on the UMA
        // metric being logged in the synchronous part of loadUrl().
        mActivityTestRule.loadUrlAsync(awContents, url);
        histogramExpectation.assertExpected();
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testFindAllAsyncEmptySearchString() {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();
        try {
            awContents.findAllAsync(null);
            Assert.fail("A null searchString should cause an exception to be thrown");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testInsertNullVisualStateCallback() {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();
        try {
            awContents.insertVisualStateCallback(0, null);
            Assert.fail("A null VisualStateCallback should cause an exception to be thrown");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    // This test verifies that Private Network Access' secure context
    // restriction (feature flag BlockInsecurePrivateNetworkRequests) does not
    // apply to Webview: insecure private network requests are allowed.
    //
    // This is a regression test for crbug.com/1255675.
    @Test
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1")
    @SmallTest
    public void testInsecurePrivateNetworkAccess() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainer.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        // This SettableFuture and its accompanying injected object allows us to
        // synchronize on the fetch result.
        final SettableFuture<Boolean> fetchResultFuture = SettableFuture.create();
        Object injectedObject =
                new Object() {
                    @JavascriptInterface
                    public void success() {
                        fetchResultFuture.set(true);
                    }

                    @JavascriptInterface
                    public void error() {
                        fetchResultFuture.set(false);
                    }
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                awContents, injectedObject, "injectedObject");

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        // Need to avoid http://localhost, which is considered secure, so we
        // use http://foo.test, which resolves to 127.0.0.1 thanks to the
        // host resolver rules command-line flag.
        //
        // The resulting document is a non-secure context in the public IP
        // address space. If the secure context restriction were applied, it
        // would not be allowed to fetch subresources from localhost.
        String url =
                testServer.getURLWithHostName(
                        "foo.test", "/set-header?Content-Security-Policy: treat-as-public-address");

        mActivityTestRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Fetch a subresource from the same server, whose IP address is still
        // 127.0.0.1, thus belonging to the local IP address space.
        // This should succeed.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                awContents,
                mContentsClient,
                "fetch('/defaultresponse')"
                        + ".then(() => { injectedObject.success() })"
                        + ".catch((err) => { "
                        + "  console.log(err); "
                        + "  injectedObject.error(); "
                        + "})");

        Assert.assertTrue(AwActivityTestRule.waitForFuture(fetchResultFuture));
    }

    private static final String HELLO_WORLD_URL = "/android_webview/test/data/hello_world.html";
    private static final String HELLO_WORLD_TITLE = "Hello, World!";
    private static final String WEBUI_URL = "chrome://safe-browsing";
    private static final String WEBUI_TITLE = "Safe Browsing";

    // Check that we can navigate between a regular web page and a WebUI page
    // that's available on AW (chrome://safe-browsing), and that the WebUI page
    // loads in its own locked renderer process when in multi-process mode.
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testWebUIUsesDedicatedProcessInMultiProcessMode() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String pageUrl = testServer.getURL(HELLO_WORLD_URL);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(HELLO_WORLD_TITLE, mActivityTestRule.getTitleOnUiThread(awContents));
        final AwRenderProcess rendererProcess1 = getRenderProcessOnUiThread(awContents);
        Assert.assertNotNull(rendererProcess1);
        // Until AW gets site isolation, ordinary web content should not be
        // locked to origin.
        boolean isLocked =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> rendererProcess1.isProcessLockedToSiteForTesting());
        Assert.assertFalse("Initial renderer process should not be locked", isLocked);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), WEBUI_URL);
        Assert.assertEquals(WEBUI_TITLE, mActivityTestRule.getTitleOnUiThread(awContents));
        final AwRenderProcess webuiProcess = getRenderProcessOnUiThread(awContents);
        Assert.assertNotEquals(rendererProcess1, webuiProcess);
        // WebUI pages should be locked to origin even on AW.
        isLocked =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> webuiProcess.isProcessLockedToSiteForTesting());
        Assert.assertTrue("WebUI process should be locked", isLocked);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        final AwRenderProcess rendererProcess2 = getRenderProcessOnUiThread(awContents);
        Assert.assertEquals(HELLO_WORLD_TITLE, mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertNotEquals(rendererProcess2, webuiProcess);
        isLocked =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> rendererProcess2.isProcessLockedToSiteForTesting());
        Assert.assertFalse("Final renderer process should not be locked", isLocked);
    }

    // In single-process mode, navigations to WebUI should work, but WebUI does
    // not gets process-isolated.
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(SINGLE_PROCESS)
    public void testWebUILoadsWithoutProcessIsolationInSingleProcessMode() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String pageUrl = testServer.getURL(HELLO_WORLD_URL);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(HELLO_WORLD_TITLE, mActivityTestRule.getTitleOnUiThread(awContents));
        final AwRenderProcess rendererProcess1 = getRenderProcessOnUiThread(awContents);
        Assert.assertNull(rendererProcess1);
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), WEBUI_URL);
        Assert.assertEquals(WEBUI_TITLE, mActivityTestRule.getTitleOnUiThread(awContents));
        final AwRenderProcess webuiProcess = getRenderProcessOnUiThread(awContents);
        Assert.assertNull(webuiProcess);
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    @OnlyRunIn(MULTI_PROCESS)
    @CommandLineFlags.Add(ContentSwitches.SITE_PER_PROCESS)
    @DisabledTest(message = "https://crbug.com/1246585")
    public void testOutOfProcessIframeSmokeTest() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        TestWebServer webServer = TestWebServer.start();
        try {
            // Destination iframe has blue color.
            final String iframeDestinationPath =
                    webServer.setResponse(
                            "/iframe_destination.html",
                            "<html><body style=\"background-color:rgb(0,0,255);\"></body></html>",
                            null);
            // Initial iframe has red color with a full-page link to navigate to destination.
            final String iframePath =
                    webServer.setResponse(
                            "/iframe.html",
                            "<html><body style=\"background-color:rgb(255,0,0);\">"
                                    + "<a href=\""
                                    + iframeDestinationPath
                                    + "\" "
                                    + "style=\"width:100%;height:100%;display:block;\"></a>"
                                    + "</body></html>",
                            null);
            // Main frame has green color at the top half, and iframe in the bottom half.
            final String pageHtml =
                    "<html><body><div"
                        + " style=\"width:100%;height:50%;background-color:rgb(0,255,0);\"></div><iframe"
                        + " style=\"width:100%;height:50%;\" src=\""
                            + iframePath
                            + "\"></iframe>"
                            + "</body></html>";

            // Iframes are loaded with origin of the test server, and the main page is loaded with
            // origin http://foo.bar. This ensures that the main and iframe are different renderer
            // processes when site isolation is enabled.
            mActivityTestRule.loadDataWithBaseUrlSync(
                    awContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    pageHtml,
                    "text/html",
                    false,
                    "http://foo.bar",
                    null);

            // Check initial iframe is displayed.
            int[] expectedQuadrantColors = {
                Color.rgb(0, 255, 0),
                Color.rgb(0, 255, 0),
                Color.rgb(255, 0, 0),
                Color.rgb(255, 0, 0),
            };
            GraphicsTestUtils.pollForQuadrantColors(testView, expectedQuadrantColors);
            assertThat(RenderProcessHostUtils.getCurrentRenderProcessCount(), greaterThan(1));

            // Click iframe to navigate. This exercises hit testing code paths.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        int width = testView.getWidth();
                        int height = testView.getHeight();
                        TouchCommon.singleClickView(testView, width / 2, height * 3 / 4);
                    });

            // Check destination iframe is displayed.
            expectedQuadrantColors =
                    new int[] {
                        Color.rgb(0, 255, 0),
                        Color.rgb(0, 255, 0),
                        Color.rgb(0, 0, 255),
                        Color.rgb(0, 0, 255),
                    };
            GraphicsTestUtils.pollForQuadrantColors(testView, expectedQuadrantColors);
            assertThat(RenderProcessHostUtils.getCurrentRenderProcessCount(), greaterThan(1));
        } finally {
            webServer.shutdown();
        }
    }

    private class FakePostDelayedTask implements BiFunction<Runnable, Long, Void> {

        @Override
        public Void apply(Runnable runnable, Long delay) {
            long time = TimeUtils.uptimeMillis() + delay;
            mTasks.add(new Pair<Runnable, Long>(runnable, time));
            return null;
        }

        public void fastForwardBy(long delay) {
            mFakeTimeTestRule.advanceMillis(delay);
            final long now = TimeUtils.uptimeMillis();
            Predicate<Pair<Runnable, Long>> deadlinePassed =
                    (Pair<Runnable, Long> p) -> {
                        return p.second <= now;
                    };

            // Tasks running can post other tasks, do it in stages to prevent concurrent
            // modification errors.
            var toRun = new ArrayList<Pair<Runnable, Long>>();
            for (var p : mTasks) {
                if (deadlinePassed.test(p)) {
                    toRun.add(p);
                }
            }
            mTasks.removeIf(deadlinePassed);
            for (var p : toRun) {
                p.first.run();
            }
        }

        public int getPendingTasksCount() {
            return mTasks.size();
        }

        private List<Pair<Runnable, Long>> mTasks = new ArrayList<Pair<Runnable, Long>>();
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testClearDrawFunctorInBackground() throws Throwable {
        mActivityTestRule.startBrowserProcess();

        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();
        AwContents.resetRecordMemoryForTesting();

        // Load a page to ensure that at least one draw has happened.
        doHardwareRenderingSmokeTest(testView);
        Assert.assertTrue(awContents.hasDrawFunctor());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var postTask = new FakePostDelayedTask();
                    awContents.setPostDelayedTaskForTesting(postTask);
                    awContents.onWindowVisibilityChanged(View.INVISIBLE);

                    // Delayed release task.
                    Assert.assertEquals(1, postTask.getPendingTasksCount());

                    postTask.fastForwardBy(AwContents.FUNCTOR_RECLAIM_DELAY_MS);
                    // Metrics task is still pending.
                    Assert.assertEquals(1, postTask.getPendingTasksCount());
                    Assert.assertFalse(awContents.hasDrawFunctor());

                    awContents.onWindowVisibilityChanged(View.VISIBLE);
                    Assert.assertFalse(awContents.hasDrawFunctor());

                    // Metrics task will not report histograms because we went back to foreground in
                    // the meantime.
                    var histograms =
                            HistogramWatcher.newBuilder()
                                    .expectNoRecords(AwContents.PSS_HISTOGRAM)
                                    .expectNoRecords(AwContents.PRIVATE_DIRTY_HISTOGRAM)
                                    .build();
                    postTask.fastForwardBy(AwContents.METRICS_COLLECTION_DELAY_MS);
                    Assert.assertEquals(0, postTask.getPendingTasksCount());
                    histograms.assertExpected();
                });

        // Rendering still works.
        doHardwareRenderingSmokeTest(testView, 42, 42, 42);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(awContents.hasDrawFunctor());
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testClearDrawFunctorInBackgroundMultipleTransitions() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwContents.resetRecordMemoryForTesting();

        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        // Load a page to ensure that at least one draw has happened.
        doHardwareRenderingSmokeTest(testView);
        Assert.assertTrue(awContents.hasDrawFunctor());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var postTask = new FakePostDelayedTask();
                    awContents.setPostDelayedTaskForTesting(postTask);
                    awContents.onWindowVisibilityChanged(View.INVISIBLE);

                    Assert.assertEquals(1, postTask.getPendingTasksCount());

                    postTask.fastForwardBy(AwContents.FUNCTOR_RECLAIM_DELAY_MS / 2);
                    awContents.onWindowVisibilityChanged(View.VISIBLE);
                    awContents.onWindowVisibilityChanged(View.INVISIBLE);
                    Assert.assertEquals(1, postTask.getPendingTasksCount());
                    postTask.fastForwardBy(AwContents.FUNCTOR_RECLAIM_DELAY_MS / 2);

                    // Not enough continuous time in background.
                    Assert.assertTrue(awContents.hasDrawFunctor());
                    // But there is still a task pending.
                    Assert.assertEquals(1, postTask.getPendingTasksCount());

                    // Multiple transitions do not post multiple tasks.
                    awContents.onWindowVisibilityChanged(View.VISIBLE);
                    awContents.onWindowVisibilityChanged(View.INVISIBLE);
                    Assert.assertEquals(1, postTask.getPendingTasksCount());

                    // Functor is reclaimed after enough continuous time in background.
                    postTask.fastForwardBy(AwContents.FUNCTOR_RECLAIM_DELAY_MS);
                    Assert.assertFalse(awContents.hasDrawFunctor());

                    // Metrics task.
                    var histograms =
                            HistogramWatcher.newBuilder()
                                    .expectAnyRecord(AwContents.PSS_HISTOGRAM)
                                    .expectAnyRecord(AwContents.PRIVATE_DIRTY_HISTOGRAM)
                                    .build();
                    Assert.assertEquals(1, postTask.getPendingTasksCount());
                    postTask.fastForwardBy(AwContents.METRICS_COLLECTION_DELAY_MS);
                    Assert.assertEquals(0, postTask.getPendingTasksCount());
                    histograms.assertExpected();
                });

        // Not testing rendering here, because all the back and forth advanced the virtual clock too
        // much, the test would time out.
    }

    @Test
    @Feature({"AndroidWebView"})
    @MediumTest
    public void testClearFunctorOnBackgroundMemorySignal() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwContents.resetRecordMemoryForTesting();

        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        // Load a page to ensure that at least one draw has happened.
        doHardwareRenderingSmokeTest(testView);
        Assert.assertTrue(awContents.hasDrawFunctor());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var postTask = new FakePostDelayedTask();
                    awContents.setPostDelayedTaskForTesting(postTask);

                    // Not required to happen in background, but this is how the notification is
                    // dispatched in real code.
                    awContents.onWindowVisibilityChanged(View.INVISIBLE);
                    Assert.assertTrue(awContents.hasDrawFunctor());
                    Assert.assertEquals(1, postTask.getPendingTasksCount());

                    awContents.onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_BACKGROUND);
                    Assert.assertFalse(awContents.hasDrawFunctor());

                    // Metrics task.
                    var histograms =
                            HistogramWatcher.newBuilder()
                                    .expectAnyRecord(AwContents.PSS_HISTOGRAM)
                                    .expectAnyRecord(AwContents.PRIVATE_DIRTY_HISTOGRAM)
                                    .build();
                    Assert.assertEquals(2, postTask.getPendingTasksCount());
                    postTask.fastForwardBy(AwContents.METRICS_COLLECTION_DELAY_MS);
                    Assert.assertEquals(1, postTask.getPendingTasksCount());
                    histograms.assertExpected();

                    awContents.onWindowVisibilityChanged(View.VISIBLE);
                    Assert.assertFalse(awContents.hasDrawFunctor());
                });

        // Rendering still works.
        doHardwareRenderingSmokeTest(testView, 42, 42, 42);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(awContents.hasDrawFunctor());
                });
    }

    // Disables hardware acceleration and ensures that there is no crash in the code that adds and
    // removes frame metrics listener. This code should do nothing when hardware acceleration is
    // disabled.
    @Test
    @DisableHardwareAcceleration
    @SmallTest
    @Feature({"AndroidWebView"})
    @Features.EnableFeatures({BaseFeatures.COLLECT_ANDROID_FRAME_TIMELINE_METRICS})
    public void testNoCrashWithoutHardwareAcceleration() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        AwContents.resetRecordMemoryForTesting();

        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testView.getAwContents();

        // Frame metrics listener is detached when AwContents becomes invisible.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    awContents.onWindowVisibilityChanged(View.INVISIBLE);
                });

        Assert.assertFalse(testView.isBackedByHardwareView());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SuppressLint("WrongConstant")
    // crbug.com/1493531
    public void testInvalidTouchEventIsRemoved() {
        mActivityTestRule.startBrowserProcess();
        AwContents awContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        MotionEvent.PointerProperties properties = new MotionEvent.PointerProperties();
        properties.id = 1;
        properties.toolType = 20;
        // Create a motion event with one pointer with tool type set to 20.
        MotionEvent event =
                MotionEvent.obtain(
                        0L,
                        0L,
                        0,
                        1,
                        new MotionEvent.PointerProperties[] {properties},
                        new MotionEvent.PointerCoords[] {new MotionEvent.PointerCoords()},
                        0,
                        0,
                        0f,
                        0f,
                        0,
                        0,
                        0,
                        0);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Input.ToolType.Android", 20);
        Assert.assertFalse(awContents.onTouchEvent(event));
        watcher.assertExpected();
    }
}
