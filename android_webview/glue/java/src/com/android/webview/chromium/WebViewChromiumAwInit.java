// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.os.Looper;
import android.webkit.CookieManager;
import android.webkit.SelectionActionMenuClient;
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
import org.chromium.android_webview.R;
import org.chromium.android_webview.StartupCallSite;
import org.chromium.android_webview.StartupController;
import org.chromium.android_webview.StartupDiagnostics;
import org.chromium.android_webview.StartupMetrics;
import org.chromium.android_webview.StartupTasksRunner;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwResource;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/**
 * Class controlling the Chromium initialization for WebView. We hold on to most static objects used
 * by WebView here. This class is shared between the webkit glue layer and the support library glue
 * layer.
 */
@Lifetime.Singleton
public class WebViewChromiumAwInit {
    private static final String TAG = "WebViewChromiumAwInit";

    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    private static final String ASSET_PATH_WORKAROUND_HISTOGRAM_NAME =
            "Android.WebView.AssetPathWorkaroundUsed.StartChromiumLocked";

    @GuardedBy("mLazyInitLock")
    private CookieManagerAdapter mDefaultCookieManager;

    @GuardedBy("mLazyInitLock")
    private WebIconDatabaseAdapter mWebIconDatabase;

    @GuardedBy("mLazyInitLock")
    private WebViewDatabaseAdapter mDefaultWebViewDatabase;

    private final ProfileStore mProfileStore = new ProfileStore(this);

    private final DefaultProfileHolder mDefaultProfileHolder = new DefaultProfileHolder();

    // This is only accessed during WebViewChromiumFactoryProvider.initialize() which is guarded by
    // the WebViewFactory lock in the framework, and on the UI thread during startChromium
    // which cannot be called before initialize() has completed.
    private FutureTask<Void> mSetUpResourcesTask;

    // Guards access to fields that are initialized on first use rather than by startChromium.
    // This lock is used across WebViewChromium startup classes ie WebViewChromiumAwInit,
    // SupportLibWebViewChromiumFactory and WebViewChromiumFactoryProvider so as to avoid deadlock.
    // TODO(crbug.com/397385172): Get rid of this lock.
    private final Object mLazyInitLock = new Object();

    private final WebViewChromiumFactoryProvider mFactory;
    private final WebViewChromiumRunQueue mWebViewStartUpCallbackRunQueue =
            new WebViewChromiumRunQueue();

    private final StartupController.Delegate mStartupDelegate =
            new StartupController.Delegate() {
                @Override
                public void waitForJavaResourcesSetup() {
                    WebViewChromiumAwInit.this.waitForJavaResourcesSetup();
                }

                @Override
                public boolean shouldForceNativeSandboxedServices() {
                    AconfigFlaggedApiDelegate aconfigDelegate =
                            AconfigFlaggedApiDelegate.getInstance();
                    return aconfigDelegate != null
                            && aconfigDelegate.isNativeWebViewZygoteEnabled(
                                    mFactory.getWebViewDelegate());
                }

                @Override
                public long getDrawFnFunctionTable() {
                    return DrawFunctor.getDrawFnFunctionTable();
                }

                @Override
                public long getDrawSWFunctionTable() {
                    return GraphicsUtils.getDrawSWFunctionTable();
                }

                @Override
                public @Nullable SelectionActionMenuClientWrapper getSelectionActionMenuClient() {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN) {
                        SelectionActionMenuClient client =
                                mFactory.getWebViewDelegate()
                                        .getSelectionActionMenuClient(
                                                ContextUtils.getApplicationContext());
                        if (client != null) {
                            return new SelectionActionMenuClientAdapter(client);
                        }
                    }
                    return null;
                }

                @Override
                public void onStartupComplete() {
                    if (mShouldInitializeDefaultProfile) {
                        try (DualTraceEvent e =
                                DualTraceEvent.scoped(
                                        "WebViewChromiumAwInit.initializeDefaultProfile")) {
                            mDefaultProfileHolder.initializeDefaultProfileOnUI();
                        }
                    }
                    mFactory.getRunQueue().notifyChromiumStarted();
                }

