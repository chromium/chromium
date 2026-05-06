// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.WorkerThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.AwPrefetchCallback.StatusCode;
import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

@JNINamespace("android_webview")
@Lifetime.Profile
public class AwPrefetchManager {

    private static final String QUEUED_PREFETCH_EXECUTION_DELAY_HISTOGRAM_NAME =
            "Android.WebView.Profile.Prefetch.QueuedPrefetchExecutionDelay";
    private static final String API_CALL_RESULT_HISTOGRAM_NAME =
            "Android.WebView.Profile.Prefetch.ApiCallResult";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(AwPrefetchApiCallResult)
    @IntDef({
        ApiCallResult.UI_THREAD_SUCCESS,
        ApiCallResult.UI_THREAD_FAILURE,
        ApiCallResult.WORKER_THREAD_PRE_PREFETCH_SUCCESS,
        ApiCallResult.WORKER_THREAD_PREFETCH_SUCCESS,
        ApiCallResult.WORKER_THREAD_PREFETCH_FAILURE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ApiCallResult {
        /** Called on UI thread and successfully created a prefetch request. */
        int UI_THREAD_SUCCESS = 0;

        /** Called on UI thread but failed to create a prefetch request (e.g. validation fail). */
        int UI_THREAD_FAILURE = 1;

        /** Called on worker thread, feature enabled, and successfully started pre-prefetch. */
        int WORKER_THREAD_PRE_PREFETCH_SUCCESS = 2;

        /**
         * Called on worker thread, fell back to UI thread (feature disabled or pre-prefetch
         * failed), and successfully started normal prefetch.
         */
        int WORKER_THREAD_PREFETCH_SUCCESS = 3;

        /**
         * Called on worker thread, failed to start prefetch. Covers validation failure on worker
         * thread and failure of fallback request on UI thread.
         */
        int WORKER_THREAD_PREFETCH_FAILURE = 4;

        int COUNT = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AwPrefetchApiCallResult)

    private static void recordApiCallResult(@ApiCallResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                API_CALL_RESULT_HISTOGRAM_NAME, result, ApiCallResult.COUNT);
    }

    private final long mNativePrefetchManager;

    private final Queue<Runnable> mQueuedPrefetchRequests = new ConcurrentLinkedQueue<>();

    /**
     * A flag to prevent scheduling {@link #executeQueuedPrefetchRequests()} multiple times
     * redundantly.
     */
    private final AtomicBoolean mIsPrefetchExecutionScheduled = new AtomicBoolean(false);

    @Nullable private CallbackForTesting mCallbackForTesting;

    public interface CallbackForTesting {
        void onPrefetchExecuted();
    }

    public AwPrefetchManager(long nativePrefetchManager) {
        mNativePrefetchManager = nativePrefetchManager;
    }

    @CalledByNative
    private static AwPrefetchManager create(long nativePrefetchManager) {
        return new AwPrefetchManager(nativePrefetchManager);
    }

    public static boolean isSecPurposeForPrefetch(String secPurposeHeaderValue) {
        return AwPrefetchManagerJni.get().isSecPurposeForPrefetch(secPurposeHeaderValue);
    }

    /** Returns whether starting prefetch requests off the main thread is enabled. */
    public static boolean isWebViewPrefetchOffTheMainThreadEnabled() {
        return AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_PREFETCH_OFF_THE_MAIN_THREAD)
                && ContentFeatureMap.isEnabled(ContentFeatureList.PREFETCH_OFF_THE_MAIN_THREAD);
    }

    @Nullable
    @AnyThread
    private static Exception getStartPrefetchErrorOrNull(
            String url, AwPrefetchParameters prefetchParameters) {
        final Exception error;
        if (!UrlUtilities.isHttps(url)) {
            error = new IllegalArgumentException("URL must have HTTPS scheme for prefetch.");
        } else if (prefetchParameters != null) {
            error = AwContents.validateAdditionalHeaders(prefetchParameters.getAdditionalHeaders());
        } else {
            error = null;
        }
        return error;
    }

    @NonNull
    private static String buildStartPrefetchTraceArgs(
            String url, AwPrefetchParameters prefetchParameters) {
        String traceArgs;
        if (prefetchParameters != null && prefetchParameters.getExpectedNoVarySearch() != null) {
            traceArgs =
                    String.format(
                            "{\n    url: %s, \n    nvs_hint: %s\n}",
                            url, prefetchParameters.getExpectedNoVarySearch());
        } else {
            traceArgs = String.format("{\n    url: %s\n}", url);
        }
        return traceArgs;
    }

