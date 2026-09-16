// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.GuardedBy;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;

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

        /** Configures ChildProcessCreationParams and initializes ChildProcessLauncherHelper. */
        void configureChildProcessLauncher();

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

    private final CountDownLatch mStartupFinished = new CountDownLatch(1);
    private final AtomicInteger mInitState = new AtomicInteger(INIT_NOT_STARTED);

    private final Object mThreadSettingLock = new Object();

    @GuardedBy("mThreadSettingLock")
    private boolean mThreadIsSet;

    private final StartupDiagnostics mStartupDiagnostics = new StartupDiagnostics();
    private final AtomicInteger mChromiumFirstStartupRequestMode =
            new AtomicInteger(StartupTasksRunner.StartupRequestMode.UNSET);
    private final WebViewChromiumRunQueue mRunQueue = new WebViewChromiumRunQueue();
    private final WebViewChromiumRunQueue mStartupCallbackQueue = new WebViewChromiumRunQueue();

    private @Nullable RuntimeException mStartupException;
    private @Nullable Error mStartupError;

    private static @Nullable StartupController sInstance;

    public static StartupController getInstance() {
        if (sInstance == null) {
            throw new IllegalStateException("StartupController is not initialized");
        }
        return sInstance;
    }

    public static StartupController initialize(Delegate delegate) {
        if (sInstance != null) {
            throw new IllegalStateException("StartupController is already initialized");
        }
        sInstance = new StartupController(delegate);
        return sInstance;
    }

    private @Nullable StartupTasksRunner mStartupTasksRunner;

    public StartupController(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Records the stack trace where the WebView provider was initialized on the main looper.
     *
     * <p>Executed during {@code WebViewChromiumFactoryProvider} instantiation before Chromium
     * startup is triggered.
     */
    public void setProviderInitOnMainLooperLocation(Throwable t) {
        mStartupDiagnostics.setProviderInitOnMainLooperLocation(t);
    }

    /** Returns the post-startup task queue. */
    public WebViewChromiumRunQueue getRunQueue() {
        return mRunQueue;
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

    /**
     * Triggers Chromium startup synchronously or waits if startup is already running on the UI
     * thread.
     *
     * <p>If the UI thread is not set, the Android main looper will be set as the UI thread.
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
     * Posts Chromium startup to the UI thread if not already started.
     *
     * <p>If the UI thread is not set, the Android main looper will be set as the UI thread.
     *
     * <p>Postcondition: Chromium startup will be finished in the near future.
     */
    public void postChromiumStartupIfNeeded(@StartupCallSite int callSite) {
        triggerChromiumStartupAndReturnTrueIfStartupIsFinished(callSite, true);
    }

    /**
     * Core entry point for triggering Chromium startup.
     *
     * <p>If {@code alwaysPost} is true, startup is always posted to the UI thread.
     *
     * <p>If {@code alwaysPost} is false, startup is posted to the UI thread if called from a non-UI
     * thread, or run synchronously if called directly on the UI thread.
     *
     * <p>If the UI thread is not set explicitly before calling this method, the main looper is
     * chosen as the UI thread.
     *
     * @return true if Chromium startup is finished, false if startup will be finished in the near
     *     future.
     */
    private boolean triggerChromiumStartupAndReturnTrueIfStartupIsFinished(
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

    /**
     * Executes Chromium startup on the UI thread.
     *
     * <p>Initializes the {@link StartupTasksRunner} if not already created and runs the startup
     * sequence. Re-throws any previously encountered startup error or runtime exception.
     */
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

    /**
     * Creates and configures the {@link StartupTasksRunner} with the ordered pre-browser process
     * and post-browser process startup steps.
     */
    private StartupTasksRunner initializeStartupTasksRunner(
            @StartupTasksRunner.StartupRequestMode int chromiumFirstStartupRequestMode) {
        if (mStartupTasksRunner != null) {
            return mStartupTasksRunner;
        }
        ArrayDeque<Runnable> preBrowserProcessStartTasks = new ArrayDeque<>();
        ArrayDeque<Runnable> postBrowserProcessStartTasks = new ArrayDeque<>();

        preBrowserProcessStartTasks.addLast(
                () -> StartupTasks.preBrowserProcessStartStepOne(mDelegate));
        preBrowserProcessStartTasks.addLast(StartupTasks::preBrowserProcessStartStepTwo);
        postBrowserProcessStartTasks.addLast(StartupTasks::postBrowserProcessStartStepOne);
        postBrowserProcessStartTasks.addLast(
                () -> {
                    StartupTasks.postBrowserProcessStartStepTwo(mDelegate);
                    finishStartup();
                });

        mStartupTasksRunner =
                new StartupTasksRunner(
                        new StartupTasksRunner.Delegate() {
                            @Override
                            public void onStartupTimingsReady(
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
                                ThreadUtils.assertOnUiThread();
                                try (DualTraceEvent e2 =
                                        DualTraceEvent.scoped(
                                                "StartupController.doAsyncBrowserStartup")) {
                                    boolean singleProcess =
                                            !CommandLine.getInstance()
                                                    .hasSwitch(
                                                            AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
                                    BrowserStartupController.getInstance()
                                            .startBrowserProcessesAsync(
                                                    LibraryProcessType.PROCESS_WEBVIEW,
                                                    /* startGpuProcess= */ false,
                                                    /* startMinimalBrowser= */ false,
                                                    singleProcess,
                                                    callback);
                                }
                            }
                        },
                        preBrowserProcessStartTasks,
                        postBrowserProcessStartTasks,
                        chromiumFirstStartupRequestMode);
        return mStartupTasksRunner;
    }

    /** Transitions WebView startup state to finished and drains post-startup queues. */
    private void finishStartup() {
        ThreadUtils.assertOnUiThread();

        mInitState.set(INIT_FINISHED);
        mStartupFinished.countDown();

        mDelegate.onStartupComplete();
        mRunQueue.notifyChromiumStarted();

        PostTask.disablePreNativeUiTasks(false);
        AwBrowserProcess.onStartupComplete();
    }

    /** Returns whether Chromium startup has finished. */
    public boolean isChromiumInitialized() {
        return mInitState.get() == INIT_FINISHED;
    }

    /** Returns the startup diagnostics and timing information. */
    public StartupDiagnostics getStartupDiagnostics() {
        return mStartupDiagnostics;
    }
}
