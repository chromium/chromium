// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.WorkerThread;

import com.android.webview.chromium.WebViewChromiumAwInit.CallSite;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwHttpCacheManager;
import org.chromium.android_webview.AwOriginMatchedHeader;
import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

/**
 * An abstraction of {@link AwBrowserContext}, this class reflects the state needed for the
 * multi-profile public API.
 *
 * <p><b>Lifecycle Note:</b> The internal state of this class is {@code null} upon construction. It
 * remains {@code null} until a method requiring Chromium state is called on this instance, at which
 * point Chromium startup is triggered and the state is populated by {@link #initializeProfile()}.
 *
 * <p>Note that this initial population may block the calling thread while synchronizing with the UI
 * thread.
 */
@Lifetime.Profile
public class Profile {
    private static final String TAG = "Profile";

    private static class State {
        @NonNull public final AwBrowserContext browserContext;
        @NonNull public final CookieManager cookieManager;
        @NonNull public final WebStorage webStorage;
        @NonNull public final GeolocationPermissions geolocationPermissions;
        @NonNull public final ServiceWorkerController serviceWorkerController;

        public State(
                @NonNull AwBrowserContext browserContext,
                @NonNull CookieManager cookieManager,
                @NonNull WebStorage webStorage,
                @NonNull GeolocationPermissions geolocationPermissions,
                @NonNull ServiceWorkerController serviceWorkerController) {
            this.browserContext = browserContext;
            this.cookieManager = cookieManager;
            this.webStorage = webStorage;
            this.geolocationPermissions = geolocationPermissions;
            this.serviceWorkerController = serviceWorkerController;
        }
    }

    @NonNull private final String mName;
    @NonNull private final String mTraceArgs;
    @NonNull private final WebViewChromiumAwInit mAwInit;

    @MonotonicNonNull private volatile State mState;

    @AnyThread
    public Profile(@NonNull final String profileName, @NonNull WebViewChromiumAwInit awInit) {
        mAwInit = awInit;
        mName = profileName;
        mTraceArgs = String.format("{name: \"%s\"}", mName);

        if (ThreadUtils.runningOnUiThread() && mAwInit.isChromiumInitialized()) {
            initializeProfile();
        }
    }

    /**
     * Initializes the profile state. This runs synchronously on the main UI thread when an API
     * requiring Chromium state is first invoked.
     */
    void initializeProfile() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.initializeProfile", mTraceArgs)) {
            if (mState != null) return;

            ThreadUtils.checkUiThread();
            AwBrowserContext browserContext = AwBrowserContextStore.getNamedContext(mName, true);
            CookieManager cookieManager;
            WebStorage webStorage;
            GeolocationPermissions geolocationPermissions;
            ServiceWorkerController serviceWorkerController;

            WebViewChromiumFactoryProvider factory = WebViewChromiumFactoryProvider.getSingleton();
            if (browserContext.isDefaultAwBrowserContext()
                    && !WebViewCachedFlags.get()
                            .isCachedFeatureEnabled(
                                    AwFeatures.WEBVIEW_BYPASS_PROVISIONAL_COOKIE_MANAGER)) {
                cookieManager = CookieManager.getInstance();
            } else {
                cookieManager = new CookieManagerAdapter(browserContext.getCookieManager());
            }
            webStorage = new WebStorageAdapter(factory, browserContext.getQuotaManagerBridge());
            geolocationPermissions =
                    new GeolocationPermissionsAdapter(
                            factory, browserContext.getGeolocationPermissions());
            serviceWorkerController =
                    new ServiceWorkerControllerAdapter(browserContext.getServiceWorkerController());

