// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.Manifest;
import android.app.compat.CompatChanges;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Build;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebViewDatabase;

import androidx.annotation.IntDef;

import com.android.webview.chromium.WebViewChromium.ApiCall;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwClassPreloader;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwCrashyClassUtils;
import org.chromium.android_webview.AwDarkMode;
import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.AwLocaleConfig;
import org.chromium.android_webview.AwNetworkChangeNotifierRegistrationPolicy;
import org.chromium.android_webview.AwProxyController;
import org.chromium.android_webview.AwServiceWorkerController;
import org.chromium.android_webview.AwThreadUtils;
import org.chromium.android_webview.AwTracingController;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.android_webview.R;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwResource;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.android_webview.variations.VariationsSeedLoader;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.JNIUtils;
import org.chromium.base.PathService;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ResourceBundle;

/**
 * Class controlling the Chromium initialization for WebView.
 * We hold on to most static objects used by WebView here.
 * This class is shared between the webkit glue layer and the support library glue layer.
 */
@Lifetime.Singleton
public class WebViewChromiumAwInit {
    private static final String TAG = "WebViewChromiumAwInit";

    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    // TODO(gsennton): store aw-objects instead of adapters here
    // Initialization guarded by mLock.
    private AwBrowserContext mDefaultBrowserContext;
    private AwTracingController mTracingController;
    private SharedStatics mSharedStatics;
    private GeolocationPermissionsAdapter mDefaultGeolocationPermissions;
    private CookieManagerAdapter mDefaultCookieManager;

    private WebIconDatabaseAdapter mWebIconDatabase;
    private WebStorageAdapter mDefaultWebStorage;
    private WebViewDatabaseAdapter mDefaultWebViewDatabase;
    private AwServiceWorkerController mDefaultServiceWorkerController;
    private AwTracingController mAwTracingController;
    private VariationsSeedLoader mSeedLoader;
    private Thread mSetUpResourcesThread;
    private AwProxyController mAwProxyController;

    // Guards accees to the other members, and is notifyAll() signalled on the UI thread
    // when the chromium process has been started.
    // This member is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    /* package */ final Object mLock = new Object();

    // mInitState should only transition INIT_NOT_STARTED -> INIT_STARTED -> INIT_FINISHED
    private static final int INIT_NOT_STARTED = 0;
    private static final int INIT_STARTED = 1;
    private static final int INIT_FINISHED = 2;
    // Read/write protected by mLock
    private int mInitState;

    private final WebViewChromiumFactoryProvider mFactory;

