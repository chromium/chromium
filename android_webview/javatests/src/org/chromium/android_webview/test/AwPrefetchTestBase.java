// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwNoVarySearchData;
import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwPrefetchManager;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.android_webview.test.util.AwPrefetchTestUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content_public.browser.test.util.NavigationControllerUtil;
import org.chromium.content_public.browser.test.util.NavigationEntrySimple;
import org.chromium.net.test.ServerCertificate;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * The tests in this class and subclasses should cover all WebView's expectations for Prefetch.
 * Changing any of these tests should be reflected in our API docs as they map to our API usage
 * expectations.
 *
 * <p>Inheritance hierarchy and test/code placement:
 *
 * <ul>
 *   <li>`AwPrefetchTestBase` (this class): base abstract class for all WebView Prefetch API tests.
 *       Put tests (methods with `@Test`) here applicable for all triggers/configurations below.
 *       `startPrefetchAndWait()`, `mRunOnWorkerThread` and feature flags sets in the subclasses
 *       differentiate the behavior.
 *   <li>`AwPrefetchOmtTestBase`: base abstract class for all OMT-trigger WebView Prefetch tests.
 *       Put tests applicable for OMT triggers in `AwPrefetchOmtTestBase`.
 *   <li>`AwPrefetch*Test`: concrete test classes for each specific trigger, feature params config,
 *       etc.
 * </ul>
 */
public abstract class AwPrefetchTestBase extends AwParameterizedTest {

    // Current tests doesn't require a complex webpage to test. Later on we may need to add specific
    // page with different resources in it.
    protected static final String BASIC_PREFETCH_RELATIVE_PATH =
            "/android_webview/test/data/hello_world.html";

    protected final TestAwContentsClient mContentsClient;
    private final boolean mRunOnWorkerThread;
    protected AwEmbeddedTestServer mTestServer;
    protected String mPrefetchUrl;
    protected AwBrowserContext mBrowserContext;
    protected AwPrefetchManager mPrefetchManager;

