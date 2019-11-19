// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.os.StrictMode;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.CommandLine;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryPrefetcher;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.memory.MemoryPressureUma;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeLocalizationUtils;
import org.chromium.chrome.browser.ChromeStrictMode;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.chrome.browser.crash.LogcatExtractionRunnable;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.services.GoogleServicesManager;
import org.chromium.chrome.browser.webapps.ActivityAssigner;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerExternalUma;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerPrefs;
import org.chromium.components.crash.browser.ChildProcessCrashObserver;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.module_installer.util.ModuleUtil;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.content_public.browser.SpeechRecognition;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.policy.CombinedPolicyProvider;
import org.chromium.ui.resources.ResourceExtractor;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Application level delegate that handles start up tasks.
 * {@link AsyncInitializationActivity} classes should override the {@link BrowserParts}
 * interface for any additional initialization tasks for the initialization to work as intended.
 */
public class ChromeBrowserInitializer {
    private static final String TAG = "BrowserInitializer";
    private static ChromeBrowserInitializer sChromeBrowserInitializer;
    private static BrowserStartupController sBrowserStartupController;
    private final Locale mInitialLocale = Locale.getDefault();
    private List<Runnable> mTasksToRunWithNative;

    private boolean mPreInflationStartupComplete;
    private boolean mPostInflationStartupComplete;
    private boolean mNativeInitializationComplete;
    private boolean mNetworkChangeNotifierInitializationComplete;

    /**
     * A callback to be executed when there is a new version available in Play Store.
     */
    public interface OnNewVersionAvailableCallback extends Runnable {
        /**
         * Set the update url to get the new version available.
         * @param updateUrl The url to be used.
         */
        void setUpdateUrl(String updateUrl);
    }

    /**
     * This class is an application specific object that orchestrates the app initialization.
     * @return The singleton instance of {@link ChromeBrowserInitializer}.
     */
    public static ChromeBrowserInitializer getInstance() {
        if (sChromeBrowserInitializer == null) {
            sChromeBrowserInitializer = new ChromeBrowserInitializer();
        }
        return sChromeBrowserInitializer;
    }

    /**
     * This class is an application specific object that orchestrates the app initialization.
     * @deprecated Use getInstance with no arguments instead.
     * @param context The context to get the application context from.
     * @return The singleton instance of {@link ChromeBrowserInitializer}.
     */
    public static ChromeBrowserInitializer getInstance(Context context) {
        return getInstance();
    }

    /**
     * @return whether native initialization is complete.
     */
    public boolean hasNativeInitializationCompleted() {
        return mNativeInitializationComplete;
    }

    /**
     * Either runs a task now, or queue it until native initialization is done.
     *
     * All Runnables added this way will run in a single UI thread task.
     *
     * @param task The task to run.
     */
    public void runNowOrAfterNativeInitialization(Runnable task) {
        if (hasNativeInitializationCompleted()) {
            task.run();
        } else {
            if (mTasksToRunWithNative == null) {
                mTasksToRunWithNative = new ArrayList<Runnable>();
            }
            mTasksToRunWithNative.add(task);
        }
    }

    /**
     * Initializes the Chrome browser process synchronously.
     */
    public void handleSynchronousStartup() {
        handleSynchronousStartupInternal(false);
    }

    /**
     * Initializes the Chrome browser process synchronously with GPU process warmup.
     */
    public void handleSynchronousStartupWithGpuWarmUp() {
        handleSynchronousStartupInternal(true);
    }