    // This enum must be kept in sync with WebViewStartup.CallSite in chrome_track_event.proto and
    // WebViewStartupCallSite in enums.xml.
    @IntDef({
        CallSite.GET_AW_TRACING_CONTROLLER,
        CallSite.GET_AW_PROXY_CONTROLLER,
        CallSite.WEBVIEW_INSTANCE,
        CallSite.GET_STATICS,
        CallSite.GET_DEFAULT_GEOLOCATION_PERMISSIONS,
        CallSite.GET_DEFAULT_SERVICE_WORKER_CONTROLLER,
        CallSite.GET_WEB_ICON_DATABASE,
        CallSite.GET_DEFAULT_WEB_STORAGE,
        CallSite.GET_DEFAULT_WEBVIEW_DATABASE,
        CallSite.GET_TRACING_CONTROLLER,
        CallSite.COUNT,
    })
    public @interface CallSite {
        int GET_AW_TRACING_CONTROLLER = 0;
        int GET_AW_PROXY_CONTROLLER = 1;
        int WEBVIEW_INSTANCE = 2;
        int GET_STATICS = 3;
        int GET_DEFAULT_GEOLOCATION_PERMISSIONS = 4;
        int GET_DEFAULT_SERVICE_WORKER_CONTROLLER = 5;
        int GET_WEB_ICON_DATABASE = 6;
        int GET_DEFAULT_WEB_STORAGE = 7;
        int GET_DEFAULT_WEBVIEW_DATABASE = 8;
        int GET_TRACING_CONTROLLER = 9;
        // Remember to update WebViewStartupCallSite in enums.xml when adding new values here.
        int COUNT = 10;
    };

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
        TraceEvent.maybeEnableEarlyTracing(/* readCommandLine= */ false);
    }

    public AwTracingController getAwTracingController() {
        synchronized (mLock) {
            if (mAwTracingController == null) {
                ensureChromiumStartedLocked(true, CallSite.GET_AW_TRACING_CONTROLLER);
            }
        }
        return mAwTracingController;
    }

    public AwProxyController getAwProxyController() {
        synchronized (mLock) {
            if (mAwProxyController == null) {
                ensureChromiumStartedLocked(true, CallSite.GET_AW_PROXY_CONTROLLER);
            }
        }
        return mAwProxyController;
    }

    // TODO: DIR_RESOURCE_PAKS_ANDROID needs to live somewhere sensible,
    // inlined here for simplicity setting up the HTMLViewer demo. Unfortunately
    // it can't go into base.PathService, as the native constant it refers to
    // lives in the ui/ layer. See ui/base/ui_base_paths.h
    private static final int DIR_RESOURCE_PAKS_ANDROID = 3003;

    protected void startChromiumLocked(@CallSite int callSite, boolean triggeredFromUIThread) {
        long startTime = SystemClock.uptimeMillis();
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.startChromiumLocked")) {
            assert Thread.holdsLock(mLock) && ThreadUtils.runningOnUiThread();

            // The post-condition of this method is everything is ready, so notify now to cover all
            // return paths. (Other threads will not wake-up until we release |mLock|, whatever).
            mLock.notifyAll();

            if (mInitState == INIT_FINISHED) {
                return;
            }

            final Context context = ContextUtils.getApplicationContext();

            JNIUtils.setClassLoader(WebViewChromiumAwInit.class.getClassLoader());

            ResourceBundle.setAvailablePakLocales(AwLocaleConfig.getWebViewSupportedPakLocales());

            // We are rewriting Java resources in the background.
            // NOTE: Any reference to Java resources will cause a crash.

            try (ScopedSysTraceEvent e =
                    ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.LibraryLoader")) {
                LibraryLoader.getInstance().ensureInitialized();
            }

            PathService.override(PathService.DIR_MODULE, "/system/lib/");
            PathService.override(DIR_RESOURCE_PAKS_ANDROID, "/system/framework/webview/paks");

            initPlatSupportLibrary();
            AwContentsStatics.setCheckClearTextPermitted(
                    context.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.O);

            waitUntilSetUpResources();

            // NOTE: Finished writing Java resources. From this point on, it's safe to use them.

            // Try to work around the resources problem.
            //
            // WebViewFactory adds WebView's asset path to the host app before any of the code in
            // the APK starts running, but it adds it using an old mechanism that doesn't persist if
            // the app's resource configuration changes for any other reason.
            //
            // By the time we get here, it's possible it's gone missing due to something on the UI
            // thread having triggered a resource update. This can happen either because WebView
            // initialization was triggered by a background thread (and thus this code is running
            // inside a posted task on the UI thread which may have taken any amount of time to
            // actually run), or because the app used CookieManager first, which triggers the code
            // being loaded and WebViewFactory doing the initial resources add, but does not call
            // startChromiumLocked until the app uses some other API, an arbitrary amount of time
            // later. So, we can try to add them again using the "better" method in WebViewDelegate.
            //
            // However, we only want to try this if the resources are actually missing, because
            // in the past we've seen this cause apps that were working to *start* crashing.
            // The first resource that gets accessed in startup happens during the
            // AwBrowserProcess.start() call when trying to determine if the device is a tablet,
            // and that's the most common place for us to crash. So, try calling that same
            // method and see if it throws - if so then we're unlikely to make the situation
            // any worse by trying to fix the path.
            //
            // This cannot fix the problem in all cases - if the app is using a weird ContextWrapper
            // or doing other unusual things with resources/assets then even adding it with this
            // mechanism might not help.
            try {
                DeviceFormFactor.isTablet();
            } catch (Resources.NotFoundException e) {
                mFactory.addWebViewAssetPath(context);
            }

            AwBrowserProcess.configureChildProcessLauncher();

            // finishVariationsInitLocked() must precede native initialization so the seed is
            // available when AwFeatureListCreator::SetUpFieldTrials() runs.
            if (!FastVariationsSeedSafeModeAction.hasRun()) {
                finishVariationsInitLocked();
            }

            AwBrowserProcess.start();

            // TODO(crbug.com/332706093): See if this can be moved before loading native.
            AwClassPreloader.preloadClasses();

            AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ true);
            doNetworkInitializations(context);

            // This has to be done after variations are initialized, so components could be
            // registered or not depending on the variations flags.
            AwBrowserProcess.loadComponents();
            AwBrowserProcess.initializeMetricsLogUploader();

            mSharedStatics = new SharedStatics();
            if (BuildInfo.isDebugAndroidOrApp()) {
                mSharedStatics.setWebContentsDebuggingEnabledUnconditionally(true);
            }

            mInitState = INIT_FINISHED;

            RecordHistogram.recordSparseHistogram(
                    "Android.WebView.TargetSdkVersion",
                    context.getApplicationInfo().targetSdkVersion);

            try (ScopedSysTraceEvent e =
                    ScopedSysTraceEvent.scoped(
                            "WebViewChromiumAwInit.initThreadUnsafeSingletons")) {
                // Initialize thread-unsafe singletons.
                AwBrowserContext defaultBrowserContext = getDefaultBrowserContextOnUiThread();
                mDefaultGeolocationPermissions =
                        new GeolocationPermissionsAdapter(
                                mFactory, defaultBrowserContext.getGeolocationPermissions());
                mDefaultWebStorage =
                        new WebStorageAdapter(
                                mFactory, defaultBrowserContext.getQuotaManagerBridge());
                mAwTracingController = getTracingController();
                mDefaultServiceWorkerController =
                        defaultBrowserContext.getServiceWorkerController();
                mAwProxyController = new AwProxyController();
            }

            if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                    ? CompatChanges.isChangeEnabled(WebSettings.ENABLE_SIMPLIFIED_DARK_MODE)
                    : BuildInfo.targetsAtLeastT()) {
                AwDarkMode.enableSimplifiedDarkMode();
            }

            if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_VERBOSE_LOGGING)) {
                logCommandLineAndActiveTrials();
            }

            PostTask.postTask(
                    TaskTraits.BEST_EFFORT,
                    () ->
                            mFactory.setWebViewContextExperimentValue(
                                    AwFeatureMap.isEnabled(
                                            AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT)));

            // This runs all the pending tasks queued for after Chromium init is finished,
            // so should be the last thing that happens in startChromiumLocked.
            mFactory.getRunQueue().drainQueue();

            AwCrashyClassUtils.maybeCrashIfEnabled();
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Startup.CreationTime.InitReason", callSite, CallSite.COUNT);

        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.CreationTime.StartChromiumLocked",
                SystemClock.uptimeMillis() - startTime);
        TraceEvent.webViewStartupStartChromiumLocked(
                startTime,
                SystemClock.uptimeMillis() - startTime,
                /* callSite= */ callSite,
                /* fromUIThread= */ triggeredFromUIThread);
    }

    /**
     * Set up resources on a background thread.
     *
     * @param context The context.
     */
    public void setUpResourcesOnBackgroundThread(int packageId, Context context) {
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped(
                        "WebViewChromiumAwInit.setUpResourcesOnBackgroundThread")) {
            assert mSetUpResourcesThread == null : "This method shouldn't be called twice.";

            // Make sure that ResourceProvider is initialized before starting the browser process.
            mSetUpResourcesThread =
                    new Thread(
                            new Runnable() {
                                @Override
                                public void run() {
                                    // Run this in parallel as it takes some time.
                                    setUpResources(packageId, context);
                                }
                            });
            mSetUpResourcesThread.start();
        }
    }

    private void waitUntilSetUpResources() {
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.waitUntilSetUpResources")) {
            mSetUpResourcesThread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    private void setUpResources(int packageId, Context context) {
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.setUpResources")) {
            R.onResourcesLoaded(packageId);

            AwResource.setResources(context.getResources());
            AwResource.setConfigKeySystemUuidMapping(android.R.array.config_keySystemUuidMapping);
        }
    }

    boolean hasStarted() {
        return mInitState == INIT_FINISHED;
    }

    void startYourEngines(boolean fromThreadSafeFunction) {
        synchronized (mLock) {
            ensureChromiumStartedLocked(fromThreadSafeFunction, CallSite.WEBVIEW_INSTANCE);
        }
    }

    // This method is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    /* package */ void ensureChromiumStartedLocked(
            boolean fromThreadSafeFunction, @CallSite int callSite) {
        assert Thread.holdsLock(mLock);

        if (mInitState == INIT_FINISHED) { // Early-out for the common case.
            return;
        }

        if (mInitState == INIT_NOT_STARTED) {
            // If we're the first thread to enter ensureChromiumStartedLocked, we need to determine
            // which thread will be the UI thread; declare init has started so that no other thread
            // will try to do this.
            mInitState = INIT_STARTED;
            setChromiumUiThreadLocked(fromThreadSafeFunction);
        }

        if (ThreadUtils.runningOnUiThread()) {
            // If we are currently running on the UI thread then we must do init now. If there was
            // already a task posted to the UI thread from another thread to do it, it will just
            // no-op when it runs.
            startChromiumLocked(callSite, /* triggeredFromUIThread= */ true);
            return;
        }

        // If we're not running on the UI thread (because init was triggered by a thread-safe
        // function), post init to the UI thread, since init is *not* thread-safe.
        AwThreadUtils.postToUiThreadLooper(
                new Runnable() {
                    @Override
                    public void run() {
                        synchronized (mLock) {
                            startChromiumLocked(callSite, /* triggeredFromUIThread= */ false);
                        }
                    }
                });

        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.waitForUIThreadInit")) {
            long startTime = SystemClock.uptimeMillis();
            // Wait for the UI thread to finish init.
            while (mInitState != INIT_FINISHED) {
                try {
                    mLock.wait();
                } catch (InterruptedException e) {
                    // Keep trying; we can't abort init as WebView APIs do not declare that they
                    // throw InterruptedException.
                }
            }
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.waitForUIThreadInit",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    private void setChromiumUiThreadLocked(boolean fromThreadSafeFunction) {
        // If we're being started from a function that's allowed to be called on any thread,
        // then we can't just assume the current thread is the UI thread; instead we assume the
        // process's main looper will be the UI thread, because that's the case for almost all
        // Android apps.
        //
        // If we're being started from a function that must be called from the UI
        // thread, then by definition the current thread is the UI thread whether it's the main
        // looper or not.
        Looper looper = fromThreadSafeFunction ? Looper.getMainLooper() : Looper.myLooper();
        Log.v(
                TAG,
                "Binding Chromium to "
                        + (Looper.getMainLooper().equals(looper) ? "main" : "background")
                        + " looper "
                        + looper);
        ThreadUtils.setUiThread(looper);
    }

    private void initPlatSupportLibrary() {
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.initPlatSupportLibrary")) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                AwDrawFnImpl.setDrawFnFunctionTable(DrawFunctor.getDrawFnFunctionTable());
            }
            DrawGLFunctor.setChromiumAwDrawGLFunction(AwContents.getAwDrawGLFunction());
            AwContents.setAwDrawSWFunctionTable(GraphicsUtils.getDrawSWFunctionTable());
            AwContents.setAwDrawGLFunctionTable(GraphicsUtils.getDrawGLFunctionTable());
        }
    }

    private void doNetworkInitializations(Context applicationContext) {
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.doNetworkInitializations")) {
            boolean forceUpdateNetworkState =
                    !AwFeatureMap.isEnabled(
                            AwFeatures.WEBVIEW_USE_INITIAL_NETWORK_STATE_AT_STARTUP);
            if (applicationContext.checkPermission(
                            Manifest.permission.ACCESS_NETWORK_STATE,
                            Process.myPid(),
                            Process.myUid())
                    == PackageManager.PERMISSION_GRANTED) {
                NetworkChangeNotifier.init();
                NetworkChangeNotifier.setAutoDetectConnectivityState(
                        new AwNetworkChangeNotifierRegistrationPolicy(), forceUpdateNetworkState);
            }
        }
    }

    public AwTracingController getTracingController() {
        if (mTracingController == null) {
            mTracingController = new AwTracingController();
        }
        return mTracingController;
    }

    // Only on UI thread.
    AwBrowserContext getDefaultBrowserContextOnUiThread() {
        assert mInitState == INIT_FINISHED;

        if (BuildConfig.ENABLE_ASSERTS && !ThreadUtils.runningOnUiThread()) {
            throw new RuntimeException(
                    "getBrowserContextOnUiThread called on " + Thread.currentThread());
        }

        if (mDefaultBrowserContext == null) {
            mDefaultBrowserContext = AwBrowserContext.getDefault();
        }
        return mDefaultBrowserContext;
    }

    /**
     * Returns the lock used for guarding chromium initialization.
     * We make this public to let higher-level classes use this lock to guard variables
     * dependent on this class, to avoid introducing new locks (which can cause deadlocks).
     */
    public Object getLock() {
        return mLock;
    }

    public SharedStatics getStatics() {
        synchronized (mLock) {
            if (mSharedStatics == null) {
                // TODO: Optimization potential: most of the static methods only need the native
                // library loaded and initialized, not the entire browser process started.
                ensureChromiumStartedLocked(true, CallSite.GET_STATICS);
                SharedStatics.setStartupTriggered();
            }
        }
        return mSharedStatics;
    }

    public GeolocationPermissions getDefaultGeolocationPermissions() {
        synchronized (mLock) {
            if (mDefaultGeolocationPermissions == null) {
                ensureChromiumStartedLocked(true, CallSite.GET_DEFAULT_GEOLOCATION_PERMISSIONS);
            }
        }
        return mDefaultGeolocationPermissions;
    }

    public CookieManager getDefaultCookieManager() {
        synchronized (mLock) {
            if (mDefaultCookieManager == null) {
                mDefaultCookieManager =
                        new CookieManagerAdapter(AwCookieManager.getDefaultCookieManager());
            }
        }
        return mDefaultCookieManager;
    }

    public AwServiceWorkerController getDefaultServiceWorkerController() {
        synchronized (mLock) {
            if (mDefaultServiceWorkerController == null) {
                ensureChromiumStartedLocked(true, CallSite.GET_DEFAULT_SERVICE_WORKER_CONTROLLER);
            }
        }
        return mDefaultServiceWorkerController;
    }

    public android.webkit.WebIconDatabase getWebIconDatabase() {
        synchronized (mLock) {
            ensureChromiumStartedLocked(true, CallSite.GET_WEB_ICON_DATABASE);
            WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_GET_INSTANCE);
            if (mWebIconDatabase == null) {
                mWebIconDatabase = new WebIconDatabaseAdapter();
            }
        }
        return mWebIconDatabase;
    }

    public WebStorage getDefaultWebStorage() {
        synchronized (mLock) {
            if (mDefaultWebStorage == null) {
                ensureChromiumStartedLocked(true, CallSite.GET_DEFAULT_WEB_STORAGE);
            }
        }
        return mDefaultWebStorage;
    }

    public WebViewDatabase getDefaultWebViewDatabase(final Context context) {
        synchronized (mLock) {
            ensureChromiumStartedLocked(true, CallSite.GET_DEFAULT_WEBVIEW_DATABASE);
            if (mDefaultWebViewDatabase == null) {
                mDefaultWebViewDatabase =
                        new WebViewDatabaseAdapter(
                                mFactory,
                                HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE),
                                mDefaultBrowserContext);
            }
        }
        return mDefaultWebViewDatabase;
    }

    // See comments in VariationsSeedLoader.java on when it's safe to call this.
    public void startVariationsInit() {
        synchronized (mLock) {
            if (mSeedLoader == null) {
                mSeedLoader = new VariationsSeedLoader();
                mSeedLoader.startVariationsInit();
            }
        }
    }

    private void finishVariationsInitLocked() {
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.finishVariationsInitLocked")) {
            assert Thread.holdsLock(mLock);
            if (mSeedLoader == null) {
                Log.e(TAG, "finishVariationsInitLocked() called before startVariationsInit()");
                startVariationsInit();
            }
            mSeedLoader.finishVariationsInit();
            mSeedLoader = null; // Allow this to be GC'd after its background thread finishes.
        }
    }

    // Log extra information, for debugging purposes. Do the work asynchronously to avoid blocking
    // startup.
    private void logCommandLineAndActiveTrials() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    // TODO(ntfschr): CommandLine can change at any time. For simplicity, only log
                    // it once during startup.
                    AwContentsStatics.logCommandLineForDebugging();
                    // Field trials can be activated at any time. We'll continue logging them as
                    // they're activated.
                    FieldTrialList.logActiveTrials();
                    // SafeMode was already determined earlier during the startup sequence, this
                    // just fetches the cached boolean state. If SafeMode was enabled, we already
                    // logged detailed information about the SafeMode config.
                    Log.i(TAG, "SafeMode enabled: " + mFactory.isSafeModeEnabled());
                });
    }

    public WebViewChromiumRunQueue getRunQueue() {
        return mFactory.getRunQueue();
    }
}
