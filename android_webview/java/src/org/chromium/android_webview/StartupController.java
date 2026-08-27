// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Build;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.metrics.TrackExitReasons;
import org.chromium.base.ApkInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ResourceBundle;

import java.util.ArrayDeque;
import java.util.concurrent.CountDownLatch;

/** Controller responsible for managing WebView startup lifecycle and tasks. */
@NullMarked
public class StartupController {
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

        /** Initializes thread-unsafe singletons in the glue layer. */
        void initThreadUnsafeSingletons();

        // TODO: Inline SelectionActionMenuClient call once aconfig flag is cleaned up.
        /** Returns the framework-level selection action menu client, if available. */
        @Nullable SelectionActionMenuClientWrapper getSelectionActionMenuClient();

        /** Callback for the glue layer to complete post-startup tasks. */
        void onStartupComplete();
    }

    private final Delegate mDelegate;
    private final StartupTasksRunner.Delegate mStartupTasksRunnerDelegate;

    private final CountDownLatch mNonUiThreadCapableStartupTasksLatch = new CountDownLatch(1);
    private @Nullable StartupTasksRunner mStartupTasksRunner;

    public StartupController(
            Delegate delegate, StartupTasksRunner.Delegate startupTasksRunnerDelegate) {
        mDelegate = delegate;
        mStartupTasksRunnerDelegate = startupTasksRunnerDelegate;
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

    /**
     * Runs startup tasks synchronously or asynchronously depending on call site and thread.
     * Initializes StartupTasksRunner on the first call.
     */
    public void runStartupTasks(
            @StartupCallSite int callSite,
            boolean triggeredFromUIThread,
            @StartupTasksRunner.StartupRequestMode int chromiumFirstStartupRequestMode) {
        StartupTasksRunner runner = initializeStartupTasksRunner(chromiumFirstStartupRequestMode);
        runner.run(callSite, triggeredFromUIThread);
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
                        mStartupTasksRunnerDelegate,
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

        mDelegate.initThreadUnsafeSingletons();

        if (ApkInfo.isDebugAndroidOrApp()) {
            AwDevToolsServer.setRemoteDebuggingEnabled(true);
        }

        if (CompatQuirks.isEnabled(CompatQuirks.Quirk.LEGACY_DARK_MODE)) {
            AwDarkMode.enableLegacyDarkMode();
        }

        AwBrowserProcess.maybeEnableSafeBrowsingFromGms();
        AwBrowserProcess.setupSupervisedUser();
        AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ true);

        AwBrowserProcess.postBackgroundTasks();

        AwContentsStatics.setSelectionActionMenuClient(mDelegate.getSelectionActionMenuClient());

        AwCrashyClassUtils.maybeCrashIfEnabled();

        mDelegate.onStartupComplete();

        PostTask.disablePreNativeUiTasks(false);
        AwBrowserProcess.onStartupComplete();
    }
}