    public AwPrefetchTestBase(AwSettingsMutation param, boolean runOnWorkerThread) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
        mContentsClient = new TestAwContentsClient();
        mRunOnWorkerThread = runOnWorkerThread;
    }

    @Rule public AwActivityTestRule mActivityTestRule;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startBrowserProcess();

        mTestServer =
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_TEST_NAMES);

        mPrefetchUrl = getUrl(BASIC_PREFETCH_RELATIVE_PATH);

        // Inject hints for PrePrefetch by default.
        final Origin prefetchOrigin = Origin.create(new GURL(mPrefetchUrl));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AwPrefetchTestUtil.setLatestPrefetchInfoForTesting(
                                prefetchOrigin.toString(), /* javascriptEnabled= */ true));

        // Create the custom context after hints are set, so it picks them up.
        mBrowserContext =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> AwBrowserContextStore.getNamedContext("TestContext", true));
        mPrefetchManager = mBrowserContext.getPrefetchManager();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AwPrefetchTestUtil.clearLatestPrefetchInfoForTesting());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchRequestResponseSuccess() throws Throwable {
        // Do the prefetch request.
        TestAwPrefetchCallback callback =
                startPrefetchAndWait(mPrefetchUrl, getAwPrefetchParameters());

        // wait then do the checks
        callback.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchRequestHTTPSOnlySupported() throws Throwable {
        // Do the prefetch request.
        TestAwPrefetchCallback callback =
                startPrefetchAndWait("http://www.example.com", getAwPrefetchParameters());

        // wait then do the checks
        callback.getOnErrorHelper().waitForNext();
        Assert.assertEquals(1, callback.getOnErrorHelper().getCallCount());
        Assert.assertEquals(
                IllegalArgumentException.class, callback.getOnErrorHelper().getError().getClass());
        Assert.assertEquals(
                "URL must have HTTPS scheme for prefetch.",
                callback.getOnErrorHelper().getError().getMessage());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().mExtras);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchRequestInvalidHeaders() throws Throwable {
        final String[] invalids = {"null\u0000", "cr\r", "nl\n"};
        for (String invalid : invalids) {
            // try each invalid string as a key and a value
            // Prepare PrefetchParameters
            Map<String, String> additionalHeaders = new HashMap<>();
            additionalHeaders.put(invalid, "bar");
            AwPrefetchParameters prefetchParameters =
                    new AwPrefetchParameters(additionalHeaders, null, true);

            // Do the prefetch request.
            TestAwPrefetchCallback callback =
                    startPrefetchAndWait("https://www.example.com", prefetchParameters);

            // wait then do the checks
            callback.getOnErrorHelper().waitForNext();
            Assert.assertEquals(1, callback.getOnErrorHelper().getCallCount());
            Assert.assertEquals(
                    IllegalArgumentException.class,
                    callback.getOnErrorHelper().getError().getClass());
            Assert.assertEquals(
                    "HTTP headers must not contain null, CR, or NL characters. Invalid header name"
                            + " '"
                            + invalid
                            + "'.",
                    callback.getOnErrorHelper().getError().getMessage());
            Assert.assertNull(callback.getOnStatusUpdatedHelper().mExtras);
        }
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchRequestDuplicate() throws Throwable {
        boolean omtEnabled = AwPrefetchManager.isWebViewPrefetchOffTheMainThreadEnabled();
        // Prepare PrefetchParameters
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(null, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);

        // Do another prefetch request but add the ignored query parameters.
        String prefetchUrlWithQueryParams = mPrefetchUrl + "?ts=1000&uid=007";
        TestAwPrefetchCallback callback2;
        if (mRunOnWorkerThread && omtEnabled) {
            CountDownLatch latch = new CountDownLatch(1);
            callback2 = new TestAwPrefetchCallback();
            // We call `startPrefetchRequestAsync()` directly instead of
            // `startPrefetchAndWait()` because when OMT is enabled, this
            // duplicate request fails PrePrefetch and falls back to the UI
            // thread. This runs the key listener on the UI thread, which
            // violates `startPrefetchAndWait()`'s assertion that it runs
            // on the worker thread.
            // TODO(crbug.com/519611014): Consider revisiting this behavior
            // where PrePrefetch duplication failure falls back to a normal
            // Prefetch.
            mPrefetchManager.startPrefetchRequestAsync(
                    SystemClock.uptimeMillis(),
                    prefetchUrlWithQueryParams,
                    prefetchParameters,
                    callback2,
                    Runnable::run,
                    prefetchKey -> {
                        callback2.setPrefetchKey(prefetchKey);
                        latch.countDown();
                    });
            Assert.assertTrue("Prefetch should start", latch.await(5, TimeUnit.SECONDS));
        } else {
            callback2 = startPrefetchAndWait(prefetchUrlWithQueryParams, prefetchParameters);
        }

        // wait then do the checks
        callback2.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.DUPLICATE_REQUEST,
                callback2.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback2.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback2.getOnErrorHelper().mError);

        if (mRunOnWorkerThread && omtEnabled) {
            // TODO(crbug.com/519611014): PrePrefetch failure fallback triggers a second duplicate
            // check on the UI thread, causing the duplicate callback to run twice.
            callback2.getOnStatusUpdatedHelper().waitForNext();
            Assert.assertEquals(2, callback2.getOnStatusUpdatedHelper().getCallCount());
            Assert.assertEquals(
                    AwPrefetchCallback.StatusCode.DUPLICATE_REQUEST,
                    callback2.getOnStatusUpdatedHelper().getStatusCode());
        } else {
            Assert.assertEquals(1, callback2.getOnStatusUpdatedHelper().getCallCount());
        }

        // Finally, do a third request with an unexpected query parameter.
        String prefetchUrlWithUnexpectedQueryParam = prefetchUrlWithQueryParams + "&q=help";
        TestAwPrefetchCallback callback3 =
                startPrefetchAndWait(prefetchUrlWithUnexpectedQueryParam, prefetchParameters);

        // wait then do the checks
        callback3.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(1, callback3.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertNotEquals(
                AwPrefetchCallback.StatusCode.DUPLICATE_REQUEST,
                callback3.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback3.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback3.getOnErrorHelper().mError);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchCancellation() throws Throwable {
        // Do the prefetch request.
        TestAwPrefetchCallback callback =
                startPrefetchAndWait(mPrefetchUrl, getAwPrefetchParameters());

        // Wait for the prefetch success & key for cancellation.
        callback.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);

        Assert.assertNotEquals(
                mPrefetchManager.getNoPrefetchKeyForTesting(), callback.getPrefetchKey());

        // Test cancellation.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final int prefetchKey = callback.getPrefetchKey();
                    Assert.assertTrue(mPrefetchManager.getIsPrefetchInCacheForTesting(prefetchKey));
                    mPrefetchManager.cancelPrefetch(prefetchKey);

                    // The prefetch for this key should no longer be in the cache after
                    // cancellation.
                    Assert.assertFalse(
                            mPrefetchManager.getIsPrefetchInCacheForTesting(prefetchKey));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSettingConfigsWithInValidValues() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Updating with negative values shouldn't be applied
                    mPrefetchManager.updatePrefetchConfiguration(-1, -1);
                    Assert.assertTrue(mPrefetchManager.getPrefetchTtlSeconds() > 0);
                    Assert.assertTrue(mPrefetchManager.getMaxPrefetches() > 0);

                    // Updating with 0 shouldn't be applied as well.
                    mPrefetchManager.updatePrefetchConfiguration(0, 0);
                    Assert.assertTrue(mPrefetchManager.getPrefetchTtlSeconds() > 0);
                    Assert.assertTrue(mPrefetchManager.getMaxPrefetches() > 0);
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSettingConfigsWithValidValues() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrefetchManager.updatePrefetchConfiguration(60, 5);
                    Assert.assertEquals(60, mPrefetchManager.getPrefetchTtlSeconds());
                    Assert.assertEquals(5, mPrefetchManager.getMaxPrefetches());
                });
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchHasExpectedSecHeaderPurposeHeaderValue() throws Throwable {
        // Do the prefetch request.
        TestAwPrefetchCallback callback =
                startPrefetchAndWait(mPrefetchUrl, getAwPrefetchParameters());

        // wait then do the checks
        callback.getOnStatusUpdatedHelper().waitForNext();
        HashMap<String, String> prefetchHeaders =
                mTestServer.getRequestHeadersForUrl(BASIC_PREFETCH_RELATIVE_PATH);
        String secPurposeHeaderValue = prefetchHeaders.get("Sec-Purpose");
        Assert.assertNotNull(secPurposeHeaderValue);
        Assert.assertTrue(AwPrefetchManager.isSecPurposeForPrefetch(secPurposeHeaderValue));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewSaveStateIncludeHeaders"})
    public void testPrefetchAndSaveState() throws Throwable {
        // --- 1. Prepare Prefetch Parameters ---
        Map<String, String> prefetchExtraHeaders = Map.of("Test-Header1", "1", "Test-Header2", "2");
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(prefetchExtraHeaders, null, true);

        // Create the initial AwContents instance
        final AwContents awContents =
                createAwTestContainerViewOnMainSync(mContentsClient).getAwContents();

        // --- 2. Execute Prefetch Request ---
        TestAwPrefetchCallback prefetchCallback =
                startPrefetchAndWait(mPrefetchUrl, prefetchParameters);
        prefetchCallback.getOnStatusUpdatedHelper().waitForNext(); // Wait for status update

        // --- 3. FIRST CHECK: Nothing saved after ONLY calling prefetch ---
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            Bundle bundle = new Bundle();
                            boolean saved = awContents.saveState(bundle);
                            // State should be false because there is no navigation history yet
                            Assert.assertFalse(
                                    "Expected saveState to return false after prefetch only",
                                    saved);
                        });

        // --- 4. Navigate (Load Content) ---
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), mPrefetchUrl);

        // --- 5. Verify Server only received one request from the prefetch and that request headers
        // are present ---
        Assert.assertEquals(1, mTestServer.getRequestCountForUrl(BASIC_PREFETCH_RELATIVE_PATH));
        Map<String, String> receivedHeaders =
                mTestServer.getRequestHeadersForUrl(BASIC_PREFETCH_RELATIVE_PATH);
        Assert.assertFalse(receivedHeaders.isEmpty());
        Assert.assertEquals("1", receivedHeaders.get("Test-Header1"));
        Assert.assertEquals("2", receivedHeaders.get("Test-Header2"));
        Assert.assertEquals("prefetch", receivedHeaders.get("Sec-Purpose"));

        // --- 6. SECOND CHECK: Navigation entry is saved after calling loadUrl ---
        // Create a new container view to restore the state into
        TestAwContentsClient restoredStateContentsClient = new TestAwContentsClient();
        AwTestContainerView restoredStateTestView =
                createAwTestContainerViewOnMainSync(restoredStateContentsClient);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            Bundle bundle = new Bundle();

                            // Now that we've navigated, state should be successfully saved
                            boolean saved = awContents.saveState(bundle);
                            Assert.assertTrue("Expected state to be successfully saved", saved);

                            // Verify that we can restore the state to the new view
                            boolean restored =
                                    restoredStateTestView.getAwContents().restoreState(bundle);
                            Assert.assertTrue(
                                    "Expected state to be successfully restored", restored);

                            // Verify the navigation history was populated
                            NavigationEntrySimple[] navHistory =
                                    NavigationControllerUtil.getNavigationHistorySimple(
                                            restoredStateTestView.getAwContents().getWebContents());
                            Assert.assertEquals(
                                    "Restored navigation history should have 1 entry",
                                    1,
                                    navHistory.length);

                            NavigationEntrySimple restoredEntry = navHistory[0];
                            Assert.assertTrue(
                                    "Prefetch with loadurl does not save headers",
                                    restoredEntry.getExtraHeaders().isEmpty());
                        });
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchAfterNavigationLogging() throws Throwable {
        mActivityTestRule.startBrowserProcess();

        final String url = getUrl(BASIC_PREFETCH_RELATIVE_PATH);

        // Do a navigation and wait for it to complete.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwTestContainerView testView =
                            createAwTestContainerViewOnMainSync(mContentsClient);
                    final AwContents awContents = testView.getAwContents();

                    AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
                    awContents.loadUrl(url);
                });

        try (var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "Prefetch.PrefetchContainer.PrefetchMatchMissed"
                                        + "ToPrefetchStarted.Embedder_WebView")
                        .build()) {

            // Make a prefetch request with the exact same URL as the navigation.
            TestAwPrefetchCallback callback = startPrefetchAndWait(url, getAwPrefetchParameters());
            callback.getOnStatusUpdatedHelper().waitForNext();

            // Cancel the prefetch so that the histogram is logged.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        final int prefetchKey = callback.getPrefetchKey();
                        mPrefetchManager.cancelPrefetch(prefetchKey);
                    });
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        }
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchAfterNavigationLogging_notLoggedScenario() throws Throwable {
        mActivityTestRule.startBrowserProcess();

        // Do a navigation and wait for it to complete.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final String url = getUrl(BASIC_PREFETCH_RELATIVE_PATH + "/1");
                    AwTestContainerView testView =
                            createAwTestContainerViewOnMainSync(mContentsClient);
                    final AwContents awContents = testView.getAwContents();

                    AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
                    awContents.loadUrl(url);
                });

        try (var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Prefetch.PrefetchContainer.PrefetchMatchMissed"
                                        + "ToPrefetchStarted.Embedder_WebView")
                        .build()) {

            // Make a prefetch request with the exact same URL as the navigation.
            final String url = getUrl(BASIC_PREFETCH_RELATIVE_PATH);
            TestAwPrefetchCallback callback = startPrefetchAndWait(url, getAwPrefetchParameters());
            callback.getOnStatusUpdatedHelper().waitForNext();

            // Cancel the prefetch so that the histogram is logged.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        final int prefetchKey = callback.getPrefetchKey();
                        mPrefetchManager.cancelPrefetch(prefetchKey);
                    });
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        }
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchBypassesHttpCacheWithHeader() throws Throwable {
        final String testPath = "/cachetime";
        final String testUrl = getUrl(testPath);

        // Perform a prefetch with the cache bypass header.
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("X-Disable-Http-Cache", "1");
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, null, true);
        TestAwPrefetchCallback callback = startPrefetchAndWait(testUrl, prefetchParameters);
        callback.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(
                "Prefetch should complete successfully.",
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertEquals(
                "Server should have received one request from the prefetch.",
                1,
                mTestServer.getRequestCountForUrl(testPath));
        final int prefetchKey = callback.getPrefetchKey();

        // Cancel the prefetch to prevent it from serving since we are testing
        // the HTTP cache.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrefetchManager.cancelPrefetch(prefetchKey);
                });

        // Load the same URL in a WebView.
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), testUrl);

        // Verify that the server received a second request, proving the prefetch response
        // was not written to the HTTP cache.
        Assert.assertEquals(
                "Server should have received a second request from the page load.",
                2,
                mTestServer.getRequestCountForUrl(testPath));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchUsesHttpCacheByDefault() throws Throwable {
        final String testPath = "/cachetime";
        final String testUrl = getUrl(testPath);

        // Perform a standard prefetch to populate the cache with a cacheable response.
        TestAwPrefetchCallback callback = startPrefetchAndWait(testUrl, getAwPrefetchParameters());
        callback.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(
                "Prefetch should complete successfully.",
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertEquals(
                "Server should have received one request from the prefetch.",
                1,
                mTestServer.getRequestCountForUrl(testPath));
        final int prefetchKey = callback.getPrefetchKey();

        // Cancel the prefetch to prevent it from serving since we are testing
        // the HTTP cache.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrefetchManager.cancelPrefetch(prefetchKey);
                });

        // Load the same URL in a WebView.
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), testUrl);

        // 4. Verify that the server did NOT receive a second request, proving the page load
        // was served from the HTTP cache populated by the prefetch.
        Assert.assertEquals(
                "Server should NOT have received a second request.",
                1,
                mTestServer.getRequestCountForUrl(testPath));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=ExternalExperimentAllowlist:123/PrefetchStudy,Group1"})
    public void testPrefetchRequestWithVariationsId() throws Throwable {
        // The Variations ID (123) must match the entry in the ExternalExperimentAllowlist
        // defined in the @CommandLineFlags above. The metrics service will only register
        // IDs that have been explicitly allowlisted for privacy and security reasons.
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(new HashMap<>(), null, true, 123);

        // We expect 1 group to be registered.
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("UMA.ExternalExperiment.GroupCount", 1)
                        .build();

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.getOnStatusUpdatedHelper().waitForNext();
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    /**
     * Tests that a Prefetch/PrePrefetch request correctly includes the "X-Requested-With" header.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchHasExpectedXRequestedWithHeader() throws Throwable {
        TestAwPrefetchCallback callback =
                startPrefetchAndWait(mPrefetchUrl, getAwPrefetchParameters());
        callback.getOnStatusUpdatedHelper().waitForNext();

        HashMap<String, String> prefetchHeaders =
                mTestServer.getRequestHeadersForUrl(BASIC_PREFETCH_RELATIVE_PATH);
        String xRequestedWith = prefetchHeaders.get("X-Requested-With");
        Assert.assertNotNull("X-Requested-With header should be present", xRequestedWith);
        Assert.assertEquals(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName(),
                xRequestedWith);
    }

    protected String getUrl(final String relativePath) {
        return mTestServer.getURLWithHostName("a.test", relativePath);
    }

    protected static AwPrefetchParameters getAwPrefetchParameters() {
        return new AwPrefetchParameters(null, null, true);
    }

    protected AwTestContainerView createAwTestContainerViewOnMainSync(TestAwContentsClient client) {
        return mActivityTestRule.createAwTestContainerViewOnMainSync(
                client,
                /* supportsLegacyQuirks= */ false,
                /* testDependencyFactory= */ null,
                mBrowserContext);
    }

    protected TestAwPrefetchCallback startPrefetchAndWait(
            String url, AwPrefetchParameters prefetchParameters) throws Exception {
        return startPrefetchAndWait(mRunOnWorkerThread, url, prefetchParameters, mPrefetchManager);
    }

    protected TestAwPrefetchCallback startPrefetchAndWait(
            boolean runOnWorkerThread,
            String url,
            AwPrefetchParameters prefetchParameters,
            AwPrefetchManager prefetchManager)
            throws Exception {
        if (!runOnWorkerThread) {
            TestAwPrefetchCallback callback = new TestAwPrefetchCallback();

            Executor callbackExecutor = Runnable::run;
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        int prefetchKey =
                                prefetchManager.startPrefetchRequest(
                                        url, prefetchParameters, callback, callbackExecutor);
                        callback.setPrefetchKey(prefetchKey);
                    });

            return callback;
        } else {
            boolean omtEnabled = AwPrefetchManager.isWebViewPrefetchOffTheMainThreadEnabled();

            HistogramWatcher.Builder builder = HistogramWatcher.newBuilder();
            if (omtEnabled) {
                // This histogram is only recorded when the Prefetch is posted on the main thread,
                // which means that PrePrefetch is failed and fallback to normal prefetch when the
                // flag is enabled.
                builder.expectNoRecords(
                        "Android.WebView.Profile.Prefetch.QueuedPrefetchExecutionDelay");
            }
            HistogramWatcher histogramWatcher = builder.build();

            TestAwPrefetchCallback callback = new TestAwPrefetchCallback();
            CountDownLatch latch = new CountDownLatch(1);
            AtomicBoolean keyListenerCalledOnWorkerThread = new AtomicBoolean(false);

            prefetchManager.startPrefetchRequestAsync(
                    SystemClock.uptimeMillis(),
                    url,
                    prefetchParameters,
                    callback,
                    Runnable::run,
                    prefetchKey -> {
                        if (!org.chromium.base.ThreadUtils.runningOnUiThread()) {
                            keyListenerCalledOnWorkerThread.set(true);
                        }
                        callback.setPrefetchKey(prefetchKey);
                        latch.countDown();
                    });

            Assert.assertTrue("Prefetch should start", latch.await(5, TimeUnit.SECONDS));

            if (omtEnabled) {
                Assert.assertTrue(
                        "Key listener should be called on worker thread for a PrePrefetch",
                        keyListenerCalledOnWorkerThread.get());
                histogramWatcher.assertExpected();
            }

            return callback;
        }
    }

    /**
     * A class to map the TestDelegate for handling the callback checks, see {@link CallbackHelper}
     * javadocs for more details.
     */
    public static class TestAwPrefetchCallback implements AwPrefetchCallback {

        public static class OnStatusUpdatedHelper extends CallbackHelper {
            private int mStatusCode = -1;
            private @Nullable Bundle mExtras;

            public int getStatusCode() {
                assertThat(getCallCount()).isGreaterThan(0);
                return mStatusCode;
            }

            @Keep
            @Nullable
            public Bundle getExtras() {
                assertThat(getCallCount()).isGreaterThan(0);
                return mExtras;
            }

            public void notifyCalled(int statusCode, @Nullable Bundle extras) {
                mStatusCode = statusCode;
                mExtras = extras;
                super.notifyCalled();
            }
        }

        public static class OnErrorHelper extends CallbackHelper {
            private Throwable mError;

            public Throwable getError() {
                assertThat(getCallCount()).isGreaterThan(0);
                return mError;
            }

            public void notifyCalled(Throwable error) {
                mError = error;
                super.notifyCalled();
            }
        }

        private final OnStatusUpdatedHelper mOnStatusUpdatedHelper;
        private final OnErrorHelper mOnErrorHelper;
        private int mPrefetchKey = -1;

        public TestAwPrefetchCallback() {
            mOnStatusUpdatedHelper = new OnStatusUpdatedHelper();
            mOnErrorHelper = new OnErrorHelper();
        }

        public OnStatusUpdatedHelper getOnStatusUpdatedHelper() {
            return mOnStatusUpdatedHelper;
        }

        public OnErrorHelper getOnErrorHelper() {
            return mOnErrorHelper;
        }

        public void setPrefetchKey(int prefetchKey) {
            mPrefetchKey = prefetchKey;
        }

        public int getPrefetchKey() {
            return mPrefetchKey;
        }

        @Override
        public void onStatusUpdated(int statusCode, @Nullable Bundle extras) {
            mOnStatusUpdatedHelper.notifyCalled(statusCode, extras);
        }

        @Override
        public void onError(Throwable e) {
            mOnErrorHelper.notifyCalled(e);
        }
    }
}
