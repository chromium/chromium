// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.ValueCallback;
import android.webkit.WebStorage;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwPrefetchOperationCallback;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.android_webview.AwPrefetchStartResultCode;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.util.concurrent.Executor;

/**
 * An abstraction of {@link AwBrowserContext}, this class reflects the state needed for the
 * multi-profile public API.
 */
@Lifetime.Profile
public class Profile {

    @NonNull private final AwBrowserContext mBrowserContext;

    @NonNull private final String mName;

    @NonNull private final CookieManager mCookieManager;

    @NonNull private final WebStorage mWebStorage;

    @NonNull private final GeolocationPermissions mGeolocationPermissions;

    @NonNull private final ServiceWorkerController mServiceWorkerController;

    public Profile(@NonNull final AwBrowserContext browserContext) {
        assert ThreadUtils.runningOnUiThread();
        WebViewChromiumFactoryProvider factory = WebViewChromiumFactoryProvider.getSingleton();
        mBrowserContext = browserContext;
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

    @AnyThread
    public void prefetchUrl(
            String url,
            @Nullable PrefetchParams params,
            ValueCallback<PrefetchOperationResult> resultCallback) {
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.Prefetch.PRE_START")) {
            if (url == null) {
                throw new IllegalArgumentException("URL cannot be null for prefetch.");
            }

            if (resultCallback == null) {
                throw new IllegalArgumentException("Callback cannot be null for prefetch.");
            }

            AwPrefetchParameters awPrefetchParameters =
                    params == null ? null : params.toAwPrefetchParams();
            AwPrefetchOperationCallback<Integer> awCallback =
                    new AwPrefetchOperationCallback<>() {
                        @Override
                        public void onResult(@AwPrefetchStartResultCode Integer result) {
                            resultCallback.onReceiveValue(
                                    PrefetchOperationResult.fromStartResultCode(result));
                        }

                        @Override
                        public void onError(Throwable e) {
                            resultCallback.onReceiveValue(
                                    new PrefetchOperationResult(
                                            PrefetchOperationStatusCode.FAILURE));
                        }
                    };
            Executor callingThreadExecutor = Runnable::run;
            ThreadUtils.runOnUiThread(
                    () ->
                            mBrowserContext.startPrefetchRequest(
                                    url, awPrefetchParameters, awCallback, callingThreadExecutor));
        }
    }

    public void clearPrefetch(String url, ValueCallback<PrefetchOperationResult> resultCallback) {
        // TODO(334016945): do the actual implementation
    }

    public void cancelPrefetch(String url, ValueCallback<PrefetchOperationResult> resultCallback) {
        // TODO(334016945): do the actual implementation
    }
}
