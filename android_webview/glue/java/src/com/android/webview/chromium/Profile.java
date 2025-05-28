// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.WorkerThread;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.util.concurrent.Executor;
import java.util.function.Consumer;

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
        String traceArgs = String.format("{name: \"%s\"}", browserContext.getName());
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.constructor", traceArgs)) {
            ThreadUtils.checkUiThread();
            mBrowserContext = browserContext;
            mName = browserContext.getName();

            WebViewChromiumFactoryProvider factory = WebViewChromiumFactoryProvider.getSingleton();
            if (browserContext.isDefaultAwBrowserContext()) {
                mCookieManager = factory.getCookieManager();
                mWebStorage = factory.getWebStorage();
                mGeolocationPermissions = factory.getGeolocationPermissions();
                mServiceWorkerController = factory.getServiceWorkerController();
            } else {
                mCookieManager = new CookieManagerAdapter(browserContext.getCookieManager());
                mWebStorage =
                        new WebStorageAdapter(factory, browserContext.getQuotaManagerBridge());
                mGeolocationPermissions =
                        new GeolocationPermissionsAdapter(
                                factory, browserContext.getGeolocationPermissions());
                mServiceWorkerController =
                        new ServiceWorkerControllerAdapter(
                                browserContext.getServiceWorkerController());
            }
        }
    }

    @NonNull
    public String getName() {
        return mName;
    }

    @NonNull
    public CookieManager getCookieManager() {
        String traceArgs = String.format("{name: \"%s\"}", mName);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.ApiCall.GET_COOKIE_MANAGER", traceArgs)) {
            return mCookieManager;
        }
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

    @UiThread
    public int prefetchUrl(
            String url,
            @Nullable PrefetchParams params,
            Executor callbackExecutor,
            PrefetchOperationCallback resultCallback) {
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.ApiCall.Prefetch.PRE_START")) {
            validatePrefetchArgs(url, resultCallback);
            return mBrowserContext
                    .getPrefetchManager()
                    .startPrefetchRequest(
                            url,
                            params == null ? null : params.toAwPrefetchParams(),
                            new ProfileWebViewPrefetchCallback(callbackExecutor, resultCallback),
                            callbackExecutor);
        }
    }

    @WorkerThread
    public void prefetchUrlAsync(
            long prefetchApiCallTriggerTimeMs,
            String url,
            @Nullable PrefetchParams params,
            Executor callbackExecutor,
            PrefetchOperationCallback resultCallback,
            Consumer<Integer> prefetchKeyListener) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.ApiCall.Prefetch.PRE_START_ASYNC")) {
            validatePrefetchArgs(url, resultCallback);
            mBrowserContext
                    .getPrefetchManager()
                    .startPrefetchRequestAsync(
                            prefetchApiCallTriggerTimeMs,
                            url,
                            params == null ? null : params.toAwPrefetchParams(),
                            new ProfileWebViewPrefetchCallback(callbackExecutor, resultCallback),
                            callbackExecutor,
                            prefetchKeyListener);
        }
    }

    @UiThread
    public void clearPrefetch(String url, PrefetchOperationCallback resultCallback) {
        // TODO(334016945): do the actual implementation
    }

    @UiThread
    public void cancelPrefetch(int prefetchKey) {
        // TODO(334016945): do the actual implementation
    }

    @UiThread
    public void setSpeculativeLoadingConfig(SpeculativeLoadingConfig speculativeLoadingConfig) {
        mBrowserContext
                .getPrefetchManager()
                .updatePrefetchConfiguration(
                        speculativeLoadingConfig.prefetchTTLSeconds,
                        speculativeLoadingConfig.maxPrefetches);
        if (speculativeLoadingConfig.maxPrerenders > 0) {
            mBrowserContext.setMaxPrerenders(speculativeLoadingConfig.maxPrerenders);
        }
    }

    private static void validatePrefetchArgs(String url, PrefetchOperationCallback resultCallback) {
        if (url == null) {
            throw new IllegalArgumentException("URL cannot be null for prefetch.");
        }

        if (resultCallback == null) {
            throw new IllegalArgumentException("Callback cannot be null for prefetch.");
        }
    }

    @UiThread
    public void warmUpRendererProcess() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.ApiCall.WARM_UP_RENDERER_PROCESS")) {
            mBrowserContext.warmUpSpareRenderer();
        }
    }
}
