// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.os.Process;
import android.os.StrictMode;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.CommandLine;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryPrefetcher;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.memory.MemoryPressureUma;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.ChainedTasks;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.NativeLibraries;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeStrictMode;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.browser.crash.LogcatExtractionRunnable;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.crash.browser.ChildProcessCrashObserver;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.module_installer.util.ModuleUtil;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.content_public.browser.SpeechRecognition;
import org.chromium.net.NetworkChangeNotifier;

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
    private List<Runnable> mTasksToRunWithFullBrowser;

    private boolean mPreInflationStartupComplete;
    private boolean mPostInflationStartupComplete;
    private boolean mNativeInitializationComplete;
    private boolean mFullBrowserInitializationComplete;
    private boolean mNetworkChangeNotifierInitializationComplete;

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
     * @return whether native (full browser) initialization is complete.
     */
    public boolean isFullBrowserInitialized() {
        return mFullBrowserInitializationComplete;
    }

    /**
     * @deprecated use isFullBrowserInitialized() instead, the name hasNativeInitializationCompleted
     * is not accurate.
     */
    @Deprecated
    public boolean hasNativeInitializationCompleted() {
        return isFullBrowserInitialized();
    }

    /**
     * Either runs a task now, or queue it until native (full browser) initialization is done.
     *
     * All Runnables added this way will run in a single UI thread task.
     *
     * @param task The task to run.
     */
    public void runNowOrAfterFullBrowserStarted(Runnable task) {
        if (isFullBrowserInitialized()) {
            task.run();
        } else {
            if (mTasksToRunWithFullBrowser == null) {
                mTasksToRunWithFullBrowser = new ArrayList<Runnable>();
            }
            mTasksToRunWithFullBrowser.add(task);
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
        handlePreNativeStartupAndLoadLibraries(parts);
        handlePostNativeStartup(false, parts);
    }

    /**
     * Executes startup tasks that can be done without native libraries, then loads the libraries.
     * See {@link BrowserParts} for a list of calls to be implemented.
     * @param parts The delegate for the {@link ChromeBrowserInitializer} to communicate
     *              initialization tasks.
     */
    public void handlePreNativeStartupAndLoadLibraries(final BrowserParts parts) {
        ThreadUtils.checkUiThread();
        if (parts.isActivityFinishingOrDestroyed()) return;
        ProcessInitializationHandler.getInstance().initializePreNative();
        try (TraceEvent e = TraceEvent.scoped("ChromeBrowserInitializer.preInflationStartup")) {
            preInflationStartup();
            parts.preInflationStartup();
        }
        if (parts.isActivityFinishingOrDestroyed()) return;
        preInflationStartupDone();
        // This should be called before calling into LibraryLoader.
        if (Process.is64Bit() && !canBeLoadedIn64Bit()) {
            throw new RuntimeException(
                    "Starting in 64-bit mode requires the 64-bit native library. If the "
                    + "device is 64-bit only, see alternatives here: "
                    + "https://crbug.com/1303857#c7.");
        }
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
        mPostInflationStartupComplete = true;
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
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> { DownloadManagerService.warmUpSharedPrefs(); });
    }

    private void preInflationStartup() {
        ThreadUtils.assertOnUiThread();
        if (mPreInflationStartupComplete) return;

        new Thread(SafeBrowsingApiBridge::ensureInitialized).start();

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
        if (!delegate.startMinimalBrowser()
                && !ProcessInitializationHandler.getInstance().postNativeInitializationComplete()) {
            tasks.add(TaskTraits.UI_DEFAULT,
                    () -> ProcessInitializationHandler.getInstance().initializePostNative());
        }

        if (!mNetworkChangeNotifierInitializationComplete) {
            tasks.add(TaskTraits.UI_DEFAULT, this::initNetworkChangeNotifier);
        }

        tasks.add(TaskTraits.UI_DEFAULT, () -> {
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

        tasks.add(TaskTraits.UI_DEFAULT, () -> {
            if (delegate.isActivityFinishingOrDestroyed()) return;
            delegate.initializeCompositor();
        });

        tasks.add(TaskTraits.UI_DEFAULT, () -> {
            if (delegate.isActivityFinishingOrDestroyed()) return;
            delegate.initializeState();
        });

        tasks.add(TaskTraits.UI_DEFAULT, () -> {
            if (delegate.isActivityFinishingOrDestroyed()) return;
            // Some tasks posted by this are on the critical path.
            delegate.startNativeInitialization();
        });

        if (!mNativeInitializationComplete) {
            tasks.add(TaskTraits.UI_DEFAULT, this::onFinishNativeInitialization);
        }

        if (!delegate.startMinimalBrowser()) {
            tasks.add(TaskTraits.UI_DEFAULT, this::onFinishFullBrowserInitialization);
        }

        int startupMode =
                getBrowserStartupController().getStartupMode(delegate.startMinimalBrowser());
        tasks.add(TaskTraits.UI_DEFAULT, () -> {
            BackgroundTaskSchedulerFactory.getUmaReporter().reportStartupMode(startupMode);
        });

        if (isAsync) {
            // We want to start this queue once the C++ startup tasks have run; allow the
            // C++ startup to run asynchonously, and set it up to start the Java queue once
            // it has finished.
            startChromeBrowserProcessesAsync(delegate.shouldStartGpuProcess(),
                    delegate.startMinimalBrowser(), new BrowserStartupController.StartupCallback() {
                        @Override
                        public void onFailure() {
                            delegate.onStartupFailure(null);
                        }

                        @Override
                        public void onSuccess() {
                            tasks.start(false);
                        }
                    });
        } else {
            startChromeBrowserProcessesSync(delegate.shouldStartGpuProcess());
            tasks.start(true);
        }
    }

    public static boolean canBeLoadedIn64Bit() {
        // Fail here before loading libmonochrome.so on 64-bit platforms, otherwise the failing
        // native stacktrace will not make it obvious that this is a bitness issue. See this bug
        // for context: https://crbug.com/1303857 While non-component builds has only one library,
        // monochrome may not be the first in the list for component builds.

        for (String libraryName : NativeLibraries.LIBRARIES) {
            if (libraryName.equals("monochrome") || libraryName.equals("monochrome.cr")) {
                return false;
            }
        }
        return true;
    }

    private void startChromeBrowserProcessesAsync(boolean startGpuProcess,
            boolean startMinimalBrowser, BrowserStartupController.StartupCallback callback) {
        try {
            TraceEvent.begin("ChromeBrowserInitializer.startChromeBrowserProcessesAsync");
            getBrowserStartupController().startBrowserProcessesAsync(
                    LibraryProcessType.PROCESS_BROWSER, startGpuProcess, startMinimalBrowser,
                    callback);
        } finally {
            TraceEvent.end("ChromeBrowserInitializer.startChromeBrowserProcessesAsync");
        }
    }

    private void startChromeBrowserProcessesSync(boolean startGpuProcess) {
        try {
            TraceEvent.begin("ChromeBrowserInitializer.startChromeBrowserProcessesSync");
            ThreadUtils.assertOnUiThread();
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            LibraryLoader.getInstance().ensureInitialized();
            StrictMode.setThreadPolicy(oldPolicy);
            LibraryPrefetcher.asyncPrefetchLibrariesToMemory();
            getBrowserStartupController().startBrowserProcessesSync(
                    LibraryProcessType.PROCESS_BROWSER, /*singleProcess=*/false,
                    /*startGpuProcess=*/startGpuProcess);
            SigninCheckerProvider.get();
        } finally {
            TraceEvent.end("ChromeBrowserInitializer.startChromeBrowserProcessesSync");
        }
    }

    private BrowserStartupController getBrowserStartupController() {
        if (sBrowserStartupController == null) {
            sBrowserStartupController = BrowserStartupController.getInstance();
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

    private void onFinishFullBrowserInitialization() {
        mFullBrowserInitializationComplete = true;

        if (mTasksToRunWithFullBrowser != null) {
            for (Runnable r : mTasksToRunWithFullBrowser) r.run();
            mTasksToRunWithFullBrowser = null;
        }
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
        UmaUtils.recordBackgroundRestrictions();

        // Needed for field trial metrics to be properly collected in minimal browser mode.
        ChromeCachedFlags.getInstance().cacheMinimalBrowserFlags();

        ModuleUtil.recordStartupTime();

        ChromeStartupDelegate startupDelegate = AppHooks.get().createChromeStartupDelegate();
        startupDelegate.init();
    }

    private ActivityStateListener createActivityStateListener() {
        return new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (newState == ActivityState.CREATED || newState == ActivityState.DESTROYED) {
                    // When the app locale is overridden a change in system locale will not effect
                    // Chrome's UI language. There is race condition where the initial locale may
                    // not equal the overridden default locale (https://crbug.com/1224756).
                    if (GlobalAppLocaleController.getInstance().isOverridden()) return;
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
