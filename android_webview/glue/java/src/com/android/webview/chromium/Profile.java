// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.ValueCallback;
import android.webkit.WebStorage;

import androidx.annotation.NonNull;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;

/**
 * An abstraction of {@link AwBrowserContext}, this class reflects the state needed for the
 * multi-profile public API.
 */
@Lifetime.Profile
public class Profile {
    @NonNull private final String mName;

    @NonNull private final CookieManager mCookieManager;

    @NonNull private final WebStorage mWebStorage;

    @NonNull private final GeolocationPermissions mGeolocationPermissions;

    @NonNull private final ServiceWorkerController mServiceWorkerController;

    public Profile(@NonNull final AwBrowserContext browserContext) {
        assert ThreadUtils.runningOnUiThread();
        WebViewChromiumFactoryProvider factory = WebViewChromiumFactoryProvider.getSingleton();
        mName = browserContext.getName();

        if (browserContext.isDefaultAwBrowserContext()) {
            mCookieManager = factory.getCookieManager();
            mWebStorage = factory.getWebStorage();
            mGeolocationPermissions = factory.getGeolocationPermissions();
            mServiceWorkerController = factory.getServiceWorkerController();
        } else {
            mCookieManager = new CookieManagerAdapter(browserContext.getCookieManager());
            mWebStorage = new WebStorageAdapter(factory, browserContext.getQuotaManagerBridge());
            mGeolocationPermissions =
                    new GeolocationPermissionsAdapter(
                            factory, browserContext.getGeolocationPermissions());
            mServiceWorkerController =
                    new ServiceWorkerControllerAdapter(browserContext.getServiceWorkerController());
        }
    }

    @NonNull
    public String getName() {
        return mName;
    }

    @NonNull
    public CookieManager getCookieManager() {
        return mCookieManager;
    }

    @NonNull
    public WebStorage getWebStorage() {
        return mWebStorage;
    }

    @NonNull
    public GeolocationPermissions getGeolocationPermissions() {
        return mGeolocationPermissions;
    }

    @NonNull
    public ServiceWorkerController getServiceWorkerController() {
        return mServiceWorkerController;
    }

    public void prefetchUrl(
            String url,
            PrefetchParams params,
            ValueCallback<PrefetchOperationResult> resultCallback) {
        // TODO(334016945): do the actual implementation AND add the params validation.
    }

    public void clearPrefetch(String url, ValueCallback<PrefetchOperationResult> resultCallback) {
        // TODO(334016945): do the actual implementation
    }

    public void cancelPrefetch(String url, ValueCallback<PrefetchOperationResult> resultCallback) {
        // TODO(334016945): do the actual implementation
    }
}
