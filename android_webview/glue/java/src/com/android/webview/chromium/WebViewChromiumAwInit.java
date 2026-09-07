// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.os.Looper;
import android.webkit.CookieManager;
import android.webkit.WebIconDatabase;
import android.webkit.WebViewDatabase;

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;

import com.android.webview.chromium.ApiCallLogger.ApiCall;
import com.android.webview.chromium.ApiCallLogger.ApiCallUserAction;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwTracingController;
import org.chromium.android_webview.DualTraceEvent;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.android_webview.StartupCallSite;
import org.chromium.android_webview.StartupController;
import org.chromium.android_webview.StartupDiagnostics;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.base.ThreadUtils;
import org.chromium.build.BuildConfig;

import java.util.Set;
import java.util.concurrent.CountDownLatch;

/**
 * Class controlling the Chromium initialization for WebView. We hold on to most static objects used
 * by WebView here. This class is shared between the webkit glue layer and the support library glue
 * layer.
 */
@Lifetime.Singleton
public class WebViewChromiumAwInit {
    private static final String TAG = "WebViewChromiumAwInit";

    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    @GuardedBy("mLazyInitLock")
    private CookieManagerAdapter mDefaultCookieManager;

    @GuardedBy("mLazyInitLock")
    private WebIconDatabaseAdapter mWebIconDatabase;

    @GuardedBy("mLazyInitLock")
    private WebViewDatabaseAdapter mDefaultWebViewDatabase;

    private final ProfileStore mProfileStore = new ProfileStore(this);

    private final DefaultProfileHolder mDefaultProfileHolder = new DefaultProfileHolder();

    // Guards access to fields that are initialized on first use rather than by startChromium.
    // This lock is used across WebViewChromium startup classes ie WebViewChromiumAwInit,
    // SupportLibWebViewChromiumFactory and WebViewChromiumFactoryProvider so as to avoid deadlock.
    // TODO(crbug.com/397385172): Get rid of this lock.
    private final Object mLazyInitLock = new Object();

    private final WebViewChromiumFactoryProvider mFactory;

