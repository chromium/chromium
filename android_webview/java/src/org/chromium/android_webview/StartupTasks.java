// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.SystemClock;
import android.os.storage.StorageManager;

import org.chromium.android_webview.accessibility.AwAccessibilityStateVisibilityManager;
import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.common.crash.AwCrashReporterClient;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.metrics.AwMetricsLogUploader;
import org.chromium.android_webview.metrics.TrackExitReasons;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.android_webview.supervised_user.AwSupervisedUserUrlClassifier;
import org.chromium.android_webview.variations.VariationsSeedLoader;
import org.chromium.base.ApkInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.PowerMonitor;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryPrefetcher;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.ResourceBundle;
import org.chromium.ui.display.DisplayAndroidManager;

import java.util.UUID;
import java.util.concurrent.CountDownLatch;

/**
 * Utility class containing stateless top-level scheduling-unit tasks executed during Chromium
 * initialization in WebView.
 *
 * <p>Each method corresponds to a distinct phase or scheduling unit coordinated by {@link
 * StartupController} and executed by {@link StartupTasksRunner}.
 */
@NullMarked
public final class StartupTasks {

    // TODO(crbug.com/444217485): StartupTasks is intended to be stateless. Remove this latch once
    // the WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT experiment is concluded.
    private static final CountDownLatch sNonUiThreadCapableStartupTasksLatch =
            new CountDownLatch(1);

    /**
     * Prepares the Java environment and prerequisites on the UI thread before native browser
     * process initialization begins.
     */
    public static void preBrowserProcessStartStepOne(StartupController.Delegate delegate) {
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
            runNonUiThreadCapableStartupTasks(delegate);
        }
        delegate.waitForJavaResourcesSetup();
        // NOTE: Finished writing Java resources. From this point on, it's safe
        // to use them.

        delegate.configureChildProcessLauncher();

