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
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content_public.browser.test.util.NavigationControllerUtil;
import org.chromium.content_public.browser.test.util.NavigationEntrySimple;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.ServerCertificate;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * This test should cover all WebView's expectations for Prefetch. Changing any of these tests
 * should be reflected in our API docs as they map to our API usage expectations.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@DoNotBatch(reason = "Tests that need browser start are incompatible with @Batch")
public class AwPrefetchTest extends AwParameterizedTest {

    // Current tests doesn't require a complex webpage to test. Later on we may need to add specific
    // page with different resources in it.
    private static final String BASIC_PREFETCH_RELATIVE_PATH =
            "/android_webview/test/data/hello_world.html";

    private final TestAwContentsClient mContentsClient;
    private AwEmbeddedTestServer mTestServer;
    private String mPrefetchUrl;

    public AwPrefetchTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
        mContentsClient = new TestAwContentsClient();
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
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AwPrefetchTestUtil.clearLatestPrefetchInfoForTesting());
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchRequestResponseSuccess() throws Throwable {
        // Prepare PrefetchParameters
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("foo", "bar");
        additionalHeaders.put("lorem", "ipsum");
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.mOnStatusUpdatedHelper.waitForNext();
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
        // Prepare PrefetchParameters
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("foo", "bar");
        additionalHeaders.put("lorem", "ipsum");
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback =
                startPrefetchingAndWait("http://www.example.com", prefetchParameters);

        // wait then do the checks
        callback.mOnErrorHelper.waitForNext();
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
            AwNoVarySearchData expectedNoVarySearch =
                    new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
            AwPrefetchParameters prefetchParameters =
                    new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

            // Do the prefetch request.
            TestAwPrefetchCallback callback =
                    startPrefetchingAndWait("https://www.example.com", prefetchParameters);

            // wait then do the checks
            callback.mOnErrorHelper.waitForNext();
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchRequestDuplicate() throws Throwable {
        // Prepare PrefetchParameters
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(null, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);

        // Do another prefetch request but add the ignored query parameters.
        String prefetchUrlWithQueryParams = mPrefetchUrl + "?ts=1000&uid=007";
        TestAwPrefetchCallback callback2 =
                startPrefetchingAndWait(prefetchUrlWithQueryParams, prefetchParameters);

        // wait then do the checks
        callback2.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback2.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.DUPLICATE_REQUEST,
                callback2.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback2.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback2.getOnErrorHelper().mError);

        // Finally, do a third request with an unexpected query parameter.
        String prefetchUrlWithUnexpectedQueryParam = prefetchUrlWithQueryParams + "&q=help";
        TestAwPrefetchCallback callback3 =
                startPrefetchingAndWait(prefetchUrlWithUnexpectedQueryParam, prefetchParameters);

        // wait then do the checks
        callback3.mOnStatusUpdatedHelper.waitForNext();
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchCancellation() throws Throwable {
        // Prepare PrefetchParameters
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(null, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // Wait for the prefetch success & key for cancellation.
        callback.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(1, callback.getOnStatusUpdatedHelper().getCallCount());
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertNull(callback.getOnStatusUpdatedHelper().getExtras());
        Assert.assertNull(callback.getOnErrorHelper().mError);

        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        Assert.assertNotEquals(
                prefetchManager.getNoPrefetchKeyForTesting(), callback.getPrefetchKey());

        // Test cancellation.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final int prefetchKey = callback.getPrefetchKey();
                    Assert.assertTrue(prefetchManager.getIsPrefetchInCacheForTesting(prefetchKey));
                    mActivityTestRule
                            .getAwBrowserContext()
                            .getPrefetchManager()
                            .cancelPrefetch(prefetchKey);

                    // The prefetch for this key should no longer be in the cache after
                    // cancellation.
                    Assert.assertFalse(prefetchManager.getIsPrefetchInCacheForTesting(prefetchKey));
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSettingConfigsWithInValidValues() {
        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Updating with negative values shouldn't be applied
                    prefetchManager.updatePrefetchConfiguration(-1, -1);
                    Assert.assertTrue(prefetchManager.getPrefetchTtlSeconds() > 0);
                    Assert.assertTrue(prefetchManager.getMaxPrefetches() > 0);

                    // Updating with 0 shouldn't be applied as well.
                    prefetchManager.updatePrefetchConfiguration(0, 0);
                    Assert.assertTrue(prefetchManager.getPrefetchTtlSeconds() > 0);
                    Assert.assertTrue(prefetchManager.getMaxPrefetches() > 0);
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSettingConfigsWithValidValues() {
        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    prefetchManager.updatePrefetchConfiguration(60, 5);
                    Assert.assertEquals(60, prefetchManager.getPrefetchTtlSeconds());
                    Assert.assertEquals(5, prefetchManager.getMaxPrefetches());
                });
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "disable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void
            testPrefetchQueueDrainedWhenUiThreadIsFree_VerifyPrefetchExecutionCount_OMTPrefetchDisabled() {
        testPrefetchQueueDrainedWhenUiThreadIsFree_VerifyPrefetchExecutionCount();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void
            testPrefetchQueueDrainedWhenUiThreadIsFree_VerifyPrefetchExecutionCount_OMTPrefetchEnabled() {
        testPrefetchQueueDrainedWhenUiThreadIsFree_VerifyPrefetchExecutionCount();
    }

    private void testPrefetchQueueDrainedWhenUiThreadIsFree_VerifyPrefetchExecutionCount() {
        AtomicInteger executedPrefetchCount = new AtomicInteger(0);
        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        prefetchManager.setCallbackForTesting(executedPrefetchCount::incrementAndGet);

        // Latch for the UI thread to block on.
        CountDownLatch uiThreadBlockLatch = new CountDownLatch(1);

        // This ensures the UI thread is waiting BEFORE the drain tasks posted by
        // startPrefetchRequestAsync can be processed.
        ThreadUtils.runOnUiThread(
                () -> {
                    try {
                        // The UI thread will stop here and wait until
                        // uiThreadBlockLatch.countDown() is called
                        // from another thread.
                        Assert.assertTrue(
                                "UI thread timed out waiting for instrumentation thread to finish"
                                        + " queueing prefetch requests.",
                                uiThreadBlockLatch.await(5, TimeUnit.SECONDS));
                    } catch (InterruptedException e) {
                        throw new RuntimeException("UI thread interrupted while blocked", e);
                    }
                });

        int numberOfPrefetches = 5;
        AwPrefetchParameters prefetchParameters = getAwPrefetchParameters();
        TestAwPrefetchCallback callback = new TestAwPrefetchCallback();

        for (int i = 0; i < numberOfPrefetches; i++) {
            // Call the async start prefetch method from the instrumentation thread.
            // This adds a prefetch request to a queue AND
            // posts a drain task to the UI thread (non-redundantly).
            // The UI thread is currently blocked by uiThreadBlockLatch.await(),
            // so the drain task will sit in its message queue until the latch is released.
            prefetchManager.startPrefetchRequestAsync(
                    SystemClock.uptimeMillis(),
                    mPrefetchUrl,
                    prefetchParameters,
                    callback,
                    Runnable::run,
                    integer -> {});
        }

        Assert.assertEquals(
                "Prefetches should be blocked from executing while UI thread is blocked.",
                0,
                executedPrefetchCount.intValue());

        // Signal the UI thread latch to unblock it.
        uiThreadBlockLatch.countDown();

        // At this point, the UI thread has been unblocked
        // and is now free to process its message queue, including the drain task.
        // Wait for the UI thread to process the queue and drain it to 0.
        // CriteriaHelper.pollInstrumentationThread runs on the instrumentation thread,
        // allowing the UI thread to run concurrently.
        CriteriaHelper.pollInstrumentationThread(
                () -> executedPrefetchCount.intValue() == numberOfPrefetches,
                "Prefetch queue did not drain after UI thread was unblocked.");
        prefetchManager.setCallbackForTesting(null);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "disable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void
            testPrefetchQueueExplicitlyDrainedDuringAwContentsInitAndLoadUrl_OMTPrefetchDisabled() {
        testPrefetchQueueExplicitlyDrainedDuringAwContentsInitAndLoadUrl();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void
            testPrefetchQueueExplicitlyDrainedDuringAwContentsInitAndLoadUrl_OMTPrefetchEnabled() {
        testPrefetchQueueExplicitlyDrainedDuringAwContentsInitAndLoadUrl();
    }

    private void testPrefetchQueueExplicitlyDrainedDuringAwContentsInitAndLoadUrl() {
        // Latch to block `AwContents` creation.
        CountDownLatch awContentsCreationLatch = new CountDownLatch(1);
        AtomicBoolean prefetchQueueDrainedDuringAwContentsConstructor = new AtomicBoolean(false);
        CountDownLatch awContentsConstructorFinishedLatch = new CountDownLatch(1);

        // Latch to block `AwContents#loadUrl` call.
        CountDownLatch loadUrlLatch = new CountDownLatch(1);
        AtomicBoolean prefetchQueueDrainedDuringLoadUrl = new AtomicBoolean(false);

        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();
        ThreadUtils.runOnUiThread(
                () -> {
                    try {
                        // Verify we drain the prefetch queue during `AwContents` constructor.
                        // Wait on the `AwContents` latch to release.
                        prefetchManager.setCallbackForTesting(
                                () -> prefetchQueueDrainedDuringAwContentsConstructor.set(true));
                        Assert.assertTrue(
                                "UI thread timed out waiting for instrumentation thread to finish"
                                    + " queueing prefetch requests before AwContents constructor.",
                                awContentsCreationLatch.await(5, TimeUnit.SECONDS));
                        Assert.assertFalse(prefetchQueueDrainedDuringAwContentsConstructor.get());
                        mActivityTestRule.startBrowserProcess();
                        AwContents awContents =
                                mActivityTestRule
                                        .createAwTestContainerViewOnMainSync(mContentsClient)
                                        .getAwContents();
                        Assert.assertTrue(
                                "Queued prefetches were not executed during AwContents"
                                        + " constructor.",
                                prefetchQueueDrainedDuringAwContentsConstructor.get());
                        awContentsConstructorFinishedLatch.countDown();

                        // Verify we drain the prefetch queue after loadUrl() is called.
                        prefetchManager.setCallbackForTesting(
                                () -> prefetchQueueDrainedDuringLoadUrl.set(true));
                        Assert.assertTrue(
                                "UI thread timed out waiting for instrumentation thread to finish"
                                        + " queueing prefetch requests before loadUrl() call.",
                                loadUrlLatch.await(5, TimeUnit.SECONDS));
                        Assert.assertFalse(prefetchQueueDrainedDuringLoadUrl.get());
                        awContents.loadUrl("about:blank");
                        Assert.assertTrue(
                                "Queued prefetches were not executed during AwContents#loadUrl.",
                                prefetchQueueDrainedDuringAwContentsConstructor.get());

                    } catch (InterruptedException e) {
                        throw new RuntimeException("UI thread interrupted while blocked", e);
                    }
                });

        AwPrefetchParameters prefetchParameters = getAwPrefetchParameters();
        TestAwPrefetchCallback callback = new TestAwPrefetchCallback();

        // Make a prefetch request on the instrumentation thread then release the `AwContents`
        // countdown latch.
        prefetchManager.startPrefetchRequestAsync(
                SystemClock.uptimeMillis(),
                mPrefetchUrl,
                prefetchParameters,
                callback,
                Runnable::run,
                integer -> {});
        awContentsCreationLatch.countDown();

        // Wait for the `AwContents` constructor to complete and the latch to be released.
        try {
            Assert.assertTrue(
                    "Instrumentation thread timed out waiting for UI thread to finish with the"
                            + " AwContents constructor.",
                    awContentsConstructorFinishedLatch.await(5, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            throw new RuntimeException(
                    "Instrumentation thread interrupted waiting for AwContents constructor to"
                            + " finish.",
                    e);
        }

        // Make another prefetch request on the instrumentation thread then release the
        // `AwContents#loadUrl` latch.
        prefetchManager.startPrefetchRequestAsync(
                SystemClock.uptimeMillis(),
                mPrefetchUrl,
                prefetchParameters,
                callback,
                Runnable::run,
                integer -> {});
        loadUrlLatch.countDown();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchHasExpectedSecHeaderPurposeHeaderValue() throws Throwable {
        // Prepare PrefetchParameters
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("foo", "bar");
        additionalHeaders.put("lorem", "ipsum");
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);

        // Do the prefetch request.
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.mOnStatusUpdatedHelper.waitForNext();
        HashMap<String, String> prefetchHeaders =
                mTestServer.getRequestHeadersForUrl(BASIC_PREFETCH_RELATIVE_PATH);
        String secPurposeHeaderValue = prefetchHeaders.get("Sec-Purpose");
        Assert.assertNotNull(secPurposeHeaderValue);
        Assert.assertTrue(AwPrefetchManager.isSecPurposeForPrefetch(secPurposeHeaderValue));
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=WebViewSaveStateIncludeHeaders"
    })
    public void testPrefetchAndSaveState() throws Throwable {
        // --- 1. Prepare Prefetch Parameters ---
        Map<String, String> prefetchExtraHeaders = Map.of("Test-Header1", "1", "Test-Header2", "2");
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(prefetchExtraHeaders, expectedNoVarySearch, true);

        // Create the initial AwContents instance
        final AwContents awContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();

        // --- 2. Execute Prefetch Request ---
        TestAwPrefetchCallback prefetchCallback =
                startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);
        prefetchCallback.mOnStatusUpdatedHelper.waitForNext(); // Wait for status update

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
                mActivityTestRule.createAwTestContainerViewOnMainSync(restoredStateContentsClient);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            Bundle bundle = new Bundle();

                            // Now that we've navigated, state should be successfully saved
                            boolean saved = awContents.saveState(bundle);
                            Assert.assertTrue("Expected state to be saved after loadUrl", saved);

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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchAfterNavigationLogging() throws Throwable {
        mActivityTestRule.startBrowserProcess();

        final String url = getUrl(BASIC_PREFETCH_RELATIVE_PATH);

        // Do a navigation and wait for it to complete.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwTestContainerView testView =
                            mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
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
            TestAwPrefetchCallback callback =
                    startPrefetchingAndWait(url, getAwPrefetchParameters());
            callback.mOnStatusUpdatedHelper.waitForNext();

            // Cancel the prefetch so that the histogram is logged.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        final int prefetchKey = callback.getPrefetchKey();
                        mActivityTestRule
                                .getAwBrowserContext()
                                .getPrefetchManager()
                                .cancelPrefetch(prefetchKey);
                    });
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        }
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchAfterNavigationLogging_notLoggedScenario() throws Throwable {
        mActivityTestRule.startBrowserProcess();

        // Do a navigation and wait for it to complete.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final String url = getUrl(BASIC_PREFETCH_RELATIVE_PATH + "/1");
                    AwTestContainerView testView =
                            mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
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
            TestAwPrefetchCallback callback =
                    startPrefetchingAndWait(url, getAwPrefetchParameters());
            callback.mOnStatusUpdatedHelper.waitForNext();

            // Cancel the prefetch so that the histogram is logged.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        final int prefetchKey = callback.getPrefetchKey();
                        mActivityTestRule
                                .getAwBrowserContext()
                                .getPrefetchManager()
                                .cancelPrefetch(prefetchKey);
                    });
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        }
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchBypassesHttpCacheWithHeader() throws Throwable {
        final String testPath = "/cachetime";
        final String testUrl = getUrl(testPath);

        // Perform a prefetch with the cache bypass header.
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("X-Disable-Http-Cache", "1");
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, null, true);
        TestAwPrefetchCallback callback = startPrefetchingAndWait(testUrl, prefetchParameters);
        callback.mOnStatusUpdatedHelper.waitForNext();
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
                    mActivityTestRule
                            .getAwBrowserContext()
                            .getPrefetchManager()
                            .cancelPrefetch(prefetchKey);
                });

        // Load the same URL in a WebView.
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
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
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testPrefetchUsesHttpCacheByDefault() throws Throwable {
        final String testPath = "/cachetime";
        final String testUrl = getUrl(testPath);

        // Perform a standard prefetch to populate the cache with a cacheable response.
        TestAwPrefetchCallback callback =
                startPrefetchingAndWait(testUrl, getAwPrefetchParameters());
        callback.mOnStatusUpdatedHelper.waitForNext();
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
                    mActivityTestRule
                            .getAwBrowserContext()
                            .getPrefetchManager()
                            .cancelPrefetch(prefetchKey);
                });

        // Load the same URL in a WebView.
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
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
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=ExternalExperimentAllowlist:123/PrefetchStudy,Group1"
    })
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
        TestAwPrefetchCallback callback = startPrefetchingAndWait(mPrefetchUrl, prefetchParameters);

        // wait then do the checks
        callback.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    // Tests that a PrePrefetch is triggered and completed successfully, and successfully served to
    // a loadUrl.
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void testPrePrefetchServedAndConsumed() throws Throwable {
        String contextName = "TestContext";
        Origin prefetchOrigin = Origin.create(new GURL(mPrefetchUrl));

        // Create a new context. This triggers a fresh `AwPrefetchManager` creation, which can pick
        // up pref's hints injected in `SetUp()`.
        AwBrowserContext context =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> AwBrowserContextStore.getNamedContext(contextName, true));
        AwPrefetchManager prefetchManager = context.getPrefetchManager();

        // PrePrefetch is triggered under the flag enabled.
        TestAwPrefetchCallback callback =
                startPrefetchAsyncAndWait(mPrefetchUrl, getAwPrefetchParameters(), prefetchManager);

        callback.mOnStatusUpdatedHelper.waitForNext();
        Assert.assertEquals(
                "PrePrefetch should complete successfully.",
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());
        Assert.assertEquals(
                "Server should have received one request from the PrePrefetch.",
                1,
                mTestServer.getRequestCountForUrl(BASIC_PREFETCH_RELATIVE_PATH));

        // Load the same URL in a WebView.
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, null, context);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), mPrefetchUrl);

        // Verify that the server did NOT receive a second request, proving the page load
        // was served from the PrePrefetch, not Prefetch and the loadUrl itself.
        Assert.assertEquals(
                "Server should NOT have received a second request.",
                1,
                mTestServer.getRequestCountForUrl(BASIC_PREFETCH_RELATIVE_PATH));
        prefetchManager.setCallbackForTesting(null);
    }

    /**
     * Tests that if PrePrefetch fails the request falls back to a standard UI thread Prefetch
     * request.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void testPrefetchFallbackWhenPrePrefetchFails() throws Throwable {
        final String profileName = "TestProfile";
        final String testUrl = getUrl(BASIC_PREFETCH_RELATIVE_PATH);

        // Intentionally DO NOT inject hints. This guarantees `PrePrefetchService`
        // will experience a cache miss and return `NO_PREFETCH_KEY`, forcing a fallback.
        ThreadUtils.runOnUiThreadBlocking(
                () -> AwPrefetchTestUtil.clearLatestPrefetchInfoForTesting());

        AwBrowserContext context =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> AwBrowserContextStore.getNamedContext(profileName, true));
        AwPrefetchManager prefetchManager = context.getPrefetchManager();

        TestAwPrefetchCallback callback = new TestAwPrefetchCallback();
        CountDownLatch prefetchStartedLatch = new CountDownLatch(1);

        // Check that the Prefetch was called instead of PrePrefetch.
        // Note that `WORKER_THREAD_PREFETCH_SUCCESS` represents for both 1) normal
        // "Prefetch success" (`PrefetchOffTheMainThread` disabled) and 2) PrePrefetch fail but
        // "Prefetch success" (`PrefetchOffTheMainThread` enabled) currently.
        HistogramWatcher fallbackHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.Profile.Prefetch.ApiCallResult",
                                AwPrefetchManager.ApiCallResult.WORKER_THREAD_PREFETCH_SUCCESS)
                        .build();

        prefetchManager.startPrefetchRequestAsync(
                SystemClock.uptimeMillis(),
                testUrl,
                getAwPrefetchParameters(),
                callback,
                Runnable::run,
                prefetchKey -> {
                    callback.setPrefetchKey(prefetchKey);
                    prefetchStartedLatch.countDown();
                });

        Assert.assertTrue(
                "Prefetch should invoke key listener",
                prefetchStartedLatch.await(5, TimeUnit.SECONDS));

        // Wait for completion.
        callback.mOnStatusUpdatedHelper.waitForNext();
        fallbackHistogramWatcher.assertExpected();
        Assert.assertEquals(
                "Fallback prefetch should complete successfully.",
                AwPrefetchCallback.StatusCode.PREFETCH_RESPONSE_COMPLETED,
                callback.getOnStatusUpdatedHelper().getStatusCode());

        // Load the same URL in a WebView and verify consumption.
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, null, context);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), testUrl);

        // Verify that the server did NOT receive a second request.
        Assert.assertEquals(
                "Server should NOT have received a second request.",
                1,
                mTestServer.getRequestCountForUrl(BASIC_PREFETCH_RELATIVE_PATH));
    }

    /**
     * Tests that a Prefetch/PrePrefetch request correctly includes the "X-Requested-With" header.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "disable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void testPrefetchHasExpectedXRequestedWithHeader_OMTPrefetchDisabled() throws Throwable {
        AwPrefetchParameters prefetchParameters = getAwPrefetchParameters();

        AwPrefetchManager prefetchManager =
                mActivityTestRule.getAwBrowserContext().getPrefetchManager();

        TestAwPrefetchCallback callback =
                startPrefetchAsyncAndWait(mPrefetchUrl, prefetchParameters, prefetchManager);
        callback.mOnStatusUpdatedHelper.waitForNext();

        HashMap<String, String> prefetchHeaders =
                mTestServer.getRequestHeadersForUrl(BASIC_PREFETCH_RELATIVE_PATH);
        String xRequestedWith = prefetchHeaders.get("X-Requested-With");
        Assert.assertNotNull("X-Requested-With header should be present", xRequestedWith);
        Assert.assertEquals(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName(),
                xRequestedWith);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void testPrefetchHasExpectedXRequestedWithHeader_OMTPrefetchEnabled() throws Throwable {
        String contextName = "TestContext";

        // Create a new context. This triggers a fresh `AwPrefetchManager` creation, which can pick
        // up pref's hints injected in `SetUp()`.
        AwBrowserContext context =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> AwBrowserContextStore.getNamedContext(contextName, true));
        AwPrefetchManager prefetchManager = context.getPrefetchManager();

        // PrePrefetch is triggered under the flag enabled.
        TestAwPrefetchCallback callback =
                startPrefetchAsyncAndWait(mPrefetchUrl, getAwPrefetchParameters(), prefetchManager);
        callback.mOnStatusUpdatedHelper.waitForNext();

        HashMap<String, String> prefetchHeaders =
                mTestServer.getRequestHeadersForUrl(BASIC_PREFETCH_RELATIVE_PATH);
        String xRequestedWith = prefetchHeaders.get("X-Requested-With");
        Assert.assertNotNull("X-Requested-With header should be present", xRequestedWith);
        Assert.assertEquals(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName(),
                xRequestedWith);
    }

    /**
     * Tests that the HTTP headers sent by OMT PrePrefetch exactly match the headers sent by a
     * normal UI-thread Prefetch.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
        "enable-features=PrefetchOffTheMainThread,WebViewPrefetchOffTheMainThread"
    })
    public void testPrePrefetchMatchesNormalPrefetchHeaders() throws Throwable {
        // Create a new context. This triggers a fresh `AwPrefetchManager` creation, which can pick
        // up pref's hints injected in `SetUp()`.
        String contextName = "TestContext";
        AwBrowserContext context =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> AwBrowserContextStore.getNamedContext(contextName, true));
        AwPrefetchManager prefetchManager = context.getPrefetchManager();

        String prefetchUrlPath = BASIC_PREFETCH_RELATIVE_PATH + "?type=prefetch";
        String prefetchUrl = getUrl(prefetchUrlPath);

        // 1. Normal Prefetch on UI thread.
        TestAwPrefetchCallback prefetchCallback = new TestAwPrefetchCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int prefetchKey =
                            prefetchManager.startPrefetchRequest(
                                    prefetchUrl,
                                    getAwPrefetchParameters(),
                                    prefetchCallback,
                                    Runnable::run);
                    prefetchCallback.setPrefetchKey(prefetchKey);
                });
        prefetchCallback.mOnStatusUpdatedHelper.waitForNext();
        HashMap<String, String> prefetchHeaders =
                mTestServer.getRequestHeadersForUrl(prefetchUrlPath);

        // 2. PrePrefetch on worker thread.
        String prePrefetchUrlPath = BASIC_PREFETCH_RELATIVE_PATH + "?type=preprefetch";
        String prePrefetchUrl = getUrl(prePrefetchUrlPath);

        // PrePrefetch is triggered under the flag enabled.
        TestAwPrefetchCallback prePrefetchCallback =
                startPrefetchAsyncAndWait(
                        prePrefetchUrl, getAwPrefetchParameters(), prefetchManager);
        prePrefetchCallback.mOnStatusUpdatedHelper.waitForNext();
        HashMap<String, String> prePrefetchHeaders =
                mTestServer.getRequestHeadersForUrl(prePrefetchUrlPath);

        // Verify that both normal Prefetch headers and PrePrefetch headers are equivalent.
        Assert.assertEquals(
                "Key sets do not match", prefetchHeaders.keySet(), prePrefetchHeaders.keySet());
        for (String key : prefetchHeaders.keySet()) {
            String prefetchVal = prefetchHeaders.get(key);
            String prePrefetchVal = prePrefetchHeaders.get(key);
            Assert.assertEquals("Header mismatch for " + key, prefetchVal, prePrefetchVal);
        }
    }

    private String getUrl(final String relativePath) {
        return mTestServer.getURLWithHostName("a.test", relativePath);
    }

    private static AwPrefetchParameters getAwPrefetchParameters() {
        AwNoVarySearchData expectedNoVarySearch =
                new AwNoVarySearchData(false, false, new String[] {"ts", "uid"}, null);
        Map<String, String> additionalHeaders = new HashMap<>();
        additionalHeaders.put("foo", "bar");
        additionalHeaders.put("lorem", "ipsum");
        AwPrefetchParameters prefetchParameters =
                new AwPrefetchParameters(additionalHeaders, expectedNoVarySearch, true);
        return prefetchParameters;
    }

    private TestAwPrefetchCallback startPrefetchingAndWait(
            String url, AwPrefetchParameters prefetchParameters) {
        TestAwPrefetchCallback callback = new TestAwPrefetchCallback();

        Executor callbackExecutor = Runnable::run;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int prefetchKey =
                            mActivityTestRule
                                    .getAwBrowserContext()
                                    .getPrefetchManager()
                                    .startPrefetchRequest(
                                            url, prefetchParameters, callback, callbackExecutor);
                    callback.setPrefetchKey(prefetchKey);
                });

        return callback;
    }

    // Calls `startPrefetchRequestAsync` on worker thread. If `WebViewPrefetchOffTheMainThread`
    // is enabled, it will start PrePrefetch on worker thread.
    private TestAwPrefetchCallback startPrefetchAsyncAndWait(
            String url, AwPrefetchParameters parameters, AwPrefetchManager prefetchManager)
            throws Exception {

        boolean omtEnabled = AwPrefetchManager.isWebViewPrefetchOffTheMainThreadEnabled();

        HistogramWatcher.Builder builder = HistogramWatcher.newBuilder();
        if (omtEnabled) {
            // This histogram is only recorded when the Prefetch is posted on the main thread,
            // which means that PrePrefetch is failed and fallback to normal prefetch when the flag
            // is enabled.
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
                parameters,
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

    /**
     * A class to map the TestDelegate for handling the callback checks, see {@link CallbackHelper}
     * javadocs for more details.
     */
    static class TestAwPrefetchCallback implements AwPrefetchCallback {

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
