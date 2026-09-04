// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.GuardedBy;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.metrics.TrackExitReasons;
import org.chromium.base.ApkInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.ui.base.ResourceBundle;

import java.util.ArrayDeque;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;

/** Controller responsible for managing WebView startup lifecycle and tasks. */
@NullMarked
public class StartupController {
    private static final String TAG = "StartupController";

    /** Delegate interface for callbacks needed during WebView global startup. */
    public interface Delegate {
        /** Wait until it's possible to access Android resources defined in the Chromium APK. */
        void waitForJavaResourcesSetup();

        /** Returns whether to use native sandboxed services. */
        boolean shouldForceNativeSandboxedServices();

        // TODO(abhijithnair): Rethink whether `getDrawFnFunctionTable` and `getDrawSWFunctionTable`
        // are the right interface. See
        // https://chromium-review.git.corp.google.com/c/chromium/src/+/8257352/comment/d9c4282e_3fa74a88/
        /** Returns the function table pointer for hardware-accelerated drawing. */
        long getDrawFnFunctionTable();

        /** Returns the function table pointer for software drawing. */
        long getDrawSWFunctionTable();

        // TODO: Inline SelectionActionMenuClient call once aconfig flag is cleaned up.
        /** Returns the framework-level selection action menu client, if available. */
        @Nullable SelectionActionMenuClientWrapper getSelectionActionMenuClient();

        /** Callback for the glue layer to complete post-startup tasks. */
        void onStartupComplete();

        /** Callback when startup diagnostics and timings are ready. */
        void onStartupDiagnosticsReady(StartupDiagnostics diagnostics);
    }

    private static final int INIT_NOT_STARTED = 0;
    private static final int INIT_POSTED = 1;
    private static final int INIT_FINISHED = 2;

    private final Delegate mDelegate;

    private final CountDownLatch mNonUiThreadCapableStartupTasksLatch = new CountDownLatch(1);
    private final CountDownLatch mStartupFinished = new CountDownLatch(1);
    private final AtomicInteger mInitState = new AtomicInteger(INIT_NOT_STARTED);

    private final Object mThreadSettingLock = new Object();

    @GuardedBy("mThreadSettingLock")
    private boolean mThreadIsSet;

    private final StartupDiagnostics mStartupDiagnostics = new StartupDiagnostics();
    private final AtomicInteger mChromiumFirstStartupRequestMode =
            new AtomicInteger(StartupTasksRunner.StartupRequestMode.UNSET);
    private final WebViewChromiumRunQueue mStartupCallbackQueue = new WebViewChromiumRunQueue();

    private @Nullable RuntimeException mStartupException;
    private @Nullable Error mStartupError;

    private @Nullable StartupTasksRunner mStartupTasksRunner;