        // finishInit() must precede native initialization so
        // the seed is available when AwFeatureListCreator::SetUpFieldTrials()
        // runs.
        VariationsSeedLoader.finishInit();
    }

    /**
     * Start the process of initializing the process, up to the point of starting the browser
     * process.
     */
    public static void preBrowserProcessStartStepTwo() {
        ThreadUtils.assertOnUiThread();
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped("StartupTasks.preBrowserProcessStartStepTwo")) {
            final Context appContext = ContextUtils.getApplicationContext();
            AwCrashReporterClient.setProcessNameCrashKey(ContextUtils.getProcessName());
            AwDataDirLock.lock(appContext);

            if (isMultiProcess()) {
                PostTask.postTask(
                        TaskTraits.BEST_EFFORT,
                        () -> {
                            ChildProcessLauncherHelper.warmUpOnAnyThread(appContext);
                        });
            }
            DisplayAndroidManager.disableHdrSdrRatioCallback();
            // The policies are used by browser startup, so we need to register the
            // policy providers before starting the browser process. This only registers
            // java objects and doesn't need the native library.
            CombinedPolicyProvider.get().registerProvider(new AwPolicyProvider(appContext));

            // Check android settings but only when safebrowsing is enabled.
            try (DualTraceEvent e2 =
                    DualTraceEvent.scoped("StartupTasks.maybeEnableSafeBrowsingFromManifest")) {
                AwSafeBrowsingConfigHelper.maybeEnableSafeBrowsingFromManifest();
            }
        }
    }

    /**
     * Runs immediate post-browser process startup tasks on the UI thread right after native browser
     * process startup completes.
     */
    public static void postBrowserProcessStartStepOne() {
        ThreadUtils.assertOnUiThread();
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped("StartupTasks.postBrowserProcessStartStepOne")) {
            finishBrowserProcessStart();

            // TODO(crbug.com/332706093): See if this can be moved before loading native.
            if (!WebViewCachedFlags.get()
                    .isCachedFeatureEnabled(AwFeatures.WEBVIEW_BACKGROUND_CLASS_PRELOADING)) {
                AwClassPreloader.preloadClasses();
            }

            Context applicationContext = ContextUtils.getApplicationContext();
            if (applicationContext.checkSelfPermission(Manifest.permission.ACCESS_NETWORK_STATE)
                    == PackageManager.PERMISSION_GRANTED) {
                NetworkChangeNotifier.init();
                NetworkChangeNotifier.setAutoDetectConnectivityState(
                        new AwNetworkChangeNotifierRegistrationPolicy());
            }
        }
    }

    /**
     * Runs the final UI-thread initialization steps before transitioning WebView startup state to
     * finished.
     */
    public static void postBrowserProcessStartStepTwo(StartupController.Delegate delegate) {
        ThreadUtils.assertOnUiThread();

        AwMetricsLogUploader.initializeUploader();

        int targetSdkVersion =
                ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion;
        RecordHistogram.recordSparseHistogram("Android.WebView.TargetSdkVersion", targetSdkVersion);

        if (ApkInfo.isDebugAndroidOrApp()) {
            AwDevToolsServer.setRemoteDebuggingEnabled(true);
        }

        if (CompatQuirks.isEnabled(CompatQuirks.Quirk.LEGACY_DARK_MODE)) {
            AwDarkMode.enableLegacyDarkMode();
        }

        AwSafeBrowsingConfigHelper.maybeEnableSafeBrowsingFromGms();
        AwSupervisedUserUrlClassifier.checkRestrictedContentBlocking();
        AwMinidumpUploader.handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ true);
        AwAccessibilityStateVisibilityManager.initializeOnStartup();
        AwTracingController.getInstance();

        postBackgroundTasks();

        AwContentsStatics.setSelectionActionMenuClient(delegate.getSelectionActionMenuClient());

        AwCrashyClassUtils.maybeCrashIfEnabled();
    }

    /**
     * Runs startup tasks that do not require the UI thread and can run in parallel on a background
     * thread.
     */
    public static void runNonUiThreadCapableStartupTasks(StartupController.Delegate delegate) {
        try {
            ResourceBundle.setAvailablePakLocales(AwLocaleConfig.getWebViewSupportedPakLocales());

            try (DualTraceEvent ignored =
                    DualTraceEvent.scoped("LibraryLoader.ensureInitialized")) {
                LibraryLoader.getInstance().ensureInitialized();
            }

            try (DualTraceEvent e =
                    DualTraceEvent.scoped("StartupTasks.configureDrawingFunctions")) {
                AwDrawFnImpl.setDrawFnFunctionTable(delegate.getDrawFnFunctionTable());
                AwContents.setAwDrawSWFunctionTable(delegate.getDrawSWFunctionTable());
            }

            AwContentsStatics.setCheckClearTextPermitted(
                    !CompatQuirks.isEnabled(CompatQuirks.Quirk.ALLOW_ALL_CLEARTEXT_TRAFFIC));
        } finally {
            sNonUiThreadCapableStartupTasksLatch.countDown();
        }
    }

    // TODO(crbug.com/544990736): This is only a separate package-private method because it is used
    // by AwBrowserProcess.startForTesting(). Inline this into postBrowserProcessStartStepOne once
    // test startup is migrated.
    /* package */ static void finishBrowserProcessStart() {
        ThreadUtils.assertOnUiThread();
        try (DualTraceEvent e1 = DualTraceEvent.scoped("StartupTasks.finishBrowserProcessStart")) {
            if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
                BrowserStartupController.getInstance()
                        .startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_WEBVIEW,
                                !isMultiProcess(),
                                /* startGpuProcess= */ false);
            }
            try (DualTraceEvent ignored =
                    DualTraceEvent.scoped(
                            "StartupTasks.finishBrowserProcessStart.createPowerMonitor")) {
                PowerMonitor.create();
            }
            try (DualTraceEvent ignored =
                    DualTraceEvent.scoped(
                            "StartupTasks.finishBrowserProcessStart.setSafeBrowsingHandler")) {
                PlatformServiceBridge.getInstance().setSafeBrowsingHandler();
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                AwContentsLifecycleNotifier.initialize();
            }

            PostTask.postTask(
                    TaskTraits.BEST_EFFORT,
                    () -> {
                        RecordHistogram.recordSparseHistogram(
                                "Android.PlayServices.Version",
                                PlatformServiceBridge.getInstance().getGmsVersionCode());
                    });
        }
    }

    private static void waitForNonUiThreadCapableStartupTasks() {
        try (DualTraceEvent e2 =
                DualTraceEvent.scoped("StartupTasks.waitForNonUiThreadCapableStartupTasks")) {
            sNonUiThreadCapableStartupTasksLatch.await();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Post tasks that need to run in the background thread after the browser process has started.
     */
    private static void postBackgroundTasks() {
        if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_VERBOSE_LOGGING)) {
            // Log extra information, for debugging purposes.
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT,
                    () -> {
                        // TODO(ntfschr): CommandLine can change at any time. For simplicity, only
                        // log it once during startup.
                        AwContentsStatics.logCommandLineForDebugging();
                        // Field trials can be activated at any time. We'll continue logging them as
                        // they're activated.
                        FieldTrialList.logActiveTrials();
                    });
        }

        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    WebViewCachedFlags.get().onStartupCompleted();
                });

        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_PREFETCH_NATIVE_LIBRARY)
                && !AwFeatureMap.getInstance()
                        .getFieldTrialParamByFeatureAsBoolean(
                                AwFeatures.WEBVIEW_PREFETCH_NATIVE_LIBRARY,
                                "WebViewPrefetchFromRenderer",
                                true)) {
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT,
                    () -> {
                        LibraryPrefetcher.prefetchNativeLibraryForWebView();
                    });
        }

        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_RECORD_APP_CACHE_HISTOGRAMS)) {
            PostTask.postDelayedTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                    () -> {
                        StorageManager storageManager =
                                (StorageManager)
                                        ContextUtils.getApplicationContext()
                                                .getSystemService(Context.STORAGE_SERVICE);
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
                                    storageManager.getCacheQuotaBytes(storageUuid) / 1024;
                            RecordHistogram.recordCount1MHistogram(
                                    "Android.WebView.CacheQuotaSize", (int) cacheQuotaKiloBytes);
                        } catch (Exception e) {
                        } finally {
                            RecordHistogram.recordTimesHistogram(
                                    "Android.WebView.GetCacheQuotaSizeTime",
                                    SystemClock.uptimeMillis() - startTimeGetCacheQuotaMs);
                        }

                        long startTimeGetCacheSizeMs = SystemClock.uptimeMillis();
                        long cacheSizeKiloBytes = -1;
                        try {
                            // This can throw `SecurityException` if the app doesn't
                            // have sufficient privileges.
                            // See crbug.com/422174715
                            cacheSizeKiloBytes =
                                    storageManager.getCacheSizeBytes(storageUuid) / 1024;
                            RecordHistogram.recordCount1MHistogram(
                                    "Android.WebView.CacheSize", (int) cacheSizeKiloBytes);
                        } catch (Exception e) {
                        } finally {
                            RecordHistogram.recordTimesHistogram(
                                    "Android.WebView.GetCacheSizeTime",
                                    SystemClock.uptimeMillis() - startTimeGetCacheSizeMs);
                        }
                        if (cacheQuotaKiloBytes != -1 && cacheSizeKiloBytes != -1) {
                            long quotaRemainingKiloBytes = cacheQuotaKiloBytes - cacheSizeKiloBytes;
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
    }

    private static boolean isMultiProcess() {
        return CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
    }

    private StartupTasks() {}
}
