// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.os.CancellationSignal;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.reflect.InvocationHandler;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;

/** Boundary interface for Profile. */
@NullMarked
public interface ProfileBoundaryInterface {
    String getName();

    CookieManager getCookieManager();

    WebStorage getWebStorage();

    GeolocationPermissions getGeoLocationPermissions();

    ServiceWorkerController getServiceWorkerController();

    void prefetchUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback);

    void prefetchUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* SpeculativeLoadingParameters */ InvocationHandler speculativeLoadingParams,
            /* PrefetchOperationCallback */ InvocationHandler callback);

    void clearPrefetch(
            String url,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback);

    void setSpeculativeLoadingConfig(/* SpeculativeLoadingConfig */ InvocationHandler config);

    void warmUpRendererProcess();

    /**
     * @deprecated Can be removed along with {@link
     *     org.chromium.support_lib_boundary.util.Features#EXTRA_HEADER_FOR_ORIGINS}
     */
    @Deprecated
    void setOriginMatchedHeader(String headerName, String headerValue, Set<String> originRules);

    void addOriginMatchedHeader(
            @NonNull String name, @NonNull String value, @NonNull Set<String> rules);

    boolean hasOriginMatchedHeader(String headerName);

    @NonNull /* List<OriginMatchedBoundaryInterface> */
            List<InvocationHandler> getOriginMatchedHeaders(
                    @Nullable String headerName, @Nullable String headerValue);

    /**
     * @deprecated Can be removed along with {@link
     *     org.chromium.support_lib_boundary.util.Features#EXTRA_HEADER_FOR_ORIGINS}
     */
    @Deprecated
    void clearOriginMatchedHeader(String headerName);

    void clearOriginMatchedHeader(@NonNull String headerName, @Nullable String headerValue);

    void clearAllOriginMatchedHeaders();

    void preconnect(String url);

    void addQuicHints(Set<String> origins);
}
