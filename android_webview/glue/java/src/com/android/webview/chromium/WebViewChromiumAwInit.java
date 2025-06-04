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
import android.os.flagging.AconfigPackage;
import android.os.storage.StorageManager;
import android.provider.DeviceConfig;
import android.provider.DeviceConfig.Properties;
import android.util.Log;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebViewDatabase;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
import org.chromium.android_webview.metrics.TrackExitReasons;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.android_webview.variations.VariationsSeedLoader;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.PathService;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryPrefetcher;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ResourceBundle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayDeque;
import java.util.Locale;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;

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

    private static final String REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME =
            "Android.WebView.RegisterResourcePathsAvailable2";

    public static class WebViewStartUpDiagnostics {
        private final Object mLock = new Object();

        @GuardedBy("mLock")
        private Long mTotalTimeUiThreadChromiumInitMillis;

        @GuardedBy("mLock")
        private Long mMaxTimePerTaskUiThreadChromiumInitMillis;

        @GuardedBy("mLock")
        private Throwable mSynchronousChromiumInitLocation;

        @GuardedBy("mLock")
        private Throwable mProviderInitOnMainLooperLocation;

        public Long getTotalTimeUiThreadChromiumInitMillis() {
            synchronized (mLock) {
                return mTotalTimeUiThreadChromiumInitMillis;
            }
        }

        public Long getMaxTimePerTaskUiThreadChromiumInitMillis() {
            synchronized (mLock) {
                return mMaxTimePerTaskUiThreadChromiumInitMillis;
            }
        }

        public @Nullable Throwable getSynchronousChromiumInitLocationOrNull() {
            synchronized (mLock) {
                return mSynchronousChromiumInitLocation;
            }
        }

        public @Nullable Throwable getProviderInitOnMainLooperLocationOrNull() {
            synchronized (mLock) {
                return mProviderInitOnMainLooperLocation;
            }
        }

        void setTotalTimeUiThreadChromiumInitMillis(Long time) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mTotalTimeUiThreadChromiumInitMillis == null);
                mTotalTimeUiThreadChromiumInitMillis = time;
            }
        }

        void setMaxTimePerTaskUiThreadChromiumInitMillis(Long time) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mMaxTimePerTaskUiThreadChromiumInitMillis == null);
                mMaxTimePerTaskUiThreadChromiumInitMillis = time;
            }
        }

        void setSynchronousChromiumInitLocation(Throwable t) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mSynchronousChromiumInitLocation == null);
                mSynchronousChromiumInitLocation = t;
            }
        }

        void setProviderInitOnMainLooperLocation(Throwable t) {
            synchronized (mLock) {
                // The setter should only be called once.
                assert (mProviderInitOnMainLooperLocation == null);
                mProviderInitOnMainLooperLocation = t;
            }
        }
    }

    public interface WebViewStartUpCallback {
        void onSuccess(WebViewStartUpDiagnostics result);
    }

    @GuardedBy("mLazyInitLock")
    private CookieManagerAdapter mDefaultCookieManager;

    @GuardedBy("mLazyInitLock")
    private WebIconDatabaseAdapter mWebIconDatabase;

    @GuardedBy("mLazyInitLock")
    private WebViewDatabaseAdapter mDefaultWebViewDatabase;

    // Volatile to guard for incorrectly trying to use this without calling `startChromium`.
    // TODO(crbug.com/389871700): Consider hiding the variable where it can't be incorrectly
    // accessed. See crrev.com/c/6081452/comment/9dff4e5e_c049d778/ for context.
    private volatile ChromiumStartedGlobals mChromiumStartedGlobals;

    private final Object mSeedLoaderLock = new Object();

    @GuardedBy("mSeedLoaderLock")
    private VariationsSeedLoader mSeedLoader;

    // This is only accessed during WebViewChromiumFactoryProvider.initialize() which is guarded by
    // the WebViewFactory lock in the framework, and on the UI thread during startChromium
    // which cannot be called before initialize() has completed.
    private Thread mSetUpResourcesThread;

    // Guards access to fields that are initialized on first use rather than by startChromium.
    // This lock is used across WebViewChromium startup classes ie WebViewChromiumAwInit,
    // SupportLibWebViewChromiumFactory and WebViewChromiumFactoryProvider so as to avoid deadlock.
    // TODO(crbug.com/397385172): Get rid of this lock.
    private final Object mLazyInitLock = new Object();

    private final Object mThreadSettingLock = new Object();

    @GuardedBy("mThreadSettingLock")
    private boolean mThreadIsSet;

    private final CountDownLatch mStartupFinished = new CountDownLatch(1);

    // mInitState should only transition INIT_NOT_STARTED -> INIT_FINISHED
    private static final int INIT_NOT_STARTED = 0;
    private static final int INIT_FINISHED = 1;

    private final AtomicInteger mInitState = new AtomicInteger(INIT_NOT_STARTED);

    private final WebViewChromiumFactoryProvider mFactory;
    private final WebViewStartUpDiagnostics mWebViewStartUpDiagnostics =
            new WebViewStartUpDiagnostics();
    private final WebViewChromiumRunQueue mWebViewStartUpCallbackRunQueue =
            new WebViewChromiumRunQueue();

    private final AtomicInteger mChromiumFirstStartupRequestMode =
            new AtomicInteger(StartupTasksRunner.UNSET);
    // Only accessed from the UI thread
    private StartupTasksRunner mStartupTasksRunner;
    private boolean mIsStartupTaskExperimentEnabled;
    private RuntimeException mStartupException;
    private Error mStartupError;

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
        CallSite.ASYNC_WEBVIEW_STARTUP,
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
        int ASYNC_WEBVIEW_STARTUP = 10;
        // Remember to update WebViewStartupCallSite in enums.xml when adding new values here.
        int COUNT = 11;
    };

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
        TraceEvent.maybeEnableEarlyTracing(/* readCommandLine= */ false);
    }

    public AwTracingController getAwTracingController() {
        triggerAndWaitForChromiumStarted(true, CallSite.GET_AW_TRACING_CONTROLLER);
        return mChromiumStartedGlobals.mAwTracingController;
    }

    public AwProxyController getAwProxyController() {
        triggerAndWaitForChromiumStarted(true, CallSite.GET_AW_PROXY_CONTROLLER);
        return mChromiumStartedGlobals.mAwProxyController;
    }

    public void setProviderInitOnMainLooperLocation(Throwable t) {
        mWebViewStartUpDiagnostics.setProviderInitOnMainLooperLocation(t);
    }

    // TODO: DIR_RESOURCE_PAKS_ANDROID needs to live somewhere sensible,
    // inlined here for simplicity setting up the HTMLViewer demo. Unfortunately
    // it can't go into base.PathService, as the native constant it refers to
    // lives in the ui/ layer. See ui/base/ui_base_paths.h
    private static final int DIR_RESOURCE_PAKS_ANDROID = 3003;

    private void startChromium(@CallSite int callSite, boolean triggeredFromUIThread) {
        assert ThreadUtils.runningOnUiThread();

        if (mInitState.get() == INIT_FINISHED) {
            return;
        }

        if (mIsStartupTaskExperimentEnabled) {
            if (mStartupException != null) {
                throw mStartupException;
            } else if (mStartupError != null) {
                throw mStartupError;
            }

            // This can be non-null for async-then-sync or multiple-async calls.
            if (mStartupTasksRunner == null) {
                mStartupTasksRunner = initializeStartupTasksRunner();
            }
        } else {
            // Makes sure we run all of the startup tasks.
            mStartupTasksRunner = initializeStartupTasksRunner();
        }

        mStartupTasksRunner.run(callSite, triggeredFromUIThread);
    }

    // Called once during the WebViewChromiumFactoryProvider initialization
    void setStartupTaskExperimentEnabled(boolean enabled) {
        assert mInitState.get() == INIT_NOT_STARTED;
        mIsStartupTaskExperimentEnabled = enabled;
    }

    // Initializes a new StartupTaskRunner with a list of tasks to run for chromium startup.
    // Postcondition of calling `.run` on the returned StartupTasksRunner is that Chromium startup
    // is finished.
    private StartupTasksRunner initializeStartupTasksRunner() {
        ArrayDeque<Runnable> tasks = new ArrayDeque<>();
        tasks.addLast(
                () -> {
                    if (mIsStartupTaskExperimentEnabled) {
                        // Disable java-side PostTask scheduling. The native-side task runners are
                        // also disabled in the native code. The unscheduled prenative tasks are
                        // migrated to the native task runner. The native task runner is enabled
                        // when we are done with startup.
                        PostTask.disablePreNativeUiTasks(true);
                    }

                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        TrackExitReasons.startTrackingStartup();
                    }

                    final Context context = ContextUtils.getApplicationContext();

                    ResourceBundle.setAvailablePakLocales(
                            AwLocaleConfig.getWebViewSupportedPakLocales());

                    try (ScopedSysTraceEvent e =
                            ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.LibraryLoader")) {
                        LibraryLoader.getInstance().ensureInitialized();
                    }

                    // TODO(crbug.com/400414092): These should be obsolete now.
                    PathService.override(PathService.DIR_MODULE, "/system/lib/");
                    PathService.override(
                            DIR_RESOURCE_PAKS_ANDROID, "/system/framework/webview/paks");

                    initPlatSupportLibrary();
                    AwContentsStatics.setCheckClearTextPermitted(
                            context.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.O);

                    waitUntilSetUpResources();
                    // NOTE: Finished writing Java resources. From this point on, it's safe
                    // to use them.

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
                        RecordHistogram.recordBooleanHistogram(
                                ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, false);
                    } catch (Resources.NotFoundException e) {
                        RecordHistogram.recordBooleanHistogram(
                                ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, true);
                        mFactory.addWebViewAssetPath(context);
                    }

                    AwBrowserProcess.configureChildProcessLauncher();

                    // finishVariationsInitLocked() must precede native initialization so
                    // the seed is available when AwFeatureListCreator::SetUpFieldTrials()
                    // runs.
                    if (!FastVariationsSeedSafeModeAction.hasRun()) {
                        finishVariationsInitLocked();
                    }
                });
        tasks.addLast(AwBrowserProcess::start);
        tasks.addLast(
                () -> {
                    // TODO(crbug.com/332706093): See if this can be moved before loading
                    // native.
                    AwClassPreloader.preloadClasses();

                    AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(
                            /* updateMetricsConsent= */ true);
                    doNetworkInitializations(ContextUtils.getApplicationContext());
                });

        // This has to be done after variations are initialized, so components could
        // be registered or not depending on the variations flags.
        tasks.addLast(AwBrowserProcess::loadComponents);
        tasks.addLast(
                () -> {
                    AwBrowserProcess.initializeMetricsLogUploader();

                    int targetSdkVersion =
                            ContextUtils.getApplicationContext()
                                    .getApplicationInfo()
                                    .targetSdkVersion;
                    RecordHistogram.recordSparseHistogram(
                            "Android.WebView.TargetSdkVersion", targetSdkVersion);

                    try (ScopedSysTraceEvent e =
                            ScopedSysTraceEvent.scoped(
                                    "WebViewChromiumAwInit.initThreadUnsafeSingletons")) {
                        mChromiumStartedGlobals = new ChromiumStartedGlobals(mFactory);
                    }

                    if (BuildInfo.isDebugAndroidOrApp()) {
                        mChromiumStartedGlobals.mSharedStatics
                                .setWebContentsDebuggingEnabledUnconditionally(true);
                    }

                    if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                            ? CompatChanges.isChangeEnabled(WebSettings.ENABLE_SIMPLIFIED_DARK_MODE)
                            : targetSdkVersion >= Build.VERSION_CODES.TIRAMISU) {
                        AwDarkMode.enableSimplifiedDarkMode();
                    }

                    if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_VERBOSE_LOGGING)) {
                        logCommandLineAndActiveTrials();
                    }

                    PostTask.postTask(
                            TaskTraits.BEST_EFFORT,
                            () -> {
                                WebViewCachedFlags.get()
                                        .onStartupCompleted(mFactory.getWebViewPrefs());
                            });

                    if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_PREFETCH_NATIVE_LIBRARY)
                            && !AwFeatureMap.getInstance()
                                    .getFieldTrialParamByFeatureAsBoolean(
                                            AwFeatures.WEBVIEW_PREFETCH_NATIVE_LIBRARY,
                                            "WebViewPrefetchFromRenderer",
                                            false)) {
                        PostTask.postTask(
                                TaskTraits.BEST_EFFORT,
                                () -> {
                                    LibraryPrefetcher.prefetchNativeLibraryForWebView();
                                });
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                        PostTask.postTask(
                                TaskTraits.BEST_EFFORT, this::logRegisterResourcePathsAvailability);
                    }

                    if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_RECORD_APP_CACHE_HISTOGRAMS)) {
                        PostTask.postDelayedTask(
                                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                                () -> {
                                    StorageManager storageManager =
                                            (StorageManager)
                                                    ContextUtils.getApplicationContext()
                                                            .getSystemService(
                                                                    Context.STORAGE_SERVICE);
                                    UUID storageUuid =
                                            ContextUtils.getApplicationContext()
                                                    .getApplicationInfo()
                                                    .storageUuid;
                                    long startTimeGetCacheQuotaMs = SystemClock.uptimeMillis();
                                    long cacheQuotaKiloBytes = -1;
                                    try {
                                        // This can throw `SecurityException` if the app doesn't
                                        // have sufficient privileges.
                                        // See crbug.com/422174715
                                        cacheQuotaKiloBytes =
                                                storageManager.getCacheQuotaBytes(storageUuid)
                                                        / 1024;
                                        RecordHistogram.recordCount1MHistogram(
                                                "Android.WebView.CacheQuotaSize",
                                                (int) cacheQuotaKiloBytes);
                                    } catch (Exception e) {
                                    } finally {
                                        RecordHistogram.recordTimesHistogram(
                                                "Android.WebView.GetCacheQuotaSizeTime",
                                                SystemClock.uptimeMillis()
                                                        - startTimeGetCacheQuotaMs);
                                    }

                                    long startTimeGetCacheSizeMs = SystemClock.uptimeMillis();
                                    long cacheSizeKiloBytes = -1;
                                    try {
                                        // This can throw `SecurityException` if the app doesn't
                                        // have sufficient privileges.
                                        // See crbug.com/422174715
                                        cacheSizeKiloBytes =
                                                storageManager.getCacheSizeBytes(storageUuid)
                                                        / 1024;
                                        RecordHistogram.recordCount1MHistogram(
                                                "Android.WebView.CacheSize",
                                                (int) cacheSizeKiloBytes);
                                    } catch (Exception e) {
                                    } finally {
                                        RecordHistogram.recordTimesHistogram(
                                                "Android.WebView.GetCacheSizeTime",
                                                SystemClock.uptimeMillis()
                                                        - startTimeGetCacheSizeMs);
                                    }
                                    if (cacheQuotaKiloBytes != -1 && cacheSizeKiloBytes != -1) {
                                        long quotaRemainingKiloBytes =
                                                cacheQuotaKiloBytes - cacheSizeKiloBytes;
                                        if (quotaRemainingKiloBytes >= 0) {
                                            RecordHistogram.recordCount1MHistogram(
                                                    "Android.WebView.CacheSizeWithinQuota",
                                                    (int) quotaRemainingKiloBytes);
                                        } else {
                                            RecordHistogram.recordCount1MHistogram(
                                                    "Android.WebView.CacheSizeExceedsQuota",
                                                    -1 * (int) quotaRemainingKiloBytes);
                                        }
                                    }
                                },
                                5000);
                    }
                    AwCrashyClassUtils.maybeCrashIfEnabled();
                    // Must happen right after Chromium initialization is complete.
                    mInitState.set(INIT_FINISHED);
                    mStartupFinished.countDown();
                    // This runs all the pending tasks queued for after Chromium init is
                    // finished, so should run after `mInitState` is `INIT_FINISHED`.
                    mFactory.getRunQueue().notifyChromiumStarted();
                    if (mIsStartupTaskExperimentEnabled) {
                        // Re-enables the taskrunners
                        PostTask.disablePreNativeUiTasks(false);
                        AwBrowserProcess.onStartupComplete();
                    }
                });

        return new StartupTasksRunner(tasks);
    }

    private void recordStartupMetrics(
            @CallSite int startCallSite,
            @CallSite int finishCallSite,
            long startTimeMs,
            long totalTimeTakenMs,
            long longestUiBlockingTaskTimeMs,
            @StartupTasksRunner.StartupMode int startupMode) {
        long wallClockTimeMs = SystemClock.uptimeMillis() - startTimeMs;
        // Record asyncStartup API metrics
        mWebViewStartUpDiagnostics.setTotalTimeUiThreadChromiumInitMillis(totalTimeTakenMs);
        mWebViewStartUpDiagnostics.setMaxTimePerTaskUiThreadChromiumInitMillis(
                longestUiBlockingTaskTimeMs);
        mWebViewStartUpCallbackRunQueue.notifyChromiumStarted();

        // Record histograms
        String startupModeString =
                switch (startupMode) {
                    case StartupTasksRunner.StartupMode.FULLY_SYNC -> ".FullySync";
                    case StartupTasksRunner.StartupMode.FULLY_ASYNC -> ".FullyAsync";
                    case StartupTasksRunner.StartupMode
                            .ASYNC_BUT_FULLY_SYNC -> ".AsyncButFullySync";
                    case StartupTasksRunner.StartupMode
                            .PARTIAL_ASYNC_THEN_SYNC -> ".PartialAsyncThenSync";
                    default -> ".Unknown";
                };
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.CreationTime.StartChromiumLocked", totalTimeTakenMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.CreationTime.StartChromiumLocked" + startupModeString,
                totalTimeTakenMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.LongestUiBlockingTaskTime",
                longestUiBlockingTaskTimeMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.LongestUiBlockingTaskTime"
                        + startupModeString,
                longestUiBlockingTaskTimeMs);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Startup.ChromiumInitTime.StartupMode",
                startupMode,
                StartupTasksRunner.StartupMode.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Startup.CreationTime.InitReason", startCallSite, CallSite.COUNT);
        if (startupMode == StartupTasksRunner.StartupMode.ASYNC_BUT_FULLY_SYNC
                || startupMode == StartupTasksRunner.StartupMode.PARTIAL_ASYNC_THEN_SYNC) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.WebView.Startup.ChromiumInitTime.AsyncToSyncSwitchReason",
                    finishCallSite,
                    CallSite.COUNT);
        }
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.WallClockTime", wallClockTimeMs);
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.ChromiumInitTime.WallClockTime" + startupModeString,
                wallClockTimeMs);

        // Record traces
        TraceEvent.webViewStartupStartChromiumLocked(
                startTimeMs,
                totalTimeTakenMs,
                /* startCallSite= */ startCallSite,
                /* finishCallSite= */ finishCallSite,
                /* startupMode= */ startupMode);
        // Also create the trace events for the earlier WebViewChromiumFactoryProvider init, which
        // happens before tracing is ready.
        TraceEvent.webViewStartupTotalFactoryInit(
                mFactory.getInitInfo().mTotalFactoryInitStartTime,
                mFactory.getInitInfo().mTotalFactoryInitDuration);
        TraceEvent.webViewStartupStage1(
                mFactory.getInitInfo().mStartTime, mFactory.getInitInfo().mDuration);
    }

    /**
     * Set up resources on a background thread. This method is called once during
     * WebViewChromiumFactoryProvider initialization which is guaranteed to finish before this field
     * is accessed by waitUntilSetUpResources.
     *
     * @param context The context.
     */
    void setUpResourcesOnBackgroundThread(int packageId, Context context) {
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

    boolean isChromiumInitialized() {
        return mInitState.get() == INIT_FINISHED;
    }

    void startYourEngines(boolean fromThreadSafeFunction) {
        // TODO(crbug.com/389871700): Consider inlining this method call. See
        // crrev.com/c/6081452/comment/96be8119_fedb4983 for reasoning.
        triggerAndWaitForChromiumStarted(fromThreadSafeFunction, CallSite.WEBVIEW_INSTANCE);
    }

    // This method is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    // Postcondition: Chromium startup is finished when this method returns.
    void triggerAndWaitForChromiumStarted(boolean fromThreadSafeFunction, @CallSite int callSite) {
        if (triggerChromiumStartupAndReturnTrueIfStartupIsFinished(
                fromThreadSafeFunction, callSite)) {
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

        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.waitForUIThreadInit")) {
            long startTime = SystemClock.uptimeMillis();
            // Wait for the UI thread to finish init.
            while (true) {
                try {
                    mStartupFinished.await();
                    break;
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

    /**
     * Triggers Chromium startup. Directly runs startup if called from the UI thread, else, posts
     * startup to the UI thread to be completed in the near future.
     *
     * @returns true if Chromium startup if finished, false if startup will be finished in the near
     *     future. If false, caller may choose to wait on the {@code mStartupFinished} latch, or
     *     {@link WebViewStartUpCallback}.
     */
    private boolean triggerChromiumStartupAndReturnTrueIfStartupIsFinished(
            boolean fromThreadSafeFunction, @CallSite int callSite) {
        if (mInitState.get() == INIT_FINISHED) { // Early-out for the common case.
            return true;
        }

        maybeSetChromiumUiThread(fromThreadSafeFunction);

        mChromiumFirstStartupRequestMode.compareAndSet(
                StartupTasksRunner.UNSET,
                ThreadUtils.runningOnUiThread()
                        ? StartupTasksRunner.SYNC
                        : StartupTasksRunner.ASYNC);
        if (ThreadUtils.runningOnUiThread()) {
            mWebViewStartUpDiagnostics.setSynchronousChromiumInitLocation(
                    new Throwable(
                            "Location where Chromium init was started synchronously on the UI"
                                    + " thread"));
            // If we are currently running on the UI thread then we must do init now. If there was
            // already a task posted to the UI thread from another thread to do it, it will just
            // no-op when it runs.
            startChromium(callSite, /* triggeredFromUIThread= */ true);
            return true;
        }

        // If we're not running on the UI thread (because init was triggered by a thread-safe
        // function), post init to the UI thread, since init is *not* thread-safe.
        // TODO(crbug.com/397372092): Consider checking if async startup is in progress so as not to
        // bother posting.
        AwThreadUtils.postToUiThreadLooper(
                () -> startChromium(callSite, /* triggeredFromUIThread= */ false));
        return false;
    }

    private void maybeSetChromiumUiThread(boolean fromThreadSafeFunction) {
        synchronized (mThreadSettingLock) {
            if (mThreadIsSet) {
                return;
            }

            // If we're being started from a function that's allowed to be called on any thread,
            // then we can't just assume the current thread is the UI thread; instead we assume
            // the process's main looper will be the UI thread, because that's the case for
            // almost all Android apps.
            //
            // If we're being started from a function that must be called from the UI
            // thread, then by definition the current thread is the UI thread whether it's the
            // main looper or not.
            Looper looper = fromThreadSafeFunction ? Looper.getMainLooper() : Looper.myLooper();
            Log.v(
                    TAG,
                    "Binding Chromium to "
                            + (Looper.getMainLooper().equals(looper) ? "main" : "background")
                            + " looper "
                            + looper);
            ThreadUtils.setUiThread(looper);
            mThreadIsSet = true;
        }
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

    AwBrowserContext getDefaultBrowserContextOnUiThread() {
        if (BuildConfig.ENABLE_ASSERTS && !ThreadUtils.runningOnUiThread()) {
            throw new RuntimeException(
                    "getBrowserContextOnUiThread called on " + Thread.currentThread());
        }
        return mChromiumStartedGlobals.mDefaultBrowserContext;
    }

    public SharedStatics getStatics() {
        // TODO: Optimization potential: most of the static methods only need the native
        // library loaded and initialized, not the entire browser process started.
        triggerAndWaitForChromiumStarted(true, CallSite.GET_STATICS);
        return mChromiumStartedGlobals.mSharedStatics;
    }

    public GeolocationPermissions getDefaultGeolocationPermissions() {
        triggerAndWaitForChromiumStarted(true, CallSite.GET_DEFAULT_GEOLOCATION_PERMISSIONS);
        return mChromiumStartedGlobals.mDefaultGeolocationPermissions;
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

    public AwServiceWorkerController getDefaultServiceWorkerController() {
        triggerAndWaitForChromiumStarted(true, CallSite.GET_DEFAULT_SERVICE_WORKER_CONTROLLER);
        return mChromiumStartedGlobals.mDefaultServiceWorkerController;
    }

    public android.webkit.WebIconDatabase getWebIconDatabase() {
        triggerAndWaitForChromiumStarted(true, CallSite.GET_WEB_ICON_DATABASE);
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_ICON_DATABASE_GET_INSTANCE);
        synchronized (mLazyInitLock) {
            if (mWebIconDatabase == null) {
                mWebIconDatabase = new WebIconDatabaseAdapter();
            }
            return mWebIconDatabase;
        }
    }

    public WebStorage getDefaultWebStorage() {
        triggerAndWaitForChromiumStarted(true, CallSite.GET_DEFAULT_WEB_STORAGE);
        return mChromiumStartedGlobals.mDefaultWebStorage;
    }

    public WebViewDatabase getDefaultWebViewDatabase(final Context context) {
        triggerAndWaitForChromiumStarted(true, CallSite.GET_DEFAULT_WEBVIEW_DATABASE);
        synchronized (mLazyInitLock) {
            if (mDefaultWebViewDatabase == null) {
                mDefaultWebViewDatabase =
                        new WebViewDatabaseAdapter(
                                mFactory,
                                HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE),
                                mChromiumStartedGlobals.mDefaultBrowserContext);
            }
            return mDefaultWebViewDatabase;
        }
    }

    // See comments in VariationsSeedLoader.java on when it's safe to call this.
    void startVariationsInit() {
        synchronized (mSeedLoaderLock) {
            if (mSeedLoader == null) {
                mSeedLoader = new VariationsSeedLoader();
                mSeedLoader.startVariationsInit();
            }
        }
    }

    private void finishVariationsInitLocked() {
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.finishVariationsInitLocked")) {
            synchronized (mSeedLoaderLock) {
                if (mSeedLoader == null) {
                    Log.e(TAG, "finishVariationsInitLocked() called before startVariationsInit()");
                    startVariationsInit();
                }
                mSeedLoader.finishVariationsInit();
                mSeedLoader = null; // Allow this to be GC'd after its background thread finishes.
            }
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

    public Object getLazyInitLock() {
        return mLazyInitLock;
    }

    // Starts up WebView asynchronously.
    // MUST NOT be called on the UI thread.
    // The callback can either be called synchronously or on the UI thread.
    public void startUpWebView(
            @NonNull WebViewStartUpCallback callback, boolean shouldRunUiThreadStartUpTasks) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            throw new IllegalStateException(
                    "startUpWebView should not be called on the Android main looper");
        }
        if (!shouldRunUiThreadStartUpTasks) {
            callback.onSuccess(mWebViewStartUpDiagnostics);
            return;
        }

        // TODO(crbug.com/389871700): We should also early out if the diagnostics information has
        // been set.
        mWebViewStartUpCallbackRunQueue.addTask(
                () -> callback.onSuccess(mWebViewStartUpDiagnostics));
        triggerChromiumStartupAndReturnTrueIfStartupIsFinished(
                true, CallSite.ASYNC_WEBVIEW_STARTUP);
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ResourcePathsApi.DISABLED, ResourcePathsApi.ENABLED, ResourcePathsApi.ERROR})
    private @interface ResourcePathsApi {
        int DISABLED = 0;
        int ENABLED = 1;
        int ERROR = 2;
        int NUM_ENTRIES = 3;
    }

    /** Logs whether the registerResourcePaths API is available to use. */
    private void logRegisterResourcePathsAvailability() {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                Properties properties = DeviceConfig.getProperties("resource_manager");
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        properties.getBoolean("android.content.res.register_resource_paths", false)
                                ? ResourcePathsApi.ENABLED
                                : ResourcePathsApi.DISABLED,
                        ResourcePathsApi.NUM_ENTRIES);
            } catch (Exception e) {
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        ResourcePathsApi.ERROR,
                        ResourcePathsApi.NUM_ENTRIES);
            }
        } else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.BAKLAVA) {
            try {
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        AconfigPackage.load("android.content.res")
                                        .getBooleanFlagValue("register_resource_paths", false)
                                ? ResourcePathsApi.ENABLED
                                : ResourcePathsApi.DISABLED,
                        ResourcePathsApi.NUM_ENTRIES);
            } catch (Exception e) {
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        ResourcePathsApi.ERROR,
                        ResourcePathsApi.NUM_ENTRIES);
            }
        }
    }

    // These are objects that need to be created on the UI thread and after chromium has started.
    // Thus created during startChromium for ease.
    private static final class ChromiumStartedGlobals {
        final AwBrowserContext mDefaultBrowserContext;
        final GeolocationPermissionsAdapter mDefaultGeolocationPermissions;
        final WebStorageAdapter mDefaultWebStorage;
        final AwServiceWorkerController mDefaultServiceWorkerController;
        final AwTracingController mAwTracingController;
        final AwProxyController mAwProxyController;
        final SharedStatics mSharedStatics;

        ChromiumStartedGlobals(WebViewChromiumFactoryProvider factory) {
            mSharedStatics = new SharedStatics();
            mDefaultBrowserContext = AwBrowserContext.getDefault();
            mDefaultGeolocationPermissions =
                    new GeolocationPermissionsAdapter(
                            factory, mDefaultBrowserContext.getGeolocationPermissions());
            mDefaultWebStorage =
                    new WebStorageAdapter(factory, mDefaultBrowserContext.getQuotaManagerBridge());
            mAwTracingController = new AwTracingController();
            mDefaultServiceWorkerController = mDefaultBrowserContext.getServiceWorkerController();
            mAwProxyController = new AwProxyController();
        }
    }

    // This class is responsible for running chromium startup tasks asynchronously or synchronously
    // depending on if startup is triggered from the background or UI thread.
    private final class StartupTasksRunner {
        private final ArrayDeque<Runnable> mQueue;
        private final int mNumTasks;
        private boolean mAsyncHasBeenTriggered;
        private long mLongestUiBlockingTaskTimeMs;
        private long mTotalTimeTakenMs;
        private long mStartupTimeMs;
        private boolean mStartupStarted;
        private @CallSite int mStartCallSite = CallSite.COUNT;
        private @CallSite int mFinishCallSite = CallSite.COUNT;
        private boolean mFirstTaskFromSynchronousCall;

        private static final int UNSET = 0;
        private static final int SYNC = 1;
        private static final int ASYNC = 2;

        // LINT.IfChange(WebViewChromiumStartupMode)
        @IntDef({
            StartupMode.FULLY_SYNC,
            StartupMode.FULLY_ASYNC,
            StartupMode.PARTIAL_ASYNC_THEN_SYNC,
            StartupMode.ASYNC_BUT_FULLY_SYNC,
            StartupMode.COUNT,
        })
        @interface StartupMode {
            // Startup was triggered on the UI thread and completed synchronously
            int FULLY_SYNC = 0;
            // Startup was triggered on a background thread and completed asynchronously
            int FULLY_ASYNC = 1;
            // Startup was triggered on a background thread, some tasks ran asynchronously. Then
            // another init call on the UI thread preempted the async run and startup completed
            // synchronously
            int PARTIAL_ASYNC_THEN_SYNC = 2;
            // Startup was triggered on a background thread, the posted task was not run yet. Then
            // another init call on the UI thread was started before the posted task and startup
            // fully completed synchronously
            int ASYNC_BUT_FULLY_SYNC = 3;
            // Remember to update WebViewStartupMode in enums.xml when adding new values here.
            int COUNT = 4;
        };

        // LINT.ThenChange(//base/tracing/protos/chrome_track_event.proto:WebViewChromiumStartupMode)

        StartupTasksRunner(ArrayDeque<Runnable> tasks) {
            mQueue = tasks;
            mNumTasks = tasks.size();
        }

        void run(@CallSite int callSite, boolean triggeredFromUIThread) {
            assert ThreadUtils.runningOnUiThread();

            if (!mStartupStarted) {
                mStartupStarted = true;
                mFirstTaskFromSynchronousCall = triggeredFromUIThread;
                mStartCallSite = callSite;
                if (mStartCallSite == CallSite.GET_STATICS) {
                    SharedStatics.setStartupTriggered();
                }
                mFinishCallSite = callSite;
                mStartupTimeMs = SystemClock.uptimeMillis();
            }

            // Early return to avoid repeating the return call within sync and async blocks
            if (mQueue.isEmpty()) {
                assert mInitState.get() == INIT_FINISHED;
                return;
            }

            if (mIsStartupTaskExperimentEnabled && !triggeredFromUIThread) {
                // Prevents triggering async run multiple times and thus reduce the interval between
                // tasks.
                if (mAsyncHasBeenTriggered) {
                    return;
                }
                mAsyncHasBeenTriggered = true;
                runAsyncStartupTaskAndPostNext(/* taskNum= */ 1);
            } else {
                // This lets us track the reason for a sync finish, especially relevant if we
                // started off asynchronously.
                mFinishCallSite = callSite;
                try (ScopedSysTraceEvent event =
                        ScopedSysTraceEvent.scoped(
                                "WebViewChromiumAwInit.startChromiumLockedSync")) {
                    timedRunWithExceptionHandling(
                            () -> {
                                while (!mQueue.isEmpty()) {
                                    mQueue.poll().run();
                                }
                            },
                            SYNC);
                }
            }
        }

        private void runAsyncStartupTaskAndPostNext(int taskNum) {
            assert ThreadUtils.runningOnUiThread();

            if (mQueue.isEmpty()) {
                return;
            }

            try (ScopedSysTraceEvent event =
                    ScopedSysTraceEvent.scoped(
                            String.format(
                                    Locale.US,
                                    "WebViewChromiumAwInit.startChromiumLockedAsync_task%d/%d",
                                    taskNum,
                                    mNumTasks))) {
                timedRunWithExceptionHandling(mQueue.poll(), ASYNC);
            }

            if (!mQueue.isEmpty()) { // Avoids unnecessarily posting to the UI thread
                AwThreadUtils.postToUiThreadLooper(
                        () -> runAsyncStartupTaskAndPostNext(taskNum + 1));
            }
        }

        // Runs the startup task while keeping track of metrics and dealing with exceptions
        private void timedRunWithExceptionHandling(Runnable task, int runMode) {
            assert ThreadUtils.runningOnUiThread();

            try {
                long startTimeMs = SystemClock.uptimeMillis();
                task.run();
                long durationMs = SystemClock.uptimeMillis() - startTimeMs;

                mLongestUiBlockingTaskTimeMs = Math.max(mLongestUiBlockingTaskTimeMs, durationMs);
                mTotalTimeTakenMs += durationMs;
                if (mQueue.isEmpty()) {
                    // We are done running all the tasks, so record the metrics.
                    recordStartupMetrics(
                            mStartCallSite,
                            mFinishCallSite,
                            /* startTimeMs= */ mStartupTimeMs,
                            /* totalTimeTakenMs= */ mTotalTimeTakenMs,
                            /* longestUiBlockingTaskTimeMs= */ mLongestUiBlockingTaskTimeMs,
                            calculateStartupMode(runMode));
                }
            } catch (RuntimeException | Error e) {
                Log.e(TAG, "WebView chromium startup failed", e);
                if (e instanceof RuntimeException re) {
                    mStartupException = re;
                } else {
                    mStartupError = (Error) e;
                }
                throw e;
            }
        }

        // To determine the startup mode, we track:
        // 1. Whether the initial startup request was synchronous or asynchronous.
        // 2. Whether the first task ran synchronously or asynchronously.
        // 3. Whether the last task ran synchronously or asynchronously.
        private @StartupMode int calculateStartupMode(int lastTaskRunMode) {
            // The control arm of our experiment runs fully synchronously.
            if (!mIsStartupTaskExperimentEnabled) {
                return StartupMode.FULLY_SYNC;
            }

            if (mFirstTaskFromSynchronousCall) {
                return mChromiumFirstStartupRequestMode.get() == SYNC
                        ? StartupMode.FULLY_SYNC
                        : StartupMode.ASYNC_BUT_FULLY_SYNC;
            }
            return lastTaskRunMode == SYNC
                    ? StartupMode.PARTIAL_ASYNC_THEN_SYNC
                    : StartupMode.FULLY_ASYNC;
        }
    }
}