    private volatile boolean mShouldInitializeDefaultProfile = true;

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
    }

    void initializeDefaultProfileOnUI() {
        if (mShouldInitializeDefaultProfile) {
            try (DualTraceEvent e =
                    DualTraceEvent.scoped("WebViewChromiumAwInit.initializeDefaultProfile")) {
                mDefaultProfileHolder.initializeDefaultProfileOnUI();
            }
        }
    }

    boolean isChromiumInitialized() {
        return getStartupController().isChromiumInitialized();
    }

    /**
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup is finished when this method returns.
     */
    void triggerAndWaitForChromiumStarted(@StartupCallSite int callSite) {
        if (isChromiumInitialized()) {
            return;
        }
        // For threadSafe WebView APIs that can trigger startup, holding a lock while waiting for
        // the startup to complete can lead to a deadlock. This would happen when:
        // - A background thread B call threadsafe funcA and acquires mLazyInitLock.
        // - Thread B posts the startup task to the UI thread and waits for completion.
        // - UI thread calls funcA before it has executed the posted startup task.
        // - UI thread blocks trying to acquire mLazyInitLock that's held by thread B.
        // - Deadlock!
        // See crbug.com/395877483 for more details.
        assert !Thread.holdsLock(mLazyInitLock);
        getStartupController().triggerAndWaitForChromiumStarted(callSite);
    }

    /**
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup will be finished in the near future.
     */
    void postChromiumStartupIfNeeded(@StartupCallSite int callSite) {
        getStartupController().postChromiumStartupIfNeeded(callSite);
    }

    void maybeSetChromiumUiThread(Looper looper) {
        getStartupController().maybeSetChromiumUiThread(looper);
    }

    public SharedStatics getSharedStatics() {
        return mFactory.getSharedStatics();
    }

    boolean isMultiProcessEnabled() {
        return mFactory.isMultiProcessEnabled();
    }

    public AwTracingController getAwTracingController() {
        triggerAndWaitForChromiumStarted(StartupCallSite.GET_AW_TRACING_CONTROLLER);
        return AwTracingController.getInstance();
    }

    public ProfileStore getProfileStore() {
        if (WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_MULTI_PROFILE_SKIP_DEFAULT_PROFILE)) {
            mShouldInitializeDefaultProfile = false;
        }
        if (ProfileStore.requiresStartup()) {
            triggerAndWaitForChromiumStarted(StartupCallSite.GET_PROFILE_STORE);
        }
        return mProfileStore;
    }

    public CookieManager getDefaultCookieManager() {
        synchronized (mLazyInitLock) {
            if (mDefaultCookieManager == null) {
                mDefaultCookieManager =
                        new CookieManagerAdapter(AwCookieManager.getDefaultCookieManager());
            }
            return mDefaultCookieManager;
        }
    }

    public WebIconDatabase getWebIconDatabase() {
        triggerAndWaitForChromiumStarted(StartupCallSite.GET_WEB_ICON_DATABASE);
        ApiCallLogger.recordWebViewApiCall(
                ApiCall.WEB_ICON_DATABASE_GET_INSTANCE,
                ApiCallUserAction.WEB_ICON_DATABASE_GET_INSTANCE);
        synchronized (mLazyInitLock) {
            if (mWebIconDatabase == null) {
                mWebIconDatabase = new WebIconDatabaseAdapter();
            }
            return mWebIconDatabase;
        }
    }

    public WebViewDatabase getDefaultWebViewDatabase(final Context context) {
        triggerAndWaitForChromiumStarted(StartupCallSite.GET_DEFAULT_WEBVIEW_DATABASE);
        synchronized (mLazyInitLock) {
            if (mDefaultWebViewDatabase == null) {
                mDefaultWebViewDatabase =
                        new WebViewDatabaseAdapter(
                                mFactory,
                                HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE));
            }
            return mDefaultWebViewDatabase;
        }
    }

    public WebViewChromiumRunQueue getRunQueue() {
        return mFactory.getRunQueue();
    }

    public Object getLazyInitLock() {
        return mLazyInitLock;
    }

    // Starts up WebView asynchronously.
    // MUST NOT be called on the UI thread.
    // The callback can either be called synchronously or on the UI thread.
    public void startUpWebView(
            StartupDiagnostics.Callback callback,
            boolean shouldRunUiThreadStartUpTasks,
            @Nullable Set<String> profilesToLoad) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            throw new IllegalStateException(
                    "startUpWebView should not be called on the Android main looper");
        }

        if (profilesToLoad != null) {
            if (!shouldRunUiThreadStartUpTasks) {
                throw new IllegalArgumentException(
                        "Can't specify profiles to load without running UI thread startup tasks");
            }
            mShouldInitializeDefaultProfile = false;
        }

        if (!shouldRunUiThreadStartUpTasks) {
            callback.onSuccess(getStartupController().getStartupDiagnostics());
            return;
        }

        getStartupController()
                .requestAsyncStartup(
                        diagnostics -> {
                            Set<String> profilesCopy =
                                    profilesToLoad != null
                                            ? profilesToLoad
                                            : Set.of(AwBrowserContext.getDefaultContextName());

                            for (String context : profilesCopy) {
                                mProfileStore.getOrCreateProfile(
                                        context, ProfileStore.CallSite.ASYNC_WEBVIEW_STARTUP);
                            }
                            callback.onSuccess(diagnostics);
                        });
    }

    public Profile getDefaultProfile(@StartupCallSite int callSite) {
        return mDefaultProfileHolder.getDefaultProfile(callSite);
    }

    public StartupController getStartupController() {
        return StartupController.getInstance();
    }

    private final class DefaultProfileHolder {
        private volatile Profile mDefaultProfile;
        private final CountDownLatch mDefaultProfileIsInitialized = new CountDownLatch(1);

        /** Must be called on the UI thread. */
        public void initializeDefaultProfileOnUI() {
            if (BuildConfig.ENABLE_ASSERTS && !ThreadUtils.runningOnUiThread()) {
                throw new RuntimeException(
                        "DefaultProfileHolder called on " + Thread.currentThread());
            }
            if (mDefaultProfile != null) return;
            mDefaultProfile =
                    mProfileStore.getOrCreateProfile(
                            AwBrowserContext.getDefaultContextName(),
                            ProfileStore.CallSite.GET_DEFAULT_PROFILE);
            mDefaultProfileIsInitialized.countDown();
        }

        /**
         * Ensures the default profile and its dependencies are initialized on the UI thread.
         *
         * <p>The {@code StartupWebView} API allows for initializing a specific list of profiles,
         * which may not include the default profile. This method acts as a safeguard, ensuring the
         * default profile is ready the first time a thread-safe framework API is called.
         */
        private void ensureInitializationIsDone(@StartupCallSite int callSite) {
            triggerAndWaitForChromiumStarted(callSite);
            if (mDefaultProfile != null) {
                return;
            }

            ThreadUtils.runOnUiThread(this::initializeDefaultProfileOnUI);
            // Wait for the UI to finish.
            while (true) {
                try {
                    mDefaultProfileIsInitialized.await();
                    break;
                } catch (InterruptedException e) {
                    // Keep trying; we can't abort here as WebView APIs do not declare that they
                    // throw InterruptedException.
                }
            }
        }

        public Profile getDefaultProfile(@StartupCallSite int callSite) {
            ensureInitializationIsDone(callSite);
            return mDefaultProfile;
        }
    }
}
