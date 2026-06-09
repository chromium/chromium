// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.os.CancellationSignal;
import android.os.SystemClock;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.webview.chromium.PrefetchOperationCallback;
import com.android.webview.chromium.PrefetchOperationStatusCode;
import com.android.webview.chromium.PrefetchParams;
import com.android.webview.chromium.Profile;
import com.android.webview.chromium.SpeculativeLoadingConfig;

import org.chromium.android_webview.AwOriginMatchedHeader;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.PrefetchOperationCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.ProfileBoundaryInterface;
import org.chromium.support_lib_boundary.SpeculativeLoadingConfigBoundaryInterface;
import org.chromium.support_lib_boundary.SpeculativeLoadingParametersBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

/** The support-lib glue implementation for Profile, delegates all the calls to {@link Profile}. */
@Lifetime.Profile
public class SupportLibProfile implements ProfileBoundaryInterface {
    private final Profile mProfileImpl;

    public SupportLibProfile(@NonNull Profile profile) {
        mProfileImpl = profile;
    }

    @NonNull
    @Override
    public String getName() {
        recordApiCall(ApiCall.GET_PROFILE_NAME);
        return mProfileImpl.getName();
    }

    @NonNull
    @Override
    public CookieManager getCookieManager() {
        recordApiCall(ApiCall.GET_PROFILE_COOKIE_MANAGER);
        return mProfileImpl.getCookieManager();
    }

    @NonNull
    @Override
    public WebStorage getWebStorage() {
        recordApiCall(ApiCall.GET_PROFILE_WEB_STORAGE);
        return mProfileImpl.getWebStorage();
    }

    @NonNull
    @Override
    public GeolocationPermissions getGeoLocationPermissions() {
        recordApiCall(ApiCall.GET_PROFILE_GEO_LOCATION_PERMISSIONS);
        return mProfileImpl.getGeolocationPermissions();
    }

    @NonNull
    @Override
    public ServiceWorkerController getServiceWorkerController() {
        recordApiCall(ApiCall.GET_PROFILE_SERVICE_WORKER_CONTROLLER);
        return mProfileImpl.getServiceWorkerController();
    }

    @Override
    public void prefetchUrl(
            String url,
            CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        recordApiCall(ApiCall.PREFETCH_URL);
        prefetchUrlInternal(
                SystemClock.uptimeMillis(),
                url,
                null,
                cancellationSignal,
                callbackExecutor,
                createOperationCallback(callback));
    }