    public StartupController(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Requests asynchronous Chromium startup and registers a callback to receive diagnostics when
     * startup is finished.
     */
    public void requestAsyncStartup(StartupDiagnostics.Callback callback) {
        mStartupCallbackQueue.addTask(() -> callback.onSuccess(getStartupDiagnostics()));
        postChromiumStartupIfNeeded(StartupCallSite.ASYNC_WEBVIEW_STARTUP);
    }

    public void maybeSetChromiumUiThread(Looper looper) {
        synchronized (mThreadSettingLock) {
            if (mThreadIsSet) {
                return;
            }
            Looper mainLooper = Looper.getMainLooper();
            boolean isUiThreadMainLooper = mainLooper.equals(looper);
            Log.v(
                    TAG,
                    "Binding Chromium to %s looper %s",
                    isUiThreadMainLooper ? "main" : "background",
                    looper);
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.Startup.IsUiThreadMainLooper", isUiThreadMainLooper);
            ThreadUtils.setUiThread(looper);
            mThreadIsSet = true;
        }
    }

    public boolean isChromiumInitialized() {
        return mInitState.get() == INIT_FINISHED;
    }

    public StartupDiagnostics getStartupDiagnostics() {
        return mStartupDiagnostics;
    }

    public void setProviderInitOnMainLooperLocation(Throwable t) {
        mStartupDiagnostics.setProviderInitOnMainLooperLocation(t);
    }

    /**
     * If UI thread is not set, Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup is finished when this method returns.
     */
    public void triggerAndWaitForChromiumStarted(@StartupCallSite int callSite) {
        if (triggerChromiumStartupAndReturnTrueIfStartupIsFinished(callSite, false)) {
            return;
        }

        try (DualTraceEvent event =
                DualTraceEvent.scoped("StartupController.waitForUIThreadInit")) {
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
    public void postChromiumStartupIfNeeded(@StartupCallSite int callSite) {
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
     *     future.
     */
    public boolean triggerChromiumStartupAndReturnTrueIfStartupIsFinished(
            @StartupCallSite int callSite, boolean alwaysPost) {
        if (mInitState.get() == INIT_FINISHED) { // Early-out for the common case.
            return true;
        }
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped(
                        "StartupController."
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

    private void startChromium(@StartupCallSite int callSite, boolean triggeredFromUIThread) {
        assert ThreadUtils.runningOnUiThread();

        if (mInitState.get() == INIT_FINISHED) {
            return;
        }

        if (mStartupException != null) {
            throw mStartupException;
        } else if (mStartupError != null) {
            throw mStartupError;
        }

        if (mStartupTasksRunner == null) {
            mStartupTasksRunner =
                    initializeStartupTasksRunner(mChromiumFirstStartupRequestMode.get());
        }
        mStartupTasksRunner.run(callSite, triggeredFromUIThread);
    }

    // These are startup tasks that can either run during provider init or during `startChromium`.
    // This is extracted out so that we can experiment with calling this in either of these
    // locations.
    public void runNonUiThreadCapableStartupTasks() {
        assert mDelegate != null;
        try {
            ResourceBundle.setAvailablePakLocales(AwLocaleConfig.getWebViewSupportedPakLocales());

            try (DualTraceEvent ignored2 =
                    DualTraceEvent.scoped("LibraryLoader.ensureInitialized")) {
                LibraryLoader.getInstance().ensureInitialized();
            }

            configureDrawingFunctions();
            AwContentsStatics.setCheckClearTextPermitted(
                    ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                            >= Build.VERSION_CODES.O);
        } finally {
            mNonUiThreadCapableStartupTasksLatch.countDown();
        }
    }

    private void configureDrawingFunctions() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("StartupController.configureDrawingFunctions")) {
            AwDrawFnImpl.setDrawFnFunctionTable(mDelegate.getDrawFnFunctionTable());
            AwContents.setAwDrawSWFunctionTable(mDelegate.getDrawSWFunctionTable());
        }
    }

    public void waitForNonUiThreadCapableStartupTasks() {
        try (DualTraceEvent e2 =
                DualTraceEvent.scoped(
                        "StartupController.waitForNonUiThreadCapableStartupTasks")) {
            mNonUiThreadCapableStartupTasksLatch.await();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    private StartupTasksRunner initializeStartupTasksRunner(
            @StartupTasksRunner.StartupRequestMode int chromiumFirstStartupRequestMode) {
        if (mStartupTasksRunner != null) {
            return mStartupTasksRunner;
        }
        ArrayDeque<Runnable> preBrowserProcessStartTasks = new ArrayDeque<>();
        ArrayDeque<Runnable> postBrowserProcessStartTasks = new ArrayDeque<>();

        preBrowserProcessStartTasks.addLast(this::preBrowserProcessStartTask);
        preBrowserProcessStartTasks.addLast(AwBrowserProcess::runPreBrowserProcessStart);
        postBrowserProcessStartTasks.addLast(this::immediatePostBrowserProcessStartTask);
        postBrowserProcessStartTasks.addLast(this::postBrowserProcessStartTask);

        mStartupTasksRunner =
                new StartupTasksRunner(
                        new StartupTasksRunner.Delegate() {
                            @Override
                            public void onStartupComplete(
                                    StartupTasksRunner.StartupTimings timings) {
                                mStartupDiagnostics.setStartupTimings(timings);
                                mStartupCallbackQueue.notifyChromiumStarted();
                                mDelegate.onStartupDiagnosticsReady(mStartupDiagnostics);
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

                            @Override
                            public void doAsyncBrowserStartup(StartupCallback callback) {
                                AwBrowserProcess.triggerAsyncBrowserProcess(callback);
                            }
                        },
                        preBrowserProcessStartTasks,
                        postBrowserProcessStartTasks,
                        chromiumFirstStartupRequestMode);
        return mStartupTasksRunner;
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
        // Disable java-side PostTask scheduling. The native-side task runners
        // are also disabled in the native code. The unscheduled prenative tasks
        // are migrated to the native task runner. The native task runner is
        // enabled when we are done with startup.
        PostTask.disablePreNativeUiTasks(true);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            TrackExitReasons.startTrackingStartup();
        }

        if (WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT)) {
            waitForNonUiThreadCapableStartupTasks();
        } else {
            runNonUiThreadCapableStartupTasks();
        }
        mDelegate.waitForJavaResourcesSetup();
        // NOTE: Finished writing Java resources. From this point on, it's safe
        // to use them.

        AwBrowserProcess.configureChildProcessLauncher(
                mDelegate.shouldForceNativeSandboxedServices());

        // finishVariationsInit() must precede native initialization so
        // the seed is available when AwFeatureListCreator::SetUpFieldTrials()
        // runs.
        AwBrowserProcess.finishVariationsInit();
    }

    /** Runs immediate post-browser startup tasks following BrowserProcess init. */
    private void immediatePostBrowserProcessStartTask() {
        AwBrowserProcess.finishBrowserProcessStart();
        // TODO(crbug.com/332706093): See if this can be moved before loading native.
        if (!WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_BACKGROUND_CLASS_PRELOADING)) {
            AwClassPreloader.preloadClasses();
        }

        AwBrowserProcess.doNetworkInitializations(ContextUtils.getApplicationContext());
    }

    /**
     * Runs post-browser-process startup tasks that need to run on the UI thread before and after
     * Chromium initialization is complete.
     */
    private void postBrowserProcessStartTask() {
        ThreadUtils.assertOnUiThread();

        AwBrowserProcess.initializeMetricsLogUploader();

        int targetSdkVersion =
                ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;
        RecordHistogram.recordSparseHistogram("Android.WebView.TargetSdkVersion", targetSdkVersion);

        if (ApkInfo.isDebugAndroidOrApp()) {
            AwDevToolsServer.setRemoteDebuggingEnabled(true);
        }

        if (CompatQuirks.isEnabled(CompatQuirks.Quirk.LEGACY_DARK_MODE)) {
            AwDarkMode.enableLegacyDarkMode();
        }

        AwBrowserProcess.maybeEnableSafeBrowsingFromGms();
        AwBrowserProcess.setupSupervisedUser();
        AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ true);
        AwBrowserProcess.startObservingOsAccessibilitySettingChanges();
        AwTracingController.getInstance();

        AwBrowserProcess.postBackgroundTasks();

        AwContentsStatics.setSelectionActionMenuClient(mDelegate.getSelectionActionMenuClient());

        AwCrashyClassUtils.maybeCrashIfEnabled();

        mInitState.set(INIT_FINISHED);
        mStartupFinished.countDown();

        mDelegate.onStartupComplete();

        PostTask.disablePreNativeUiTasks(false);
        AwBrowserProcess.onStartupComplete();
    }
}
