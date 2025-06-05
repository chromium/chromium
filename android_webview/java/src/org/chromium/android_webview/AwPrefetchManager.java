// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.AwPrefetchCallback.StatusCode;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.ContentFeatureList;

import java.util.Optional;
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

    @Nullable
    private static Exception getStartPrefetchErrorOrNull(
            String url, AwPrefetchParameters prefetchParameters) {
        final Exception error;
        if (!UrlUtilities.isHttps(url)) {
            error = new IllegalArgumentException("URL must have HTTPS scheme for prefetch.");
        } else if (!AwFeatureMap.isEnabled(
                ContentFeatureList.PREFETCH_BROWSER_INITIATED_TRIGGERS)) {
            error =
                    new IllegalStateException(
                            "WebView initiated prefetching feature is not" + " enabled.");
        } else if (prefetchParameters != null) {
            Optional<IllegalArgumentException> exception =
                    AwBrowserContext.validateAdditionalHeaders(
                            prefetchParameters.getAdditionalHeaders());
            if (exception.isPresent()) {
                error = exception.get();
            } else {
                error = null;
            }
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

    @WorkerThread
    public void startPrefetchRequestAsync(
            long prefetchApiCallTriggerTimeMs,
            @NonNull String url,
            @Nullable AwPrefetchParameters prefetchParameters,
            @NonNull AwPrefetchCallback callback,
            @NonNull Executor callbackExecutor,
            @NonNull Consumer<Integer> prefetchKeyListener) {
        assert !ThreadUtils.runningOnUiThread();
        Runnable startPrefetchRunnable =
                () -> {
                    long startDelayMs = SystemClock.uptimeMillis() - prefetchApiCallTriggerTimeMs;
                    int prefetchKey =
                            startPrefetchRequest(
                                    url, prefetchParameters, callback, callbackExecutor);
                    prefetchKeyListener.accept(prefetchKey);

                    // Log the delay only if the prefetch was actually sent.
                    if (prefetchKey != AwPrefetchManagerJni.get().getNoPrefetchKey()) {
                        RecordHistogram.recordTimesHistogram(
                                QUEUED_PREFETCH_EXECUTION_DELAY_HISTOGRAM_NAME, startDelayMs);
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

    @VisibleForTesting
    public int getTTlInSec() {
        return AwPrefetchManagerJni.get().getTtlInSec(mNativePrefetchManager);
    }

    @VisibleForTesting
    public int getMaxPrefetches() {
        return AwPrefetchManagerJni.get().getMaxPrefetches(mNativePrefetchManager);
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
                () -> callback.onStatusUpdated(StatusCode.PREFETCH_START_FAILED_DUPLICATE, null));
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

        int getTtlInSec(long nativeAwPrefetchManager);

        int getMaxPrefetches(long nativeAwPrefetchManager);
    }
}
