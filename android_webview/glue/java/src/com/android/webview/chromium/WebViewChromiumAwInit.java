// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.compat.CompatChanges;
import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.webkit.CookieManager;
import android.webkit.WebIconDatabase;
import android.webkit.WebSettings;
import android.webkit.WebViewDatabase;

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;

import com.android.webview.chromium.ApiCallLogger.ApiCall;
import com.android.webview.chromium.ApiCallLogger.ApiCallUserAction;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwClassPreloader;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwCrashyClassUtils;
import org.chromium.android_webview.AwDarkMode;
import org.chromium.android_webview.AwLocaleConfig;
import org.chromium.android_webview.AwProxyController;
import org.chromium.android_webview.AwThreadUtils;
import org.chromium.android_webview.AwTracingController;
import org.chromium.android_webview.DualTraceEvent;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.android_webview.R;
import org.chromium.android_webview.StartupCallSite;
import org.chromium.android_webview.StartupDiagnostics;
import org.chromium.android_webview.StartupMetrics;
import org.chromium.android_webview.StartupTasksRunner;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwResource;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.metrics.TrackExitReasons;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ApkInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ResourceBundle;

import java.util.ArrayDeque;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
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

    @GuardedBy("mLazyInitLock")
    private CookieManagerAdapter mDefaultCookieManager;

    @GuardedBy("mLazyInitLock")
    private WebIconDatabaseAdapter mWebIconDatabase;

    @GuardedBy("mLazyInitLock")
    private WebViewDatabaseAdapter mDefaultWebViewDatabase;

    private final ProfileStore mProfileStore = new ProfileStore(this);

    // Volatile to guard for incorrectly trying to use this without calling `startChromium`.
    // TODO(crbug.com/389871700): Consider hiding the variable where it can't be incorrectly
    // accessed. See crrev.com/c/6081452/comment/9dff4e5e_c049d778/ for context.
    private volatile ChromiumStartedGlobals mChromiumStartedGlobals;

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

    private final Object mThreadSettingLock = new Object();

    @GuardedBy("mThreadSettingLock")
    private boolean mThreadIsSet;

    private final CountDownLatch mStartupFinished = new CountDownLatch(1);

    private final CountDownLatch mNonUiThreadCapableStartupTasksLatch = new CountDownLatch(1);

    // mInitState should only transition from INIT_NOT_STARTED to INIT_FINISHED with possibly
    // INIT_POSTED as an intermediate state. INIT_POSTED is set right before posting `startChromium`
    // on the UI thread in case of async startup.
    private static final int INIT_NOT_STARTED = 0;
    private static final int INIT_POSTED = 1;
    private static final int INIT_FINISHED = 2;

    private final AtomicInteger mInitState = new AtomicInteger(INIT_NOT_STARTED);
    private final WebViewChromiumFactoryProvider mFactory;
    private final StartupDiagnostics mStartupDiagnostics = new StartupDiagnostics();
    private final WebViewChromiumRunQueue mWebViewStartUpCallbackRunQueue =
            new WebViewChromiumRunQueue();

    private final AwBrowserProcess.StartupDelegate mStartupDelegate =
            new AwBrowserProcess.StartupDelegate() {
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
            };

    private final AtomicInteger mChromiumFirstStartupRequestMode =
            new AtomicInteger(StartupTasksRunner.StartupRequestMode.UNSET);
    // Only accessed from the UI thread
    private StartupTasksRunner mStartupTasksRunner;
    private RuntimeException mStartupException;
    private Error mStartupError;
    private boolean mRunStartupTasksAsync;

    private volatile boolean mShouldInitializeDefaultProfile = true;

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
    }

    private void startChromium(@StartupCallSite int callSite, boolean triggeredFromUIThread) {
        assert ThreadUtils.runningOnUiThread();

        if (mInitState.get() == INIT_FINISHED) {
            return;
        }

        if (mRunStartupTasksAsync) {
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

    void setProviderInitOnMainLooperLocation(Throwable t) {
        mStartupDiagnostics.setProviderInitOnMainLooperLocation(t);
    }

    // Called once during the WebViewChromiumFactoryProvider initialization
    void runStartupTasksAsync(boolean enabled) {
        assert mInitState.get() == INIT_NOT_STARTED;
        mRunStartupTasksAsync = enabled;
    }

    // These are startup tasks that can either run during provider init or during `startChromium`.
    // This is extracted out so that we can experiment with calling this in either of these
    // locations.
    public void runNonUiThreadCapableStartupTasks() {
        try {
            ResourceBundle.setAvailablePakLocales(AwLocaleConfig.getWebViewSupportedPakLocales());

            try (DualTraceEvent ignored2 =
                    DualTraceEvent.scoped("LibraryLoader.ensureInitialized")) {
                LibraryLoader.getInstance().ensureInitialized();
            }

            initPlatSupportLibrary();
            AwContentsStatics.setCheckClearTextPermitted(
                    ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                            >= Build.VERSION_CODES.O);
        } finally {
            mNonUiThreadCapableStartupTasksLatch.countDown();
        }
    }

    private void waitForNonUiThreadCapableStartupTasks() {
        try (DualTraceEvent e2 =
                DualTraceEvent.scoped(
                        "WebViewChromiumAwInit.waitForNonUiThreadCapableStartupTasks")) {
            mNonUiThreadCapableStartupTasksLatch.await();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    // Initializes a new StartupTaskRunner with a list of tasks to run for chromium startup.
    // Postcondition of calling `.run` on the returned StartupTasksRunner is that Chromium startup
    // is finished.
    // Note: You should abstract any logic that is not strictly dependent on glue layer code into
    // a static method in AwBrowserProcess so they can be unit-tested.
    private StartupTasksRunner initializeStartupTasksRunner() {
        ArrayDeque<Runnable> preBrowserProcessStartTasks = new ArrayDeque<>();
        ArrayDeque<Runnable> postBrowserProcessStartTasks = new ArrayDeque<>();

        preBrowserProcessStartTasks.addLast(this::preBrowserProcessStartTask);

        addBrowserProcessStartTasksToQueue(
                preBrowserProcessStartTasks, postBrowserProcessStartTasks);

        postBrowserProcessStartTasks.addLast(this::postBrowserProcessStartTask);

        // Initialize the decoupled StartupTasksRunner with a Delegate interface implementation.
        return new StartupTasksRunner(
                new StartupTasksRunner.Delegate() {
                    @Override
                    public void onStartupComplete(StartupDiagnostics diagnostics) {
                        recordStartupMetrics();
                    }

                    @Override
                    public void onStartupFailed(RuntimeException e) {
                        mStartupException = e;
                    }

                    @Override
                    public void onStartupFailed(Error e) {
                        mStartupError = e;
                    }

                    @Override
                    public boolean isStartupFinished() {
                        return mInitState.get() == INIT_FINISHED;
                    }
                },
                mStartupDiagnostics,
                preBrowserProcessStartTasks,
                postBrowserProcessStartTasks,
                mRunStartupTasksAsync,
                mChromiumFirstStartupRequestMode.get());
    }

    private void preBrowserProcessStartTask() {
        if (WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT)) {
            PostTask.postTask(
                    TaskTraits.USER_VISIBLE,
                    () -> {
                        PlatformServiceBridge.getInstance();
                    });
        }
        if (mRunStartupTasksAsync) {
            // Disable java-side PostTask scheduling. The native-side task runners
            // are also disabled in the native code. The unscheduled prenative tasks
            // are migrated to the native task runner. The native task runner is
            // enabled when we are done with startup.
            PostTask.disablePreNativeUiTasks(true);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            TrackExitReasons.startTrackingStartup();
        }

        if (WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT)) {
            waitForNonUiThreadCapableStartupTasks();
        } else {
            runNonUiThreadCapableStartupTasks();
        }
        mStartupDelegate.waitForJavaResourcesSetup();
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
            RecordHistogram.recordBooleanHistogram(ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, false);
        } catch (Resources.NotFoundException e) {
            RecordHistogram.recordBooleanHistogram(ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, true);
            mFactory.addWebViewAssetPath(ContextUtils.getApplicationContext());
        }

        AwBrowserProcess.configureChildProcessLauncher(
                mStartupDelegate.shouldForceNativeSandboxedServices());

        // finishVariationsInit() must precede native initialization so
        // the seed is available when AwFeatureListCreator::SetUpFieldTrials()
        // runs.
        AwBrowserProcess.finishVariationsInit();
    }

    private void postBrowserProcessStartTask() {
        AwBrowserProcess.initializeMetricsLogUploader();

        int targetSdkVersion =
                ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;
        RecordHistogram.recordSparseHistogram("Android.WebView.TargetSdkVersion", targetSdkVersion);

        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.initThreadUnsafeSingletons")) {
            mChromiumStartedGlobals = new ChromiumStartedGlobals();
        }

        if (ApkInfo.isDebugAndroidOrApp()) {
            getSharedStatics().setWebContentsDebuggingEnabledUnconditionally(true);
        }

        if ((Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                ? CompatChanges.isChangeEnabled(WebSettings.ENABLE_SIMPLIFIED_DARK_MODE)
                : targetSdkVersion >= Build.VERSION_CODES.TIRAMISU) {
            AwDarkMode.enableSimplifiedDarkMode();
        }

        AwBrowserProcess.maybeEnableSafeBrowsingFromGms();
        AwBrowserProcess.setupSupervisedUser();
        AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ true);

        AwBrowserProcess.postBackgroundTasks(
                mFactory.isSafeModeEnabled(), mFactory.getWebViewPrefs());

        AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate != null) {
            AwContentsStatics.setSelectionActionMenuClient(
                    delegate.getSelectionActionMenuClient(mFactory.getWebViewDelegate()));
        }

        AwCrashyClassUtils.maybeCrashIfEnabled();
        // Must happen right after Chromium initialization is complete.
        mInitState.set(INIT_FINISHED);
        mStartupFinished.countDown();

        // Initialize the default profile once Chromium initialization is fully complete,
        // ensuring it is available before executing pending post-init tasks.
        if (mShouldInitializeDefaultProfile) {
            try (DualTraceEvent e =
                    DualTraceEvent.scoped("WebViewChromiumAwInit.initializeDefaultProfile")) {
                mDefaultProfileHolder.initializeDefaultProfileOnUI();
            }
        }

        // This runs all the pending tasks queued for after Chromium init is
        // finished, so should run after `mInitState` is `INIT_FINISHED`.
        mFactory.getRunQueue().notifyChromiumStarted();
        if (mRunStartupTasksAsync) {
            // Re-enables the taskrunners
            PostTask.disablePreNativeUiTasks(false);
            AwBrowserProcess.onStartupComplete();
        }
    }

    private void addBrowserProcessStartTasksToQueue(
            ArrayDeque<Runnable> preBrowserProcessStartTasks,
            ArrayDeque<Runnable> postBrowserProcessStartTasks) {
        StartupCallback callback =
                new StartupCallback() {
                    @Override
                    public void onSuccess(
                            @Nullable BrowserStartupController.StartupMetrics metrics) {
                        mStartupTasksRunner.recordContentMetrics(metrics);
                        mStartupTasksRunner.finishAsyncRun();
                    }

                    @Override
                    public void onFailure() {
                        throw new ProcessInitException(LoaderErrors.NATIVE_STARTUP_FAILED);
                    }
                };
        if (mRunStartupTasksAsync) {
            preBrowserProcessStartTasks.addLast(
                    () -> {
                        AwBrowserProcess.runPreBrowserProcessStart();
                        if (mStartupTasksRunner.getRunState()
                                == StartupTasksRunner.StartupRequestMode.ASYNC) {
                            AwBrowserProcess.triggerAsyncBrowserProcess(callback);
                        }
                    });
            postBrowserProcessStartTasks.addLast(
                    () -> {
                        AwBrowserProcess.finishBrowserProcessStart();
                        runImmediateTaskAfterBrowserProcessInit();
                    });
        } else {
            preBrowserProcessStartTasks.addLast(
                    () -> {
                        // Starts browser process synchronously.
                        AwBrowserProcess.runPreBrowserProcessStart();
                        AwBrowserProcess.finishBrowserProcessStart();
                        if (mStartupTasksRunner.getRunState()
                                == StartupTasksRunner.StartupRequestMode.ASYNC) {
                            // Tell the StartupTaskRunner to continue with the
                            // postBrowserProcessStartQueue.
                            mStartupTasksRunner.finishAsyncRun();
                        }
                    });

            postBrowserProcessStartTasks.addLast(this::runImmediateTaskAfterBrowserProcessInit);
        }
    }

    // Run the next startup task following BrowserProcess init.
    private void runImmediateTaskAfterBrowserProcessInit() {
        // TODO(crbug.com/332706093): See if this can be moved before loading native.
        if (!WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_BACKGROUND_CLASS_PRELOADING)) {
            AwClassPreloader.preloadClasses();
        }

        AwBrowserProcess.doNetworkInitializations(ContextUtils.getApplicationContext());
    }

    private void recordStartupMetrics() {
        mWebViewStartUpCallbackRunQueue.notifyChromiumStarted();

        // Stop early trace event collection.
        // They have already been emitted if a trace session was started to capture startup.
        EarlyTraceEvent.reset();

         // Record histograms
        StartupMetrics.recordChromiumInitTimes(mStartupDiagnostics);

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
    }

    boolean isChromiumInitialized() {
        return mInitState.get() == INIT_FINISHED;
    }

    boolean isChromiumInitStarted() {
        return mInitState.get() != INIT_NOT_STARTED;
    }

    /**
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup is finished when this method returns.
     */
    void triggerAndWaitForChromiumStarted(@StartupCallSite int callSite) {
        if (triggerChromiumStartupAndReturnTrueIfStartupIsFinished(callSite, false)) {
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

        try (DualTraceEvent event =
                DualTraceEvent.scoped("WebViewChromiumAwInit.waitForUIThreadInit")) {
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
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup will be finished in the near future.
     */
    void postChromiumStartupIfNeeded(@StartupCallSite int callSite) {
        triggerChromiumStartupAndReturnTrueIfStartupIsFinished(callSite, true);
    }

    /**
     * Triggers Chromium startup.
     *
     * <p>If `alwaysPost` is true, startup is always posted to the UI thread.
     *
     * <p>If `alwaysPost` is false, startup is posted to UI thread if not called on the UI thread
     * and startup will be run synchronously if called on the UI thread.
     *
     * <p>If the UI thread is not set explicitly before calling this method, the main looper is
     * chosen as the UI thread.
     *
     * @return true if Chromium startup is finished, false if startup will be finished in the near
     *     future. If false, caller may choose to wait on the {@code mStartupFinished} latch, or
     *     {@link WebViewStartUpCallback}.
     */
    private boolean triggerChromiumStartupAndReturnTrueIfStartupIsFinished(
            @StartupCallSite int callSite, boolean alwaysPost) {
        if (mInitState.get() == INIT_FINISHED) { // Early-out for the common case.
            return true;
        }
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped(
                        "WebViewChromiumFactoryProvider."
                                + "triggerChromiumStartupAndReturnTrueIfStartupIsFinished")) {
            maybeSetChromiumUiThread(Looper.getMainLooper());
            boolean runSynchronously = !alwaysPost && ThreadUtils.runningOnUiThread();
            mChromiumFirstStartupRequestMode.compareAndSet(
                    StartupTasksRunner.StartupRequestMode.UNSET,
                    runSynchronously
                            ? StartupTasksRunner.StartupRequestMode.SYNC
                            : StartupTasksRunner.StartupRequestMode.ASYNC);
            if (runSynchronously) {
                mStartupDiagnostics.setSynchronousChromiumInitLocation(
                        new Throwable(
                                "Location where Chromium init was started synchronously on the UI"
                                        + " thread"));
                // If we are currently running on the UI thread then we must do init now. If there
                // was already a task posted to the UI thread from another thread to do it, it will
                // just no-op when it runs.
                startChromium(callSite, /* triggeredFromUIThread= */ true);
                return true;
            }
            if (mInitState.compareAndSet(INIT_NOT_STARTED, INIT_POSTED)) {
                if (callSite != StartupCallSite.ASYNC_WEBVIEW_STARTUP) {
                    mStartupDiagnostics.setAsynchronousChromiumInitLocation(
                            new Throwable(
                                    "Location where Chromium init was started asynchronously on a"
                                            + " non-UI thread"));
                }
                // If we're not running on the UI thread (because init was triggered by a
                // thread-safe
                // function), post init to the UI thread, since init is *not* thread-safe.
                AwThreadUtils.postToUiThreadLooper(
                        () -> startChromium(callSite, /* triggeredFromUIThread= */ false));
            }
            return false;
        }
    }

    void maybeSetChromiumUiThread(Looper looper) {
        synchronized (mThreadSettingLock) {
            if (mThreadIsSet) {
                return;
            }
            Looper mainLooper = Looper.getMainLooper();
            boolean isUiThreadMainLooper = mainLooper.equals(looper);
            Log.v(
                    TAG,
                    "Binding Chromium to "
                            + (isUiThreadMainLooper ? "main" : "background")
                            + " looper "
                            + looper);
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.Startup.IsUiThreadMainLooper", isUiThreadMainLooper);
            ThreadUtils.setUiThread(looper);
            mThreadIsSet = true;
        }
    }

    private void initPlatSupportLibrary() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("WebViewChromiumAwInit.initPlatSupportLibrary")) {
            AwDrawFnImpl.setDrawFnFunctionTable(mStartupDelegate.getDrawFnFunctionTable());
            AwContents.setAwDrawSWFunctionTable(mStartupDelegate.getDrawSWFunctionTable());
        }
    }

    public SharedStatics getSharedStatics() {
        return mFactory.getSharedStatics();
    }

    boolean isMultiProcessEnabled() {
        return mFactory.isMultiProcessEnabled();
    }

    public AwTracingController getAwTracingController() {
        triggerAndWaitForChromiumStarted(StartupCallSite.GET_AW_TRACING_CONTROLLER);
        return mChromiumStartedGlobals.mAwTracingController;
    }

    public AwProxyController getAwProxyController() {
        triggerAndWaitForChromiumStarted(StartupCallSite.GET_AW_PROXY_CONTROLLER);
        return mChromiumStartedGlobals.mAwProxyController;
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
            callback.onSuccess(mStartupDiagnostics);
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
                    callback.onSuccess(mStartupDiagnostics);
                });
        postChromiumStartupIfNeeded(StartupCallSite.ASYNC_WEBVIEW_STARTUP);
    }

    // These are objects that need to be created on the UI thread and after chromium has started.
    // Thus created during startChromium for ease.
    private static final class ChromiumStartedGlobals {
        final AwTracingController mAwTracingController;
        final AwProxyController mAwProxyController;

        ChromiumStartedGlobals() {
            mAwProxyController = new AwProxyController();
            mAwTracingController = new AwTracingController();
        }
    }

    public Profile getDefaultProfile(@StartupCallSite int callSite) {
        return mDefaultProfileHolder.getDefaultProfile(callSite);
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
