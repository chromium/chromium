// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.SystemClock;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Test;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwPrefetchManager;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.android_webview.test.util.AwPrefetchTestUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;

import java.util.HashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Base class containing all OffTheMainThread (OMT) prefetch tests. Concrete subclasses instantiate
 * this test suite with specific OMT feature configurations.
 */
public abstract class AwPrefetchOmtTestBase extends AwPrefetchTestBase {

    public AwPrefetchOmtTestBase(AwSettingsMutation param) {
        super(param, /* runOnWorkerThread= */ true);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrefetchQueueDrainedWhenUiThreadIsFree_VerifyPrefetchExecutionCount() {
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
    public void testPrefetchQueueExplicitlyDrainedDuringAwContentsInitAndLoadUrl() {
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

    /**
     * Tests that a PrePrefetch is triggered and completed successfully, and successfully served to
     * a loadUrl.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrePrefetchServedAndConsumed() throws Throwable {
        // PrePrefetch is triggered under the flag enabled.
        TestAwPrefetchCallback callback =
                startPrefetchAndWait(mPrefetchUrl, getAwPrefetchParameters());

        callback.getOnStatusUpdatedHelper().waitForNext();
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
                createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), mPrefetchUrl);

        // Verify that the server did NOT receive a second request, proving the page load
        // was served from the PrePrefetch, not Prefetch and the loadUrl itself.
        Assert.assertEquals(
                "Server should NOT have received a second request.",
                1,
                mTestServer.getRequestCountForUrl(BASIC_PREFETCH_RELATIVE_PATH));
    }

    /**
     * Tests that if PrePrefetch fails the request falls back to a standard UI thread Prefetch
     * request.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
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

        // Call `startPrefetchRequestAsync()` directly here because we expect the PrePrefetch to
        // fail and fallback to standard prefetch on the UI thread. The helper method
        // `startPrefetchAndWait()` has strict assertions that the key listener must be called
        // on a background worker thread, which is not true in this fallback case.
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
        callback.getOnStatusUpdatedHelper().waitForNext();
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
     * Tests that the HTTP headers sent by OMT PrePrefetch exactly match the headers sent by a
     * normal UI-thread Prefetch.
     */
    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testPrePrefetchMatchesNormalPrefetchHeaders() throws Throwable {
        String prefetchUrlPath = BASIC_PREFETCH_RELATIVE_PATH + "?type=prefetch";
        String prefetchUrl = getUrl(prefetchUrlPath);

        // 1. Normal Prefetch on UI thread.
        TestAwPrefetchCallback prefetchCallback =
                startPrefetchAndWait(
                        /* runOnWorkerThread= */ false,
                        prefetchUrl,
                        getAwPrefetchParameters(),
                        mPrefetchManager);
        prefetchCallback.getOnStatusUpdatedHelper().waitForNext();
        HashMap<String, String> prefetchHeaders =
                mTestServer.getRequestHeadersForUrl(prefetchUrlPath);

        // 2. PrePrefetch on worker thread.
        String prePrefetchUrlPath = BASIC_PREFETCH_RELATIVE_PATH + "?type=preprefetch";
        String prePrefetchUrl = getUrl(prePrefetchUrlPath);

        // PrePrefetch is triggered under the flag enabled.
        TestAwPrefetchCallback prePrefetchCallback =
                startPrefetchAndWait(
                        /* runOnWorkerThread= */ true,
                        prePrefetchUrl,
                        getAwPrefetchParameters(),
                        mPrefetchManager);
        prePrefetchCallback.getOnStatusUpdatedHelper().waitForNext();
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
}