    @Override
    public void prefetchUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* SpeculativeLoadingParameters */ InvocationHandler speculativeLoadingParams,
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        recordApiCall(ApiCall.PREFETCH_URL_WITH_PARAMS);
        long apiCallTriggerTimeMs = SystemClock.uptimeMillis();
        SpeculativeLoadingParametersBoundaryInterface speculativeLoadingParameters =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        SpeculativeLoadingParametersBoundaryInterface.class,
                        speculativeLoadingParams);

        assert speculativeLoadingParameters != null;
        PrefetchParams prefetchParams =
                SupportLibSpeculativeLoadingParametersAdapter
                        .fromSpeculativeLoadingParametersBoundaryInterface(
                                speculativeLoadingParameters);
        prefetchUrlInternal(
                apiCallTriggerTimeMs,
                url,
                prefetchParams,
                cancellationSignal,
                callbackExecutor,
                createOperationCallback(callback));
    }

    private void prefetchUrlInternal(
            long apiCallTriggerTimeMs,
            String url,
            @Nullable PrefetchParams prefetchParams,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            PrefetchOperationCallback callback) {
        if (ThreadUtils.runningOnUiThread()) {
            int prefetchKey =
                    mProfileImpl.prefetchUrl(url, prefetchParams, callbackExecutor, callback);
            setCancelListener(cancellationSignal, prefetchKey);
        } else {
            Consumer<Integer> prefetchKeyListener =
                    prefetchKey -> setCancelListener(cancellationSignal, prefetchKey);
            mProfileImpl.prefetchUrlAsync(
                    apiCallTriggerTimeMs,
                    url,
                    prefetchParams,
                    callbackExecutor,
                    callback,
                    prefetchKeyListener);
        }
    }

    public void setCancelListener(CancellationSignal cancellationSignal, int prefetchKey) {
        if (cancellationSignal != null) {
            cancellationSignal.setOnCancelListener(
                    () -> {
                        recordApiCall(ApiCall.CANCEL_PREFETCH);
                        mProfileImpl.cancelPrefetch(prefetchKey);
                    });
        }
    }

    @Override
    public void clearPrefetch(
            String url,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        // Keeping this around so we don't break the Boundary Interface.
        // The method itself is deprecated.
    }

    @Override
    public void setMaxPrerenders(int maxPrerenders) {
        recordApiCall(ApiCall.SET_MAX_PRERENDERS);
        mProfileImpl.setMaxPrerenders(maxPrerenders);
    }

    @Override
    public void setMaxPrerenders(@Nullable Integer maxPrerenders) {
        recordApiCall(ApiCall.SET_MAX_PRERENDERS);
        mProfileImpl.setMaxPrerenders(maxPrerenders);
    }

    @Override
    public void clearMaxPrerenders() {
        recordApiCall(ApiCall.CLEAR_MAX_PRERENDERS);
        mProfileImpl.clearMaxPrerenders();
    }

    @Override
    public void setMaxPrefetches(@Nullable Integer maxPrefetches) {
        recordApiCall(ApiCall.SET_MAX_PREFETCHES);
        mProfileImpl.setMaxPrefetches(maxPrefetches);
    }

    @Override
    public void setPrefetchTtlSeconds(@Nullable Integer prefetchTtlSeconds) {
        recordApiCall(ApiCall.SET_PREFETCH_TTL_SECONDS);
        mProfileImpl.setPrefetchTtlSeconds(prefetchTtlSeconds);
    }

    @Override
    public void setMaxPrefetches(int maxPrefetches) {
        recordApiCall(ApiCall.SET_MAX_PREFETCHES);
        mProfileImpl.setMaxPrefetches(maxPrefetches);
    }

    @Override
    public void setPrefetchTtlSeconds(int prefetchTtlSeconds) {
        recordApiCall(ApiCall.SET_PREFETCH_TTL_SECONDS);
        mProfileImpl.setPrefetchTtlSeconds(prefetchTtlSeconds);
    }

    @Override
    public void clearMaxPrefetches() {
        recordApiCall(ApiCall.CLEAR_MAX_PREFETCHES);
        mProfileImpl.clearMaxPrefetches();
    }

    @Override
    public void clearPrefetchTtl() {
        recordApiCall(ApiCall.CLEAR_PREFETCH_TTL);
        mProfileImpl.clearPrefetchTtl();
    }

    @Override
    public int getMaxPrerenders() {
        recordApiCall(ApiCall.GET_MAX_PRERENDERS);
        return mProfileImpl.getMaxPrerenders();
    }

    @Override
    public int getMaxPrefetches() {
        recordApiCall(ApiCall.GET_MAX_PREFETCHES);
        return mProfileImpl.getMaxPrefetches();
    }

    @Override
    public int getPrefetchTtlSeconds() {
        recordApiCall(ApiCall.GET_PREFETCH_TTL_SECONDS);
        return mProfileImpl.getPrefetchTtlSeconds();
    }

    /**
     * @deprecated Can be removed along with {@link
     *     org.chromium.support_lib_boundary.util.Features#SPECULATIVE_LOADING_CONFIG}
     */
    @Deprecated
    @Override
    public void setSpeculativeLoadingConfig(
            /* SpeculativeLoadingConfig */ InvocationHandler config) {
        recordApiCall(ApiCall.SET_SPECULATIVE_LOADING_CONFIG);
        SpeculativeLoadingConfigBoundaryInterface speculativeLoadingConfig =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        SpeculativeLoadingConfigBoundaryInterface.class, config);
        mProfileImpl.setSpeculativeLoadingConfig(
                new SpeculativeLoadingConfig(
                        speculativeLoadingConfig.getMaxPrefetches(),
                        speculativeLoadingConfig.getPrefetchTTLSeconds(),
                        speculativeLoadingConfig.getMaxPrerenders()));
    }

    private PrefetchOperationCallback createOperationCallback(
            /* PrefetchOperationCallback */ InvocationHandler callback) {
        PrefetchOperationCallbackBoundaryInterface operationCallback =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        PrefetchOperationCallbackBoundaryInterface.class, callback);
        // Ensure WebMessageCallbackCompat.onMessage() is supported by the support library before
        // calling it.
        return new PrefetchOperationCallback() {
            @Override
            public void onResult(@PrefetchOperationStatusCode int resultCode) {
                String[] supportedFeatures;
                try {
                    supportedFeatures = operationCallback.getSupportedFeatures();
                } catch (IllegalArgumentException e) {
                    // PrefetchOperationCallbackBoundaryInterface did not originally implement
                    // FeatureFlagHolderBoundaryInterface, so it is possible that the call to
                    // `getSupportedFeatures` will fail with IllegalArgumentException in
                    // Method#invoke.
                    // This means that we should call the old `onSuccess` method instead.
                    operationCallback.onSuccess();
                    return;
                }
                if (BoundaryInterfaceReflectionUtil.containsFeature(
                        supportedFeatures, Features.PREFETCH_WITH_CALLBACK_RESULT_V1)) {
                    mapResult(operationCallback, resultCode);
                } else {
                    operationCallback.onSuccess();
                }
            }

            @Override
            public void onError(
                    @PrefetchOperationStatusCode int errorCode,
                    String message,
                    int networkErrorCode) {
                mapFailure(operationCallback, errorCode, message, networkErrorCode);
            }
        };
    }

    private void mapResult(
            PrefetchOperationCallbackBoundaryInterface callback,
            @PrefetchOperationStatusCode int resultCode) {
        int type =
                switch (resultCode) {
                    case PrefetchOperationStatusCode.DUPLICATE_REQUEST ->
                            PrefetchOperationCallbackBoundaryInterface
                                    .PrefetchResultTypeBoundaryInterface.DUPLICATE;
                    default ->
                            PrefetchOperationCallbackBoundaryInterface
                                    .PrefetchResultTypeBoundaryInterface.SUCCESS;
                };
        callback.onResult(type);
    }

    private void mapFailure(
            PrefetchOperationCallbackBoundaryInterface callback,
            @PrefetchOperationStatusCode int errorCode,
            String message,
            int networkErrorCode) {
        int type =
                switch (errorCode) {
                    case PrefetchOperationStatusCode.SERVER_FAILURE ->
                            PrefetchOperationCallbackBoundaryInterface
                                    .PrefetchExceptionTypeBoundaryInterface.NETWORK;
                    default ->
                            PrefetchOperationCallbackBoundaryInterface
                                    .PrefetchExceptionTypeBoundaryInterface.GENERIC;
                };
        callback.onFailure(type, message, networkErrorCode);
    }

    @Override
    public void warmUpRendererProcess() {
        ThreadUtils.checkUiThread();
        recordApiCall(ApiCall.PROFILE_WARM_UP_RENDERER_PROCESS);
        mProfileImpl.warmUpRendererProcess();
    }

    /**
     * @deprecated Can be removed along with {@link
     *     org.chromium.support_lib_boundary.util.Features#EXTRA_HEADER_FOR_ORIGINS}
     */
    @Deprecated
    @Override
    public void setOriginMatchedHeader(
            @NonNull String headerName,
            @NonNull String headerValue,
            @NonNull Set<String> originRules) {
        recordApiCall(ApiCall.SET_ORIGIN_MATCHED_HEADER);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.SET_ORIGIN_MATCHED_HEADER")) {
            mProfileImpl.setOriginMatchedHeader(headerName, headerValue, originRules);
        }
    }

    @Override
    public boolean hasOriginMatchedHeader(@NonNull String headerName) {
        recordApiCall(ApiCall.HAS_ORIGIN_MATCHED_HEADER);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.HAS_ORIGIN_MATCHED_HEADER")) {
            return mProfileImpl.hasOriginMatchedHeader(headerName);
        }
    }

    @Override
    public void addOriginMatchedHeader(
            @NonNull String name, @NonNull String value, @NonNull Set<String> rules) {
        recordApiCall(ApiCall.ADD_ORIGIN_MATCHED_HEADER);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.ADD_ORIGIN_MATCHED_HEADER")) {
            mProfileImpl.addOriginMatchedHeader(name, value, rules);
        }
    }

    @Override
    public @NonNull /* List<OriginMatchedBoundaryInterface> */ List<InvocationHandler>
            getOriginMatchedHeaders(@Nullable String headerName, @Nullable String headerValue) {
        recordApiCall(ApiCall.GET_ORIGIN_MATCHED_HEADERS);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_ORIGIN_MATCHED_HEADERS")) {
            /* List<OriginMatchedBoundaryInterface> */ List<AwOriginMatchedHeader>
                    originMatchedHeaders =
                            mProfileImpl.findOriginMatchedHeaders(headerName, headerValue);
            List<InvocationHandler> invocationHandlers =
                    new ArrayList<>(originMatchedHeaders.size());
            for (AwOriginMatchedHeader header : originMatchedHeaders) {
                invocationHandlers.add(
                        BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                new SupportLibOriginMatchedHeader(header)));
            }
            return invocationHandlers;
        }
    }

    /**
     * @deprecated Can be removed along with {@link
     *     org.chromium.support_lib_boundary.util.Features#EXTRA_HEADER_FOR_ORIGINS}
     */
    @Deprecated
    @Override
    public void clearOriginMatchedHeader(@NonNull String headerName) {
        clearOriginMatchedHeader(headerName, null);
    }

    @Override
    public void clearOriginMatchedHeader(@NonNull String headerName, @Nullable String headerValue) {
        recordApiCall(ApiCall.CLEAR_ORIGIN_MATCHED_HEADER);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.CLEAR_ORIGIN_MATCHED_HEADER")) {
            mProfileImpl.clearOriginMatchedHeader(headerName, headerValue);
        }
    }

    @Override
    public void clearAllOriginMatchedHeaders() {
        recordApiCall(ApiCall.CLEAR_ALL_ORIGIN_MATCHED_HEADERS);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.CLEAR_ALL_ORIGIN_MATCHED_HEADERS")) {
            mProfileImpl.clearAllOriginMatchedHeaders();
        }
    }

    @Override
    public void preconnect(String url) {
        recordApiCall(ApiCall.PRECONNECT);
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.PRECONNECT")) {
            mProfileImpl.preconnect(url);
        }
    }

    @Override
    public void addQuicHints(Set<String> origins) {
        recordApiCall(ApiCall.ADD_QUIC_HINTS);
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.ADD_QUIC_HINTS")) {
            mProfileImpl.addQuicHints(origins);
        }
    }

    @NonNull
    @Override
    public /* HttpCacheBoundaryInterface */ InvocationHandler getHttpCache() {
        recordApiCall(ApiCall.GET_HTTP_CACHE);
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.GET_HTTP_CACHE")) {
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibHttpCache(mProfileImpl.getHttpCacheManager()));
        }
    }
}