    public void setCallbackForTesting(@Nullable CallbackForTesting callbackForTesting) {
        mCallbackForTesting = callbackForTesting;
    }

    @UiThread
    public int startPrefetchRequest(
            @NonNull String url,
            @Nullable AwPrefetchParameters prefetchParameters,
            @NonNull AwPrefetchCallback callback,
            @NonNull Executor callbackExecutor) {
        int prefetchKey =
                startPrefetchRequestInternal(url, prefetchParameters, callback, callbackExecutor);
        if (prefetchKey != AwPrefetchManagerJni.get().getNoPrefetchKey()) {
            recordApiCallResult(ApiCallResult.UI_THREAD_SUCCESS);
        } else {
            recordApiCallResult(ApiCallResult.UI_THREAD_FAILURE);
        }
        return prefetchKey;
    }

    @UiThread
    private int startPrefetchRequestInternal(
            @NonNull String url,
            @Nullable AwPrefetchParameters prefetchParameters,
            @NonNull AwPrefetchCallback callback,
            @NonNull Executor callbackExecutor) {
        assert ThreadUtils.runningOnUiThread();
        Exception error = getStartPrefetchErrorOrNull(url, prefetchParameters);
        if (error != null) {
            callbackExecutor.execute(() -> callback.onError(error));
            return AwPrefetchManagerJni.get().getNoPrefetchKey();
        }
        String traceArgs = buildStartPrefetchTraceArgs(url, prefetchParameters);
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.Prefetch.START", traceArgs)) {
            int prefetchKey =
                    AwPrefetchManagerJni.get()
                            .startPrefetchRequest(
                                    mNativePrefetchManager,
                                    url,
                                    prefetchParameters,
                                    callback,
                                    callbackExecutor);
            if (mCallbackForTesting != null) {
                mCallbackForTesting.onPrefetchExecuted();
            }
            return prefetchKey;
        }
    }

    @UiThread
    public int startPrefetchFromPrePrefetch(int prePrefetchKey) {
        assert ThreadUtils.runningOnUiThread();
        // TODO(crbug.com/452406598): Redefine an appropriate `TraceEvent`.
        int prefetchKey =
                AwPrefetchManagerJni.get()
                        .startPrefetchFromPrePrefetch(mNativePrefetchManager, prePrefetchKey);
        if (mCallbackForTesting != null) {
            mCallbackForTesting.onPrefetchExecuted();
        }
        return prefetchKey;
    }

    @WorkerThread
    public void startPrefetchRequestAsync(
            long prefetchApiCallTriggerTimeMs,
            @NonNull String url,
            @Nullable AwPrefetchParameters prefetchParameters,
            @NonNull AwPrefetchCallback callback,
            @NonNull Executor callbackExecutor,
            @NonNull Consumer<Integer> prefetchKeyListener) {
        assert !ThreadUtils.runningOnUiThread();

        if (isWebViewPrefetchOffTheMainThreadEnabled()) {
            Exception error = getStartPrefetchErrorOrNull(url, prefetchParameters);
            if (error != null) {
                callbackExecutor.execute(() -> callback.onError(error));
                prefetchKeyListener.accept(AwPrefetchManagerJni.get().getNoPrefetchKey());
                // Fails immediately without attempting fallback.
                recordApiCallResult(ApiCallResult.WORKER_THREAD_PREFETCH_FAILURE);
                return;
            }

            int prePrefetchKey =
                    AwPrefetchManagerJni.get()
                            .startPrePrefetchRequest(
                                    mNativePrefetchManager,
                                    url,
                                    prefetchParameters,
                                    callback,
                                    callbackExecutor);

            if (prePrefetchKey != AwPrefetchManagerJni.get().getNoPrefetchKey()) {
                prefetchKeyListener.accept(prePrefetchKey);
                recordApiCallResult(ApiCallResult.WORKER_THREAD_PRE_PREFETCH_SUCCESS);

                Runnable startPrefetchRunnable =
                        () -> {
                            int prefetchKey = startPrefetchFromPrePrefetch(prePrefetchKey);
                            assert prefetchKey == prePrefetchKey;
                        };
                mQueuedPrefetchRequests.offer(startPrefetchRunnable);

                // Atomically check if the prefetch execution is scheduled, and if not, set it to
                // true and schedule.
                if (mIsPrefetchExecutionScheduled.compareAndSet(false, true)) {
                    ThreadUtils.postOnUiThread(this::executeQueuedPrefetchRequests);
                }
                return;
            }
            // Fallback to normal prefetch.
        }

        Runnable startPrefetchRunnable =
                () -> {
                    long startDelayMs = SystemClock.uptimeMillis() - prefetchApiCallTriggerTimeMs;
                    int prefetchKey =
                            startPrefetchRequestInternal(
                                    url, prefetchParameters, callback, callbackExecutor);
                    prefetchKeyListener.accept(prefetchKey);

                    if (prefetchKey != AwPrefetchManagerJni.get().getNoPrefetchKey()) {
                        recordApiCallResult(ApiCallResult.WORKER_THREAD_PREFETCH_SUCCESS);
                        // Log the delay only if the prefetch was actually sent.
                        RecordHistogram.recordTimesHistogram(
                                QUEUED_PREFETCH_EXECUTION_DELAY_HISTOGRAM_NAME, startDelayMs);
                    } else {
                        recordApiCallResult(ApiCallResult.WORKER_THREAD_PREFETCH_FAILURE);
                    }
                };
        mQueuedPrefetchRequests.offer(startPrefetchRunnable);

        // Atomically check if the prefetch execution is scheduled, and if not, set it to true
        // and schedule.
        if (mIsPrefetchExecutionScheduled.compareAndSet(false, true)) {
            ThreadUtils.postOnUiThread(this::executeQueuedPrefetchRequests);
        }
    }

    @UiThread
    public void executeQueuedPrefetchRequests() {
        assert ThreadUtils.runningOnUiThread();
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.Prefetch.EXECUTE_SCHEDULED_REQUESTS")) {
            mIsPrefetchExecutionScheduled.set(false);
            Runnable prefetchTask;
            while ((prefetchTask = mQueuedPrefetchRequests.poll()) != null) {
                prefetchTask.run();
            }
        }
    }

    @UiThread
    public void cancelPrefetch(final int prefetchKey) {
        assert ThreadUtils.runningOnUiThread();
        AwPrefetchManagerJni.get().cancelPrefetch(mNativePrefetchManager, prefetchKey);
    }

    @UiThread
    public boolean getIsPrefetchInCacheForTesting(final int prefetchKey) {
        // IN-TESTS
        assert ThreadUtils.runningOnUiThread();
        return AwPrefetchManagerJni.get()
                .getIsPrefetchInCacheForTesting(mNativePrefetchManager, prefetchKey);
    }

    @UiThread
    public void updatePrefetchConfiguration(int prefetchTTLSeconds, int maxPrefetches) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.Prefetch.SET_SPECULATIVE_LOADING_CONFIG")) {
            assert ThreadUtils.runningOnUiThread();
            if (prefetchTTLSeconds > 0) {
                AwPrefetchManagerJni.get().setTtlInSec(mNativePrefetchManager, prefetchTTLSeconds);
            }

            if (maxPrefetches > 0) {
                AwPrefetchManagerJni.get().setMaxPrefetches(mNativePrefetchManager, maxPrefetches);
            }
        }
    }

    @UiThread
    public void setMaxPrefetches(int maxPrefetches) {
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.Prefetch.SET_MAX_PREFETCHES")) {
            assert ThreadUtils.runningOnUiThread();
            AwPrefetchManagerJni.get().setMaxPrefetches(mNativePrefetchManager, maxPrefetches);
        }
    }

    @UiThread
    public void setPrefetchTtlSeconds(int prefetchTtlSeconds) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.Prefetch.SET_PREFETCH_TTL_SECONDS")) {
            assert ThreadUtils.runningOnUiThread();
            AwPrefetchManagerJni.get().setTtlInSec(mNativePrefetchManager, prefetchTtlSeconds);
        }
    }

    @UiThread
    public void clearMaxPrefetches() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.Prefetch.CLEAR_MAX_PREFETCHES")) {
            assert ThreadUtils.runningOnUiThread();
            AwPrefetchManagerJni.get().clearMaxPrefetches(mNativePrefetchManager);
        }
    }

    @UiThread
    public void clearPrefetchTtl() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.Prefetch.CLEAR_PREFETCH_TTL_SECONDS")) {
            assert ThreadUtils.runningOnUiThread();
            AwPrefetchManagerJni.get().clearTtl(mNativePrefetchManager);
        }
    }

    @UiThread
    public int getMaxPrefetches() {
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.Prefetch.GET_MAX_PREFETCHES")) {
            assert ThreadUtils.runningOnUiThread();
            return AwPrefetchManagerJni.get().getMaxPrefetches(mNativePrefetchManager);
        }
    }

    @UiThread
    public int getPrefetchTtlSeconds() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.Prefetch.GET_PREFETCH_TTL_SECONDS")) {
            assert ThreadUtils.runningOnUiThread();
            return AwPrefetchManagerJni.get().getTtlInSec(mNativePrefetchManager);
        }
    }

    public int getNoPrefetchKeyForTesting() {
        return AwPrefetchManagerJni.get().getNoPrefetchKey();
    }

    @CalledByNative
    public void onPrefetchStartFailedGeneric(
            AwPrefetchCallback callback, Executor callbackExecutor) {
        callbackExecutor.execute(
                () -> callback.onStatusUpdated(StatusCode.PREFETCH_START_FAILED, null));
    }

    @CalledByNative
    public void onPrefetchStartFailedDuplicate(
            AwPrefetchCallback callback, Executor callbackExecutor) {
        callbackExecutor.execute(
                () -> callback.onStatusUpdated(StatusCode.DUPLICATE_REQUEST, null));
    }

    @CalledByNative
    public void onPrefetchResponseCompleted(
            AwPrefetchCallback callback, Executor callbackExecutor) {
        callbackExecutor.execute(
                () -> callback.onStatusUpdated(StatusCode.PREFETCH_RESPONSE_COMPLETED, null));
    }

    @CalledByNative
    public void onPrefetchResponseError(AwPrefetchCallback callback, Executor callbackExecutor) {
        callbackExecutor.execute(
                () -> callback.onStatusUpdated(StatusCode.PREFETCH_RESPONSE_GENERIC_ERROR, null));
    }

    @CalledByNative
    public void onPrefetchResponseServerError(
            AwPrefetchCallback callback, Executor callbackExecutor, int httpResponseCode) {
        Bundle extras = new Bundle();
        extras.putInt(AwPrefetchCallback.EXTRA_HTTP_RESPONSE_CODE, httpResponseCode);
        callbackExecutor.execute(
                () -> callback.onStatusUpdated(StatusCode.PREFETCH_RESPONSE_SERVER_ERROR, extras));
    }

    @NativeMethods
    interface Natives {

        int getNoPrefetchKey();

        boolean isSecPurposeForPrefetch(@JniType("std::string") String secPurposeHeaderValue);

        // Returns the prefetch key used to cancel the request.
        int startPrefetchRequest(
                long nativeAwPrefetchManager,
                @JniType("std::string") String url,
                AwPrefetchParameters prefetchParameters,
                AwPrefetchCallback callback,
                Executor callbackExecutor);

        int startPrePrefetchRequest(
                long nativeAwPrefetchManager,
                @JniType("std::string") String url,
                AwPrefetchParameters prefetchParameters,
                AwPrefetchCallback callback,
                Executor callbackExecutor);

        int startPrefetchFromPrePrefetch(long nativeAwPrefetchManager, int prefetchKey);

        /**
         * Attempts the cancel the prefetch request using the key returned from {@link
         * Natives#startPrefetchRequest(long, String, AwPrefetchParameters, AwPrefetchCallback,
         * Executor)}.
         */
        void cancelPrefetch(long nativeAwPrefetchManager, int prefetchKey);

        // IN-TESTS
        boolean getIsPrefetchInCacheForTesting(long nativeAwPrefetchManager, int prefetchKey);

        void setTtlInSec(long nativeAwPrefetchManager, int ttlInSeconds);

        void setMaxPrefetches(long nativeAwPrefetchManager, int maxPrefetches);

        void clearTtl(long nativeAwPrefetchManager);

        void clearMaxPrefetches(long nativeAwPrefetchManager);

        int getTtlInSec(long nativeAwPrefetchManager);

        int getMaxPrefetches(long nativeAwPrefetchManager);
    }
}