    private void handleSynchronousStartupInternal(final boolean startGpuProcess) {
        ThreadUtils.checkUiThread();

        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public boolean shouldStartGpuProcess() {
                return startGpuProcess;
            }
        };
        handlePreNativeStartup(parts);
        handlePostNativeStartup(false, parts);
    }

    /**
     * Execute startup tasks that can be done without native libraries. See {@link BrowserParts} for
     * a list of calls to be implemented.
     * @param parts The delegate for the {@link ChromeBrowserInitializer} to communicate
     *              initialization tasks.
     */
    public void handlePreNativeStartup(final BrowserParts parts) {
        ThreadUtils.checkUiThread();
        ProcessInitializationHandler.getInstance().initializePreNative();
        try (TraceEvent e = TraceEvent.scoped("ChromeBrowserInitializer.preInflationStartup")) {
            preInflationStartup();
            parts.preInflationStartup();
        }
        if (parts.isActivityFinishingOrDestroyed()) return;
        preInflationStartupDone();
        parts.setContentViewAndLoadLibrary(() -> this.onInflationComplete(parts));
    }

    /**
     * This is called after the layout inflation has been completed (in the callback sent to {@link
     * BrowserParts#setContentViewAndLoadLibrary}). This continues the post-inflation pre-native
     * startup tasks. Namely {@link BrowserParts#postInflationStartup()}.
     * @param parts The {@link BrowserParts} that has finished layout inflation
     */
    private void onInflationComplete(final BrowserParts parts) {
        if (parts.isActivityFinishingOrDestroyed()) return;
        postInflationStartup();
        parts.postInflationStartup();
    }

    /**
     * This is needed for device class manager which depends on commandline args that are
     * initialized in preInflationStartup()
     */
    private void preInflationStartupDone() {
        // Domain reliability uses significant enough memory that we should disable it on low memory
        // devices for now.
        // TODO(zbowling): remove this after domain reliability is refactored. (crbug.com/495342)
        if (SysUtils.isLowEndDevice()) {
            CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_DOMAIN_RELIABILITY);
        }
    }

    /**
     * Pre-load shared prefs to avoid being blocked on the disk access async task in the future.
     * Running in an AsyncTask as pre-loading itself may cause I/O.
     */
    private void warmUpSharedPrefs() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
                ActivityAssigner.warmUpSharedPrefs();
                DownloadManagerService.warmUpSharedPrefs();
                BackgroundTaskSchedulerPrefs.warmUpSharedPrefs();
            });
        } else {
            ActivityAssigner.warmUpSharedPrefs();
            DownloadManagerService.warmUpSharedPrefs();
            BackgroundTaskSchedulerPrefs.warmUpSharedPrefs();
        }
    }

    private void preInflationStartup() {
        ThreadUtils.assertOnUiThread();
        if (mPreInflationStartupComplete) return;

        // Ensure critical files are available, so they aren't blocked on the file-system
        // behind long-running accesses in next phase.
        // Don't do any large file access here!
        ChromeStrictMode.configureStrictMode();
        ChromeWebApkHost.init();

        // Time this call takes in background from test devices:
        // - Pixel 2: ~10 ms
        // - Nokia 1 (Android Go): 20-200 ms
        warmUpSharedPrefs();

        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        ApplicationStatus.registerStateListenerForAllActivities(createActivityStateListener());

        mPreInflationStartupComplete = true;
    }

    private void postInflationStartup() {
        ThreadUtils.assertOnUiThread();
        if (mPostInflationStartupComplete) return;

        // Check to see if we need to extract any new resources from the APK. This could
        // be on first run when we need to extract all the .pak files we need, or after
        // the user has switched locale, in which case we want new locale resources.
        ResourceExtractor.get().setResultTraits(UiThreadTaskTraits.BOOTSTRAP);
        ResourceExtractor.get().startExtractingResources(LocaleUtils.toLanguage(
                ChromeLocalizationUtils.getUiLocaleStringForCompressedPak()));

        mPostInflationStartupComplete = true;
    }

    /**
     * Execute startup tasks that require native libraries to be loaded. See {@link BrowserParts}
     * for a list of calls to be implemented.
     * @param isAsync Whether this call should synchronously wait for the browser process to be
     *                fully initialized before returning to the caller.
     * @param delegate The delegate for the {@link ChromeBrowserInitializer} to communicate
     *                 initialization tasks.
     */
    public void handlePostNativeStartup(final boolean isAsync, final BrowserParts delegate) {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread";
        if (!mPostInflationStartupComplete) {
            throw new IllegalStateException(
                    "ChromeBrowserInitializer.handlePostNativeStartup called before "
                    + "ChromeBrowserInitializer.postInflationStartup has been run.");
        }
        final ChainedTasks tasks = new ChainedTasks();
        // If full browser process is not going to be launched, it is up to individual service to
        // launch its required components.
        if (!delegate.startServiceManagerOnly()
                && !ProcessInitializationHandler.getInstance().postNativeInitializationComplete()) {
            tasks.add(UiThreadTaskTraits.BOOTSTRAP,
                    () -> ProcessInitializationHandler.getInstance().initializePostNative());
        }

        if (!mNetworkChangeNotifierInitializationComplete) {
            tasks.add(UiThreadTaskTraits.BOOTSTRAP, this::initNetworkChangeNotifier);
        }

        tasks.add(UiThreadTaskTraits.BOOTSTRAP, () -> {
            // This is not broken down as a separate task, since this:
            // 1. Should happen as early as possible
            // 2. Only submits asynchronous work
            // 3. Is thus very cheap (profiled at 0.18ms on a Nexus 5 with Lollipop)
            // It should also be in a separate task (and after) initNetworkChangeNotifier, as
            // this posts a task to the UI thread that would interfere with preconneciton
            // otherwise. By preconnecting afterwards, we make sure that this task has run.
            delegate.maybePreconnect();

            onStartNativeInitialization();
        });

        tasks.add(UiThreadTaskTraits.BOOTSTRAP, () -> {
            if (delegate.isActivityFinishingOrDestroyed()) return;
            delegate.initializeCompositor();
        });

        tasks.add(UiThreadTaskTraits.BOOTSTRAP, () -> {
            if (delegate.isActivityFinishingOrDestroyed()) return;
            delegate.initializeState();
        });

        tasks.add(UiThreadTaskTraits.BOOTSTRAP, () -> {
            if (delegate.isActivityFinishingOrDestroyed()) return;
            // Some tasks posted by this are on the critical path.
            delegate.startNativeInitialization();
        });

        if (!mNativeInitializationComplete) {
            tasks.add(UiThreadTaskTraits.DEFAULT, this::onFinishNativeInitialization);
        }

        int startupMode =
                getBrowserStartupController().getStartupMode(delegate.startServiceManagerOnly());
        tasks.add(UiThreadTaskTraits.DEFAULT, () -> {
            BackgroundTaskSchedulerExternalUma.getInstance().reportStartupMode(startupMode);
        });

        if (isAsync) {
            // We want to start this queue once the C++ startup tasks have run; allow the
            // C++ startup to run asynchonously, and set it up to start the Java queue once
            // it has finished.
            startChromeBrowserProcessesAsync(delegate.shouldStartGpuProcess(),
                    delegate.startServiceManagerOnly(),
                    new BrowserStartupController.StartupCallback() {
                        @Override
                        public void onFailure() {
                            delegate.onStartupFailure();
                        }

                        @Override
                        public void onSuccess() {
                            tasks.start(false);
                        }
                    });
        } else {
            startChromeBrowserProcessesSync();
            tasks.start(true);
        }
    }

    private void startChromeBrowserProcessesAsync(boolean startGpuProcess,
            boolean startServiceManagerOnly, BrowserStartupController.StartupCallback callback) {
        try {
            TraceEvent.begin("ChromeBrowserInitializer.startChromeBrowserProcessesAsync");
            getBrowserStartupController().startBrowserProcessesAsync(
                    startGpuProcess, startServiceManagerOnly, callback);
        } finally {
            TraceEvent.end("ChromeBrowserInitializer.startChromeBrowserProcessesAsync");
        }
    }

    private void startChromeBrowserProcessesSync() {
        try {
            TraceEvent.begin("ChromeBrowserInitializer.startChromeBrowserProcessesSync");
            ThreadUtils.assertOnUiThread();
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
            StrictMode.setThreadPolicy(oldPolicy);
            LibraryPrefetcher.asyncPrefetchLibrariesToMemory();
            getBrowserStartupController().startBrowserProcessesSync(false);
            GoogleServicesManager.get();
        } finally {
            TraceEvent.end("ChromeBrowserInitializer.startChromeBrowserProcessesSync");
        }
    }

    private BrowserStartupController getBrowserStartupController() {
        if (sBrowserStartupController == null) {
            sBrowserStartupController =
                    BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER);
        }
        return sBrowserStartupController;
    }

    public void initNetworkChangeNotifier() {
        if (mNetworkChangeNotifierInitializationComplete) return;
        mNetworkChangeNotifierInitializationComplete = true;

        ThreadUtils.assertOnUiThread();
        TraceEvent.begin("NetworkChangeNotifier.init");
        // Enable auto-detection of network connectivity state changes.
        NetworkChangeNotifier.init();
        NetworkChangeNotifier.setAutoDetectConnectivityState(true);
        TraceEvent.end("NetworkChangeNotifier.init");
    }

    private void onStartNativeInitialization() {
        ThreadUtils.assertOnUiThread();
        if (mNativeInitializationComplete) return;
        // The policies are used by browser startup, so we need to register the policy providers
        // before starting the browser process.
        AppHooks.get().registerPolicyProviders(CombinedPolicyProvider.get());

        SpeechRecognition.initialize();
    }

    private void onFinishNativeInitialization() {
        if (mNativeInitializationComplete) return;

        mNativeInitializationComplete = true;
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());

        // When a child process crashes, search for the most recent minidump for the child's process
        // ID and attach a logcat to it. Then upload it to the crash server. Note that the logcat
        // extraction might fail. This is ok; in that case, the minidump will be found and uploaded
        // upon the next browser launch.
        ChildProcessCrashObserver.registerCrashCallback(
                new ChildProcessCrashObserver.ChildCrashedCallback() {
                    @Override
                    public void childCrashed(int pid) {
                        CrashFileManager crashFileManager = new CrashFileManager(
                                ContextUtils.getApplicationContext().getCacheDir());

                        File minidump = crashFileManager.getMinidumpSansLogcatForPid(pid);
                        if (minidump != null) {
                            AsyncTask.THREAD_POOL_EXECUTOR.execute(
                                    new LogcatExtractionRunnable(minidump));
                        } else {
                            Log.e(TAG, "Missing dump for child " + pid);
                        }
                    }
                });

        MemoryPressureUma.initializeForBrowser();
        if (mTasksToRunWithNative != null) {
            for (Runnable r : mTasksToRunWithNative) r.run();
            mTasksToRunWithNative = null;
        }

        // TODO(crbug.com/960767): Remove this in M77.
        ServiceManagerStartupUtils.cleanupSharedPreferences();

        // Needed for field trial metrics to be properly collected in ServiceManager only mode.
        FeatureUtilities.cacheNativeFlagsForServiceManagerOnlyMode();

        ModuleUtil.recordStartupTime();
    }

    private ActivityStateListener createActivityStateListener() {
        return new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (newState == ActivityState.CREATED || newState == ActivityState.DESTROYED) {
                    // Android destroys Activities at some point after a locale change, but doesn't
                    // kill the process.  This can lead to a bug where Chrome is halfway RTL, where
                    // stale natively-loaded resources are not reloaded (http://crbug.com/552618).
                    if (!mInitialLocale.equals(Locale.getDefault())) {
                        Log.e(TAG, "Killing process because of locale change.");
                        Process.killProcess(Process.myPid());
                    }
                }
            }
        };
    }

    /**
     * For unit testing of clients.
     * @param initializer The (dummy or mocked) initializer to use.
     */
    public static void setForTesting(ChromeBrowserInitializer initializer) {
        sChromeBrowserInitializer = initializer;
    }

    /**
     * Set {@link BrowserStartupController) to use for unit testing.
     * @param controller The (dummy or mocked) {@link BrowserStartupController) instance.
     */
    public static void setBrowserStartupControllerForTesting(BrowserStartupController controller) {
        sBrowserStartupController = controller;
    }
}
