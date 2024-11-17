// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.ValueCallback;
import android.webkit.WebStorage;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for Profile. */
public interface ProfileBoundaryInterface {
    String getName();

    CookieManager getCookieManager();

    WebStorage getWebStorage();

    GeolocationPermissions getGeoLocationPermissions();

    ServiceWorkerController getServiceWorkerController();

    void prefetchUrl(
            String url,
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    callback);

    void prefetchUrl(
            String url,
            /* PrefetchParamsBoundaryInterface */ InvocationHandler prefetchParams,
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    callback);

    void cancelPrefetch(
            String url,
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    callback);

    void clearPrefetch(
            String url,
            ValueCallback</* PrefetchOperationResultBoundaryInterface */ InvocationHandler>
                    callback);
}