            mState =
                    new State(
                            browserContext,
                            cookieManager,
                            webStorage,
                            geolocationPermissions,
                            serviceWorkerController);
        }
    }

    @NonNull
    private State getInitializedState(@CallSite int callSite) {
        if (mState != null) {
            return mState;
        }

        mAwInit.triggerAndWaitForChromiumStarted(callSite);

        /**
         * TODO(crbug.com/529836096): This is a temporary workaround. During async startup,
         * WebViewChromiumAwInit processes the startup run queue (drainQueue) BEFORE it re-enables
         * UI tasks via PostTask.disablePreNativeUiTasks(false). If we simply call
         * ThreadUtils.runOnUiThreadBlocking() here while already on the UI thread, PostTask will
         * see that UI tasks are still "disabled" and refuse to run the task inline. Instead, it
         * will post the task to the Android MessageQueue and synchronously block waiting for it to
         * finish (via FutureTask.get()). This causes a self-deadlock because the UI thread is
         * blocked and can never process its own MessageQueue to execute the task. By explicitly
         * checking if we are already on the UI thread, we can run the initialization directly
         * inline and safely bypass PostTask.
         */
        if (!ThreadUtils.runningOnUiThread()) {
            ThreadUtils.runOnUiThreadBlocking(this::initializeProfile);
        } else {
            initializeProfile();
        }

        // Satisfy NullAway: initializeProfile() guarantees mState is non-null.
        assert mState != null;

        return mState;
    }

    public AwBrowserContext getBrowserContext() {
        return getInitializedState(CallSite.PROFILE_GET_BROWSER_CONTEXT).browserContext;
    }

    @NonNull
    public String getName() {
        return mName;
    }

    public void preconnect(String url) {
        getInitializedState(CallSite.PROFILE_PRECONNECT)
                .browserContext
                .getPreconnector()
                .preconnect(new GURL(url));
    }

    /**
     * Enqueues a preconnect request for the given URL.
     *
     * <p>Unlike {@link #preconnect(String)}, this method is non-blocking and does not trigger
     * synchronous native Chromium initialization. The preconnect task is added to the WebView
     * startup queue and will execute asynchronously once native library initialization completes.
     *
     * @param url The target URL destination to preconnect to.
     */
    public void enqueuePreconnect(@NonNull String url) {
        if (url == null) {
            throw new IllegalArgumentException("URL cannot be null for enqueuePreconnect.");
        }
        validatePreconnectUrl(url);
        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.ApiCall.ENQUEUE_PRECONNECT", mTraceArgs)) {
            mAwInit.getRunQueue().addTask(() -> preconnect(url));
        }
    }

    /**
     * Validates the URL synchronously to ensure the exception is thrown on the calling thread,
     * avoiding an uncatchable crash if validation were deferred to the background task.
     */
    private void validatePreconnectUrl(@NonNull String url) {
        GURL gurl = new GURL(url);
        if (!gurl.isValid()
                || (!gurl.getOrigin().getScheme().equals("http")
                        && !gurl.getOrigin().getScheme().equals("https"))) {
            throw new IllegalArgumentException("Invalid URL: " + gurl.getPossiblyInvalidSpec());
        }
    }

    @NonNull
    public CookieManager getCookieManager() {
        State state = getInitializedState(CallSite.PROFILE_GET_COOKIE_MANAGER);

        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.ApiCall.GET_COOKIE_MANAGER", mTraceArgs)) {
            return state.cookieManager;
        }
    }

    @NonNull
    public WebStorage getWebStorage() {
        return getInitializedState(CallSite.PROFILE_GET_WEB_STORAGE).webStorage;
    }

    @NonNull
    public GeolocationPermissions getGeolocationPermissions() {
        return getInitializedState(CallSite.PROFILE_GET_GEOLOCATION_PERMISSIONS)
                .geolocationPermissions;
    }

    @NonNull
    public ServiceWorkerController getServiceWorkerController() {
        return getInitializedState(CallSite.PROFILE_GET_SERVICE_WORKER_CONTROLLER)
                .serviceWorkerController;
    }

    @UiThread
    public int prefetchUrl(
            String url,
            @Nullable PrefetchParams params,
            Executor callbackExecutor,
            PrefetchOperationCallback resultCallback) {
        AwBrowserContext browserContext =
                getInitializedState(CallSite.PROFILE_PREFETCH_URL).browserContext;

        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.ApiCall.Prefetch.PRE_START")) {
            validatePrefetchArgs(url, resultCallback);
            return browserContext
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
        AwBrowserContext browserContext =
                getInitializedState(CallSite.PROFILE_PREFETCH_URL_ASYNC).browserContext;

        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.ApiCall.Prefetch.PRE_START_ASYNC")) {
            validatePrefetchArgs(url, resultCallback);
            browserContext
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
    public void cancelPrefetch(int prefetchKey) {
        getInitializedState(CallSite.PROFILE_CANCEL_PREFETCH)
                .browserContext
                .getPrefetchManager()
                .cancelPrefetch(prefetchKey);
    }

    /**
     * Keeping this API since there are still versions hit it. setMaxPrerenders with int parameter
     * should be used.
     */
    @UiThread
    public void setMaxPrerenders(@Nullable Integer maxPrerenders) {
        if (maxPrerenders == null) {
            clearMaxPrerenders();
        } else if (maxPrerenders >= 0) {
            getInitializedState(CallSite.PROFILE_SET_MAX_PRERENDERS)
                    .browserContext
                    .setMaxPrerenders(maxPrerenders);
        } else {
            throw new IllegalArgumentException("Maximum prerenders can not be negative.");
        }
    }

    /** Restores the default maxPrerenders */
    @UiThread
    public void clearMaxPrerenders() {
        getInitializedState(CallSite.PROFILE_CLEAR_MAX_PRERENDERS)
                .browserContext
                .clearMaxPrerenders();
    }

    /**
     * @return Max Prerenders set for the {@link Profile}
     */
    @UiThread
    public int getMaxPrerenders() {
        return getInitializedState(CallSite.PROFILE_GET_MAX_PRERENDERS)
                .browserContext
                .getAllowedPrerenderingCount();
    }

    /**
     * @param maxPrerenders The maximum number of prerenders.
     */
    @UiThread
    public void setMaxPrerenders(int maxPrerenders) {
        if (maxPrerenders < 0) {
            throw new IllegalArgumentException("Maximum prerenders can not be negative.");
        }
        getInitializedState(CallSite.PROFILE_SET_MAX_PRERENDERS)
                .browserContext
                .setMaxPrerenders(maxPrerenders);
    }

    /**
     * @param maxPrefetches The maximum number of prefetches.
     */
    @UiThread
    public void setMaxPrefetches(int maxPrefetches) {
        if (maxPrefetches < 0) {
            throw new IllegalArgumentException("Maximum prefetches can not be negative.");
        }
        getInitializedState(CallSite.PROFILE_SET_MAX_PREFETCHES)
                .browserContext
                .getPrefetchManager()
                .setMaxPrefetches(maxPrefetches);
    }

    /**
     * Keeping this API since there are still versions hit it. setMaxPrefetches with int parameter
     * should be used.
     */
    @UiThread
    public void setMaxPrefetches(@Nullable Integer maxPrefetches) {
        if (maxPrefetches == null) {
            clearMaxPrefetches();
        } else if (maxPrefetches >= 0) {
            getInitializedState(CallSite.PROFILE_SET_MAX_PREFETCHES)
                    .browserContext
                    .getPrefetchManager()
                    .setMaxPrefetches(maxPrefetches);
        } else {
            throw new IllegalArgumentException("Maximum prefetches can not be negative.");
        }
    }

    /**
     * Keeping this API since there are still versions hit it. setPrefetchTtlSeconds with int
     * parameter should be used.
     */
    @UiThread
    public void setPrefetchTtlSeconds(@Nullable Integer prefetchTtlSeconds) {
        if (prefetchTtlSeconds == null) {
            clearPrefetchTtl();
        } else if (prefetchTtlSeconds >= 0) {
            getInitializedState(CallSite.PROFILE_SET_PREFETCH_TTL_SECONDS)
                    .browserContext
                    .getPrefetchManager()
                    .setPrefetchTtlSeconds(prefetchTtlSeconds);
        } else {
            throw new IllegalArgumentException("Prefetch TTL seconds can not be negative.");
        }
    }

    /**
     * @param prefetchTTLSeconds Sets the TTL seconds for prefetch.
     */
    @UiThread
    public void setPrefetchTtlSeconds(int prefetchTtlSeconds) {
        AwBrowserContext browserContext =
                getInitializedState(CallSite.PROFILE_SET_PREFETCH_TTL_SECONDS).browserContext;

        if (prefetchTtlSeconds < 0) {
            throw new IllegalArgumentException("Prefetch TTL seconds can not be negative.");
        }
        browserContext.getPrefetchManager().setPrefetchTtlSeconds(prefetchTtlSeconds);
    }

    /** Restores the maximum number of prefetches to its default value. */
    @UiThread
    public void clearMaxPrefetches() {
        getInitializedState(CallSite.PROFILE_CLEAR_MAX_PREFETCHES)
                .browserContext
                .getPrefetchManager()
                .clearMaxPrefetches();
    }

    /** Sets the TTL seconds for prefetch to its default value. */
    @UiThread
    public void clearPrefetchTtl() {
        getInitializedState(CallSite.PROFILE_CLEAR_PREFETCH_TTL)
                .browserContext
                .getPrefetchManager()
                .clearPrefetchTtl();
    }

    /**
     * @return Max Prefetches set for the {@link Profile}
     */
    @UiThread
    public int getMaxPrefetches() {
        return getInitializedState(CallSite.PROFILE_GET_MAX_PREFETCHES)
                .browserContext
                .getPrefetchManager()
                .getMaxPrefetches();
    }

    /**
     * @return The TTL seconds for the {@link Profile}.
     */
    @UiThread
    public int getPrefetchTtlSeconds() {
        return getInitializedState(CallSite.PROFILE_GET_PREFETCH_TTL_SECONDS)
                .browserContext
                .getPrefetchManager()
                .getPrefetchTtlSeconds();
    }

    @UiThread
    public void setSpeculativeLoadingConfig(SpeculativeLoadingConfig speculativeLoadingConfig) {
        AwBrowserContext browserContext =
                getInitializedState(CallSite.PROFILE_SET_SPECULATIVE_LOADING_CONFIG).browserContext;

        browserContext
                .getPrefetchManager()
                .updatePrefetchConfiguration(
                        speculativeLoadingConfig.prefetchTTLSeconds,
                        speculativeLoadingConfig.maxPrefetches);
        if (speculativeLoadingConfig.maxPrerenders > 0) {
            browserContext.setMaxPrerenders(speculativeLoadingConfig.maxPrerenders);
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
        AwBrowserContext browserContext =
                getInitializedState(CallSite.PROFILE_WARM_UP_RENDERER_PROCESS).browserContext;

        try (TraceEvent event =
                TraceEvent.scoped("WebView.Profile.ApiCall.WARM_UP_RENDERER_PROCESS")) {
            browserContext.warmUpSpareRenderer();
        }
    }

    @UiThread
    public void setOriginMatchedHeader(
            String headerName, String headerValue, Set<String> originRules) {
        getInitializedState(CallSite.PROFILE_SET_ORIGIN_MATCHED_HEADER)
                .browserContext
                .setOriginMatchedHeader(headerName, headerValue, originRules);
    }

    @UiThread
    public void addOriginMatchedHeader(
            String headerName, String headerValue, Set<String> originRules) {
        getInitializedState(CallSite.PROFILE_ADD_ORIGIN_MATCHED_HEADER)
                .browserContext
                .addOriginMatchedHeader(headerName, headerValue, originRules);
    }

    @UiThread
    public boolean hasOriginMatchedHeader(String headerName) {
        return getInitializedState(CallSite.PROFILE_HAS_ORIGIN_MATCHED_HEADER)
                .browserContext
                .hasOriginMatchedHeader(headerName);
    }

    @UiThread
    public List<AwOriginMatchedHeader> findOriginMatchedHeaders(
            @Nullable String headerName, @Nullable String headerValue) {
        return getInitializedState(CallSite.PROFILE_FIND_ORIGIN_MATCHED_HEADERS)
                .browserContext
                .findOriginMatchedHeaders(headerName, headerValue);
    }

    @UiThread
    public void clearOriginMatchedHeader(String headerName, @Nullable String headerValue) {
        getInitializedState(CallSite.PROFILE_CLEAR_ORIGIN_MATCHED_HEADER)
                .browserContext
                .clearOriginMatchedHeader(headerName, headerValue);
    }

    @UiThread
    public void clearAllOriginMatchedHeaders() {
        getInitializedState(CallSite.PROFILE_CLEAR_ALL_ORIGIN_MATCHED_HEADERS)
                .browserContext
                .clearAllOriginMatchedHeaders();
    }

    @UiThread
    public void addQuicHints(Set<String> origins) {
        mAwInit.getRunQueue()
                .addTask(
                        () -> {
                            AwBrowserContext browserContext =
                                    getInitializedState(CallSite.PROFILE_ADD_QUIC_HINTS)
                                            .browserContext;

                            if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_ADD_QUIC_HINTS)) {
                                browserContext.addQuicHints(origins);
                            } else {
                                Log.w(TAG, "Profile.addQuicHints has been disabled.");
                            }
                        });
    }

    /**
     * @return The HTTP cache manager for the {@link Profile}
     */
    @UiThread
    public AwHttpCacheManager getHttpCacheManager() {
        return getInitializedState(CallSite.PROFILE_GET_HTTP_CACHE_MANAGER)
                .browserContext
                .getHttpCacheManager();
    }

    @UiThread
    public void setCrossOriginIsolatedAllowList(@NonNull Set<String> originPatterns) {
        getInitializedState(CallSite.PROFILE_SET_CROSS_ORIGIN_ISOLATED_ALLOW_LIST)
                .browserContext
                .setCrossOriginIsolatedAllowList(originPatterns);
    }

    @UiThread
    public @NonNull Set<String> getCrossOriginIsolatedAllowList() {
        return getInitializedState(CallSite.PROFILE_GET_CROSS_ORIGIN_ISOLATED_ALLOW_LIST)
                .browserContext
                .getCrossOriginIsolatedAllowList();
    }
}
