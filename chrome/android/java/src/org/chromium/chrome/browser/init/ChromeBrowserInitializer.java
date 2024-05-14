// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.os.StrictMode;

import org.chromium.base.CommandLine;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryPrefetcher;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.task.ChainedTasks;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.SigninCheckerProvider;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.ArrayList;
import java.util.List;

/**
 * Application level delegate that handles start up tasks. {@link AsyncInitializationActivity}
 * classes should override the {@link BrowserParts} interface for any additional initialization
 * tasks for the initialization to work as intended.
 */
public class ChromeBrowserInitializer {
    private static final String TAG = "BrowserInitializer";
    private static ChromeBrowserInitializer sChromeBrowserInitializer =
            new ChromeBrowserInitializer();
    private static BrowserStartupController sBrowserStartupController;
    private List<Runnable> mTasksToRunWithFullBrowser;

    private boolean mPostInflationStartupComplete;
    private boolean mFullBrowserInitializationComplete;

    /**
     * This class is an application specific object that orchestrates the app initialization.
     *
     * @return The singleton instance of {@link ChromeBrowserInitializer}.
     */
    public static ChromeBrowserInitializer getInstance() {
        return sChromeBrowserInitializer;
    }

    /**
     * @return whether native (full browser) initialization is complete.
     */
    public boolean isFullBrowserInitialized() {
        return mFullBrowserInitializationComplete;
    }

    /**
     * Either runs a task now, or queue it until full browser initialization is done.
     * <p>
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

    /** Initializes the Chrome browser process synchronously. */
    public void handleSynchronousStartup() {
        handleSynchronousStartupInternal(false);
    }

    /** Initializes the Chrome browser process synchronously with GPU process warmup. */
    public void handleSynchronousStartupWithGpuWarmUp() {
        handleSynchronousStartupInternal(true);
    }

    private void handleSynchronousStartupInternal(final boolean startGpuProcess) {
        ThreadUtils.checkUiThread();

        BrowserParts parts =
                new EmptyBrowserParts() {
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
        ProcessInitializationHandler.getInstance().initializePreNativeLibraryLoad();
        try (TraceEvent e = TraceEvent.scoped("ChromeBrowserInitializer.preInflationStartup")) {
            parts.preInflationStartup();
        }
        if (parts.isActivityFinishingOrDestroyed()) return;
        preInflationStartupDone();
        parts.setContentViewAndLoadLibrary(() -> this.onInflationComplete(parts));
    }

    public boolean isPostInflationStartupComplete() {
        return mPostInflationStartupComplete;
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
     * Execute startup tasks that require native libraries to be loaded. See {@link BrowserParts}
     * for a list of calls to be implemented.
     *
     * @param isAsync Whether this call should synchronously wait for the browser process to be
     *     fully initialized before returning to the caller.
     * @param delegate The delegate for the {@link ChromeBrowserInitializer} to communicate
     *     initialization tasks.
     */
    public void handlePostNativeStartup(final boolean isAsync, final BrowserParts delegate) {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread";
        if (!mPostInflationStartupComplete) {
            throw new IllegalStateException(
                    "ChromeBrowserInitializer.handlePostNativeStartup called before "
                            + "ChromeBrowserInitializer.postInflationStartup has been run.");
        }
        final ChainedTasks tasks = new ChainedTasks();
        ProcessInitializationHandler.getInstance()
                .enqueuePostNativeTasksToRunBeforeActivityNativeInit(
                        tasks, delegate.startMinimalBrowser());

        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Run as early as possible. It should also be in a separate task (and after)
                    // initNetworkChangeNotifier, as this posts a task to the UI thread that would
                    // interfere with preconneciton otherwise. By preconnecting afterwards, we make
                    // sure that this task has run.
                    delegate.maybePreconnect();
                });

        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (delegate.isActivityFinishingOrDestroyed()) return;
                    delegate.initializeCompositor();
                });

        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (delegate.isActivityFinishingOrDestroyed()) return;
                    delegate.initializeState();
                });

        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (delegate.isActivityFinishingOrDestroyed()) return;
                    // Some tasks posted by this are on the critical path.
                    delegate.startNativeInitialization();
                });

        ProcessInitializationHandler.getInstance()
                .enqueuePostNativeTasksToRunAfterActivityNativeInit(tasks);

        if (!delegate.startMinimalBrowser()) {
            tasks.add(TaskTraits.UI_DEFAULT, this::onFinishFullBrowserInitialization);
        }

        int startupMode =
                getBrowserStartupController().getStartupMode(delegate.startMinimalBrowser());
        tasks.add(
                TaskTraits.UI_DEFAULT,
                () -> {
                    BackgroundTaskSchedulerFactory.getUmaReporter().reportStartupMode(startupMode);
                });

        if (isAsync) {
            // We want to start this queue once the C++ startup tasks have run; allow the
            // C++ startup to run asynchonously, and set it up to start the Java queue once
            // it has finished.
            startChromeBrowserProcessesAsync(
                    delegate.shouldStartGpuProcess(),
                    delegate.startMinimalBrowser(),
                    new BrowserStartupController.StartupCallback() {
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

    private void startChromeBrowserProcessesAsync(
            boolean startGpuProcess,
            boolean startMinimalBrowser,
            BrowserStartupController.StartupCallback callback) {
        try {
            TraceEvent.begin("ChromeBrowserInitializer.startChromeBrowserProcessesAsync");
            getBrowserStartupController()
                    .startBrowserProcessesAsync(
                            LibraryProcessType.PROCESS_BROWSER,
                            startGpuProcess,
                            startMinimalBrowser,
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
            getBrowserStartupController()
                    .startBrowserProcessesSync(
                            LibraryProcessType.PROCESS_BROWSER,
                            /* singleProcess= */ false,
                            /* startGpuProcess= */ startGpuProcess);
            SigninCheckerProvider.get(ProfileManager.getLastUsedRegularProfile());
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

    private void onFinishFullBrowserInitialization() {
        mFullBrowserInitializationComplete = true;

        if (mTasksToRunWithFullBrowser != null) {
            for (Runnable r : mTasksToRunWithFullBrowser) r.run();
            mTasksToRunWithFullBrowser = null;
        }
    }

    /**
     * For unit testing of clients.
     *
     * @param initializer The (placeholder or mocked) initializer to use.
     */
    public static void setForTesting(ChromeBrowserInitializer initializer) {
        var oldValue = sChromeBrowserInitializer;
        sChromeBrowserInitializer = initializer;
        ResettersForTesting.register(() -> sChromeBrowserInitializer = oldValue);
    }

    /**
     * Set {@link BrowserStartupController ) to use for unit testing.
     * @param controller The (placeholder or mocked) {@link BrowserStartupController) instance.
     */
    public static void setBrowserStartupControllerForTesting(BrowserStartupController controller) {
        var oldValue = sBrowserStartupController;
        sBrowserStartupController = controller;
        ResettersForTesting.register(() -> sBrowserStartupController = oldValue);
    }
}
