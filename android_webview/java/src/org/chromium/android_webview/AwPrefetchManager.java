// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.AwPrefetchCallback.StatusCode;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.ContentFeatureList;

import java.util.Optional;
import java.util.concurrent.Executor;

@JNINamespace("android_webview")
@Lifetime.Profile
public class AwPrefetchManager {
    private long mNativePrefetchManager;

    public AwPrefetchManager(long nativePrefetchManager) {
        mNativePrefetchManager = nativePrefetchManager;
    }

    @CalledByNative
    private static AwPrefetchManager create(long nativePrefetchManager) {
        return new AwPrefetchManager(nativePrefetchManager);
    }

    @UiThread
    public int startPrefetchRequest(
            @NonNull String url,
            @Nullable AwPrefetchParameters prefetchParameters,
            @NonNull AwPrefetchCallback callback,
            @NonNull Executor callbackExecutor) {
        assert ThreadUtils.runningOnUiThread();
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.Prefetch.START")) {
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

            if (error != null) {
                callbackExecutor.execute(() -> callback.onError(error));
                return AwPrefetchManagerJni.get().getNoPrefetchKey();
            }

            return AwPrefetchManagerJni.get()
                    .startPrefetchRequest(
                            mNativePrefetchManager,
                            url,
                            prefetchParameters,
                            callback,
                            callbackExecutor);
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