                @Override
                public void onStartupDiagnosticsReady(StartupDiagnostics diagnostics) {
                    recordStartupMetrics(diagnostics.getStartupTimings());
                }
            };

    private final StartupController mStartupController = new StartupController(mStartupDelegate);

    private volatile boolean mShouldInitializeDefaultProfile = true;

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
    }

    void setProviderInitOnMainLooperLocation(Throwable t) {
        mStartupController.setProviderInitOnMainLooperLocation(t);
    }

    private void recordStartupMetrics(StartupTasksRunner.StartupTimings timings) {
        mWebViewStartUpCallbackRunQueue.notifyChromiumStarted();

        // Stop early trace event collection.
        // They have already been emitted if a trace session was started to capture startup.
        EarlyTraceEvent.reset();

        // Record histograms
        StartupMetrics.recordChromiumInitTimes(timings);
        // Also create the trace events for the earlier WebViewChromiumFactoryProvider init, which
        // happens before tracing is ready.
        mFactory.recordInitTraces();
    }

    /**
     * Set up resources on a background thread, in parallel with chromium initialization as it takes
     * some time. This method is called once during WebViewChromiumFactoryProvider initialization
     * which is guaranteed to finish before this field is accessed by waitForJavaResourcesSetup.
     *
     * @param context The context.
     */
    void setUpResourcesOnBackgroundThread(int packageId, Context context) {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.setUpResourcesOnBackgroundThread")) {
            assert mSetUpResourcesTask == null : "This method shouldn't be called twice.";

            Runnable setUpResourcesRunnable =
                    new Runnable() {
                        @Override
                        public void run() {
                            try (DualTraceEvent e =
                                    DualTraceEvent.scoped("WebViewChromiumAwInit.setUpResources")) {
                                R.onResourcesLoaded(packageId);

                                AwResource.setResources(context.getResources());
                                AwResource.setConfigKeySystemUuidMapping(
                                        android.R.array.config_keySystemUuidMapping);
                            }
                        }
                    };

            // Make sure that ResourceProvider is initialized before starting the browser process.
            mSetUpResourcesTask = new FutureTask<>(setUpResourcesRunnable, null);
            PostTask.postTask(TaskTraits.USER_VISIBLE, mSetUpResourcesTask);
        }
    }

    private void waitForJavaResourcesSetup() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.waitForJavaResourcesSetup")) {
            mSetUpResourcesTask.get();
        } catch (InterruptedException | ExecutionException e) {
            throw new RuntimeException(e);
        }
       // TODO(crbug.com/400413041) : Remove this workaround.
        // Try to work around the resources problem.
        //
        // WebViewFactory adds WebView's asset path to the host app before any
        // of the code in the APK starts running, but it adds it using an old
        // mechanism that doesn't persist if the app's resource configuration
        // changes for any other reason.
        //
        // By the time we get here, it's possible it's gone missing due to
        // something on the UI thread having triggered a resource update. This
        // can happen either because WebView initialization was triggered by a
        // background thread (and thus this code is running inside a posted task
        // on the UI thread which may have taken any amount of time to actually
        // run), or because the app used CookieManager first, which triggers the
        // code being loaded and WebViewFactory doing the initial resources add,
        // but does not call startChromium until the app uses some other
        // API, an arbitrary amount of time later. So, we can try to add them
        // again using the "better" method in WebViewDelegate.
        //
        // However, we only want to try this if the resources are actually
        // missing, because in the past we've seen this cause apps that were
        // working to *start* crashing. The first resource that gets accessed in
        // startup happens during the AwBrowserProcess.start() call when trying
        // to determine if the device is a tablet, and that's the most common
        // place for us to crash. So, try calling that same method and see if it
        // throws - if so then we're unlikely to make the situation any worse by
        // trying to fix the path.
        //
        // This cannot fix the problem in all cases - if the app is using a
        // weird ContextWrapper or doing other unusual things with
        // resources/assets then even adding it with this mechanism might not
        // help.
        try {
            DeviceFormFactor.isTablet();
            RecordHistogram.recordBooleanHistogram(ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, false);
        } catch (Resources.NotFoundException e) {
            RecordHistogram.recordBooleanHistogram(ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, true);
            mFactory.addWebViewAssetPath(ContextUtils.getApplicationContext());
        }
    }

    boolean isChromiumInitialized() {
        return mStartupController.isChromiumInitialized();
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
        mStartupController.triggerAndWaitForChromiumStarted(callSite);
    }

    /**
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup will be finished in the near future.
     */
    void postChromiumStartupIfNeeded(@StartupCallSite int callSite) {
        mStartupController.postChromiumStartupIfNeeded(callSite);
    }

    void maybeSetChromiumUiThread(Looper looper) {
        mStartupController.maybeSetChromiumUiThread(looper);
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
            callback.onSuccess(mStartupController.getStartupDiagnostics());
            return;
        }

        mWebViewStartUpCallbackRunQueue.addTask(
                () -> {
                    Set<String> profilesCopy =
                            profilesToLoad != null
                                    ? profilesToLoad
                                    : Set.of(AwBrowserContext.getDefaultContextName());

                    for (String context : profilesCopy) {
                        mProfileStore.getOrCreateProfile(
                                context, ProfileStore.CallSite.ASYNC_WEBVIEW_STARTUP);
                    }
                    callback.onSuccess(mStartupController.getStartupDiagnostics());
                });
        postChromiumStartupIfNeeded(StartupCallSite.ASYNC_WEBVIEW_STARTUP);
    }

    public Profile getDefaultProfile(@StartupCallSite int callSite) {
        return mDefaultProfileHolder.getDefaultProfile(callSite);
    }

    public StartupController getStartupController() {
        return mStartupController;
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
