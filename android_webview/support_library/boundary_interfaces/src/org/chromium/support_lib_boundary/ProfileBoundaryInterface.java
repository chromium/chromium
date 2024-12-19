// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.os.CancellationSignal;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

import java.lang.reflect.InvocationHandler;
import java.util.concurrent.Executor;

/** Boundary interface for Profile. */
public interface ProfileBoundaryInterface {
    String getName();

    CookieManager getCookieManager();

    WebStorage getWebStorage();

    GeolocationPermissions getGeoLocationPermissions();

    ServiceWorkerController getServiceWorkerController();

    void prefetchUrl(
            String url,
            CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback);

    void prefetchUrl(
            String url,
            CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* SpeculativeLoadingParameters */ InvocationHandler speculativeLoadingParams,
            /* PrefetchOperationCallback */ InvocationHandler callback);

    void clearPrefetch(
            String url,
            Executor callbackExecutor,
            /* PrefetchOperationCallback */ InvocationHandler callback);
}
