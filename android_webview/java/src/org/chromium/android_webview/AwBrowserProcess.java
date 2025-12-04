// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.StrictMode;
import android.os.SystemClock;
import android.os.storage.StorageManager;
import android.util.LruCache;
import android.util.Pair;

import androidx.annotation.IntDef;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.common.services.ICrashReceiverService;
import org.chromium.android_webview.common.services.IMetricsBridgeService;
import org.chromium.android_webview.common.services.ServiceConnectionDelayRecorder;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.metrics.AndroidMetricsLogConsumer;
import org.chromium.android_webview.metrics.AndroidMetricsLogUploader;
import org.chromium.android_webview.metrics.AwMetricsLogUploader;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.android_webview.metrics.AwNonembeddedUmaReplayer;
import org.chromium.android_webview.metrics.MetricsFilteringDecorator;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.android_webview.proto.MetricsBridgeRecords.HistogramRecord;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.android_webview.supervised_user.AwSupervisedUserSafeModeAction;
import org.chromium.android_webview.supervised_user.AwSupervisedUserUrlClassifier;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.PowerMonitor;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryPrefetcher;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.component_updater.ComponentLoaderPolicyBridge;
import org.chromium.components.component_updater.EmbeddedComponentLoader;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.ui.display.DisplayAndroidManager;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Wrapper for the steps needed to initialize the java and native sides of webview chromium. */
@JNINamespace("android_webview")
@Lifetime.Singleton
public final class AwBrowserProcess {
    private static final String TAG = "AwBrowserProcess";

    private static final String WEBVIEW_DIR_BASENAME = "webview";

    private static final int MINUTES_PER_DAY =
            (int) TimeUnit.SECONDS.toMinutes(TimeUtils.SECONDS_PER_DAY);

    // To avoid any potential synchronization issues we post all minidump-copying actions to
    // the same sequence to be run serially.
    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    private static String sWebViewPackageName;
    private static @ApkType int sApkType;
    private static @Nullable String sProcessDataDirSuffix;
    private static boolean sDataDirBasePathOverridden;

    /**
     * Loads the native library, and performs basic static construction of objects needed to run
     * webview in this process. Does not create threads; safe to call from zygote. Note: it is up to
     * the caller to ensure this is only called once.
     *
     * @param processDataDirSuffix The suffix to use when setting the data directory for this
     *     process; null to use no suffix.
     */
    public static void loadLibrary(String processDataDirSuffix) {
        loadLibrary(null, null, processDataDirSuffix);
    }

    /**
     * Loads the native library, and performs basic static construction of objects needed to run
     * webview in this process. Does not create threads; safe to call from zygote. Note: it is up to
     * the caller to ensure this is only called once.
     *
     * @param processDataDirBasePath The base path to use when setting the data directory for this
     *     process; null to use default base path.
     * @param processCacheDirBasePath The base path to use when setting the cache directory for this
     *     process; null to use default base path.
     * @param processDataDirSuffix The suffix to use when setting the data directory for this
     *     process; null to use no suffix.
     */
    public static void loadLibrary(
            String processDataDirBasePath,
            String processCacheDirBasePath,
            String processDataDirSuffix) {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_WEBVIEW);
        sProcessDataDirSuffix = processDataDirSuffix;
        sDataDirBasePathOverridden = (processDataDirBasePath != null);
        if (processDataDirSuffix == null) {
            PathUtils.setPrivateDirectoryPath(
                    processDataDirBasePath,
                    processCacheDirBasePath,
                    WEBVIEW_DIR_BASENAME,
                    "WebView");
        } else {
            String processDataDirName = WEBVIEW_DIR_BASENAME + "_" + processDataDirSuffix;
            PathUtils.setPrivateDirectoryPath(
                    processDataDirBasePath,
                    processCacheDirBasePath,
                    processDataDirName,
                    processDataDirName);
        }
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            LibraryLoader.getInstance().loadNow();
            // Switch the command line implementation from Java to native.
            // It's okay for the WebView to do this before initialization because we have
            // setup the JNI bindings by this point.
            LibraryLoader.getInstance().switchCommandLineForWebView();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Configures child process launcher. This is required only if child services are used in
     * WebView.
     */
    public static void configureChildProcessLauncher() {
        final boolean isExternalService = true;
        final boolean bindToCaller = true;
        final boolean ignoreVisibilityForImportance = true;
        ChildProcessCreationParams.set(
                getWebViewPackageName(),
                getWebViewPackageName(),
                isExternalService,
                LibraryProcessType.PROCESS_WEBVIEW_CHILD,
                bindToCaller,
                ignoreVisibilityForImportance);
    }

    /**
     * Configures child process launcher for tests. This is required for multiprocess mode to ensure
     * the process type of the child process is WebView, but many of the other fields from
     * configureChildProcessLauncher do not work in testing, so tests need a customized version of
     * that method.
     */
    public static void configureChildProcessLauncherForTesting() {
        final boolean isExternalService = false;
        final boolean bindToCaller = false;
        final boolean ignoreVisibilityForImportance = false;
        ChildProcessCreationParams.set(
                ContextUtils.getApplicationContext().getPackageName(),
                ContextUtils.getApplicationContext().getPackageName(),
                isExternalService,
                LibraryProcessType.PROCESS_WEBVIEW_CHILD,
                bindToCaller,
                ignoreVisibilityForImportance);
    }

    /**
     * Asynchronously triggers the chromium browser process initialization. Creates threads and
     * performs other per-app resource allocations; must not be called from zygote.
     *
     * <p>Note: it is up to the caller to ensure this is only called once.
     *
     * @param callback This is triggered when the async startup completes.
     * @param shouldScheduleFlushStartupTasks Whether to post a task to flush the startup tasks
     *     instead of letting them complete asynchronously
     */
    public static void triggerAsyncBrowserProcess(
            StartupCallback callback, boolean shouldScheduleFlushStartupTasks) {
        ThreadUtils.assertOnUiThread();
        try (DualTraceEvent e2 =
                DualTraceEvent.scoped("AwBrowserProcess.startBrowserProcessAsync")) {
            BrowserStartupController.getInstance()
                    .startBrowserProcessesAsync(
                            LibraryProcessType.PROCESS_WEBVIEW,
                            /* startGpuProcess= */ false,
                            /* startMinimalBrowser= */ false,
                            /* singleProcess= */ !isMultiProcess(),
                            /* scheduleFlushStartupTasks= */ shouldScheduleFlushStartupTasks,
                            callback);
        }
    }

    /**
     * Finishes the chromium browser process initialization. Starts the browser process
     * synchronously if not already started.
     *
     * <p>Note: it is up to the caller to ensure this is only called once.
     */
    public static void finishBrowserProcessStart() {
        ThreadUtils.assertOnUiThread();
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped("AwBrowserProcess.finishBrowserProcessStart")) {
            if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
                BrowserStartupController.getInstance()
                        .startBrowserProcessesSync(
                                LibraryProcessType.PROCESS_WEBVIEW,
                                !isMultiProcess(),
                                /* startGpuProcess= */ false);
            }
            try (DualTraceEvent ignored =
                    DualTraceEvent.scoped(
                            "AwBrowserProcess.finishBrowserProcessStart.createPowerMonitor")) {
                PowerMonitor.create();
            }
            try (DualTraceEvent ignored =
                    DualTraceEvent.scoped(
                            "AwBrowserProcess.finishBrowserProcessStart.setSafeBrowsingHandler")) {
                PlatformServiceBridge.getInstance().setSafeBrowsingHandler();
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                AwContentsLifecycleNotifier.initialize();
            }

            if (!WebViewCachedFlags.get()
                    .isCachedFeatureEnabled(
                            AwFeatures.WEBVIEW_OPT_IN_TO_GMS_BIND_SERVICE_OPTIMIZATION)) {
                setupSupervisedUser();
            }

            if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_CACHE_BOUNDARY_INTERFACE_METHODS)) {
                // There are currently less than 200 methods in the boundary interfaces.
                // This cache should only start evicting elements if the cache keys somehow don't
                // have value semantics.
                LruCache<Pair<Method, @Nullable ClassLoader>, @Nullable Method> cache =
                        new LruCache<>(200) {
                            @Override
                            protected void entryRemoved(
                                    boolean evicted,
                                    Pair<Method, ClassLoader> key,
                                    Method oldValue,
                                    Method newValue) {
                                super.entryRemoved(evicted, key, oldValue, newValue);
                                // This is a counting histogram.
                                RecordHistogram.recordBooleanHistogram(
                                        "Android.WebView.AndroidX.MethodCacheEviction", true);
                            }
                        };

                // The LruCache.get method is final, so we have to do logging of the lookup result
                // as a separate consumer.
                Consumer<Boolean> getResultLogger =
                        gotCacheResult ->
                                RecordHistogram.recordBooleanHistogram(
                                        "Android.WebView.AndroidX.MethodCacheGetResult",
                                        gotCacheResult);

                BoundaryInterfaceReflectionUtil.setMethodCache(cache, getResultLogger);
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

    /**
     * Runs parts of browser process start that precede starting the browser process via the
     * BrowserStartupController.
     */
    public static void runPreBrowserProcessStart() {
        ThreadUtils.assertOnUiThread();
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped("AwBrowserProcess.runPreBrowserProcessStart")) {
            final Context appContext = ContextUtils.getApplicationContext();
            AwBrowserProcessJni.get().setProcessNameCrashKey(ContextUtils.getProcessName());
            AwDataDirLock.lock(appContext);

            if (isMultiProcess()) {
                PostTask.postTask(
                        TaskTraits.BEST_EFFORT,
                        () -> {
                            ChildProcessLauncherHelper.warmUpOnAnyThread(appContext);
                        });
            }
            configureDisplayAndroidManager();
            // The policies are used by browser startup, so we need to register the
            // policy providers before starting the browser process. This only registers
            // java objects and doesn't need the native library.
            CombinedPolicyProvider.get().registerProvider(new AwPolicyProvider(appContext));

            // Check android settings but only when safebrowsing is enabled.
            try (DualTraceEvent e2 =
                    DualTraceEvent.scoped("AwBrowserProcess.maybeEnableSafeBrowsingFromManifest")) {
                AwSafeBrowsingConfigHelper.maybeEnableSafeBrowsingFromManifest();
            }
            if (!WebViewCachedFlags.get()
                    .isCachedFeatureEnabled(
                            AwFeatures.WEBVIEW_OPT_IN_TO_GMS_BIND_SERVICE_OPTIMIZATION)) {
                maybeEnableSafeBrowsingFromGms();
            }
        }
    }

    public static void setupSupervisedUser() {
        try (DualTraceEvent ignored =
                DualTraceEvent.scoped("AwBrowserProcess.setupSupervisedUser")) {
            AwSupervisedUserUrlClassifier classifier = AwSupervisedUserUrlClassifier.getInstance();
            if (classifier != null && AwSupervisedUserSafeModeAction.isSupervisionEnabled()) {
                classifier.checkIfNeedRestrictedContentBlocking();
            }
        }
    }

    public static void maybeEnableSafeBrowsingFromGms() {
        try (DualTraceEvent e2 =
                DualTraceEvent.scoped("AwBrowserProcess.maybeEnableSafeBrowsingFromGms")) {
            AwSafeBrowsingConfigHelper.maybeEnableSafeBrowsingFromGms();
        }
    }

    private static boolean isMultiProcess() {
        return CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
    }

    /**
     * onStartupComplete performs the final steps of Chromium startup, e.g enabling the task
     * runners. It's called when WebViewChromiumAwInit startup tasks are done. Tests that start the
     * browser process directly should use this.
     */
    public static void startForTesting() {
        runPreBrowserProcessStart();
        finishBrowserProcessStart();
        onStartupComplete();
    }

    public static void setWebViewPackageName(String webViewPackageName) {
        assert sWebViewPackageName == null || sWebViewPackageName.equals(webViewPackageName);
        sWebViewPackageName = webViewPackageName;
    }

    public static String getWebViewPackageName() {
        if (sWebViewPackageName == null) return ""; // May be null in testing.
        return sWebViewPackageName;
    }

    public static void setProcessDataDirSuffixForTesting(@Nullable String processDataDirSuffix) {
        sProcessDataDirSuffix = processDataDirSuffix;
    }

    @Nullable
    public static String getProcessDataDirSuffix() {
        return sProcessDataDirSuffix;
    }

    public static boolean isDataDirBasePathOverridden() {
        return sDataDirBasePathOverridden;
    }

    public static void initializeApkType(ApplicationInfo info) {
        if (info == null || info.metaData == null) {
            sApkType = ApkType.UNKNOWN;
            return;
        }

        String libraryName = info.metaData.getString("com.android.webview.WebViewLibrary");
        if (libraryName == null) {
            sApkType = ApkType.UNKNOWN;
            return;
        }

        if (libraryName.contains("libwebviewchromium")) {
            // The library name for standalone should be "libwebviewchromium.so".
            sApkType = ApkType.STANDALONE;
            return;
        }

        if (libraryName.contains("libmonochrome")) {
            // The library name for monochrome and trichrome is "libmonochrome.so"
            // or "libmonochrome_64.so".
            if (info.sharedLibraryFiles != null && info.sharedLibraryFiles.length > 0) {
                // Only Trichrome uses shared library files.
                sApkType = ApkType.TRICHROME;
            } else if (info.className.toLowerCase(Locale.ROOT).contains("monochrome")) {
                // Only Monochrome has "monochrome" in the application class name.
                sApkType = ApkType.MONOCHROME;
            } else {
                sApkType = ApkType.UNKNOWN;
            }
            return;
        }

        sApkType = ApkType.UNKNOWN;
    }

    /** Returns the WebView APK type. */
    @CalledByNative
    public static @ApkType int getApkType() {
        return sApkType;
    }

    /** Trigger minidump copying, which in turn triggers minidump uploading. */
    @CalledByNative
    private static void triggerMinidumpUploading() {
        handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ false);
    }

    /**
     * Trigger minidump uploading, and optionaly also update the metrics-consent value depending on
     * whether the Android Checkbox is toggled on.
     *
     * @param updateMetricsConsent whether to update the metrics-consent value to represent the
     *     Android Checkbox toggle.
     */
    public static void handleMinidumpsAndSetMetricsConsent(final boolean updateMetricsConsent) {
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped("AwBrowserProcess.handleMinidumpsAndSetMetricsConsent")) {
            final boolean enableMinidumpUploadingForTesting =
                    CommandLine.getInstance()
                            .hasSwitch(BaseSwitches.ENABLE_CRASH_REPORTER_FOR_TESTING);
            if (enableMinidumpUploadingForTesting) {
                handleMinidumps(/* userApproved= */ true);
            }

            PlatformServiceBridge.getInstance()
                    .queryMetricsSetting(
                            enabled -> {
                                ThreadUtils.assertOnUiThread();
                                boolean userApproved = Boolean.TRUE.equals(enabled);
                                if (updateMetricsConsent) {
                                    AwMetricsServiceClient.setConsentSetting(userApproved);
                                }

                                if (!enableMinidumpUploadingForTesting) {
                                    handleMinidumps(userApproved);
                                }
                            });
        }
    }

    private static String getCrashUuid(File file) {
        String fileName = file.getName();
        // crash report uuid is the minidump file name without any extensions.
        int firstDotIndex = fileName.indexOf('.');
        if (firstDotIndex == -1) {
            firstDotIndex = fileName.length();
        }
        return fileName.substring(0, firstDotIndex);
    }

    private static void deleteMinidumps(final File[] minidumpFiles) {
        for (File minidump : minidumpFiles) {
            if (!minidump.delete()) {
                Log.w(TAG, "Couldn't delete file " + minidump.getAbsolutePath());
            }
        }
    }

    private static void transmitMinidumps(
            final File[] minidumpFiles,
            final Map<String, Map<String, String>> crashesInfoMap,
            final ICrashReceiverService service) {
        // Pass file descriptors pointing to our minidumps to the
        // minidump-copying service, allowing it to copy contents of the
        // minidumps to WebView's data directory.
        // Delete the app filesystem references to the minidumps after passing
        // the file descriptors so that we avoid trying to copy the minidumps
        // again if anything goes wrong. This makes sense given that a failure
        // to copy a file usually means that retrying won't succeed either,
        // because e.g. the disk is full, or the file system is corrupted.
        int fileCount = minidumpFiles.length;
        ParcelFileDescriptor[] minidumpFds = new ParcelFileDescriptor[fileCount];
        Map<String, String>[] crashInfos = new Map[fileCount];
        for (int i = 0; i < fileCount; ++i) {
            File file = minidumpFiles[i];
            ParcelFileDescriptor p = null;
            try {
                p = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
            } catch (IOException e) {
            }
            minidumpFds[i] = p;
            crashInfos[i] = crashesInfoMap.get(getCrashUuid(file));
        }

        try {
            // AIDL does not support arrays of objects, so use a List here.
            service.transmitCrashes(minidumpFds, Arrays.asList(crashInfos));
        } catch (Exception e) {
            // Exception can be RemoteException, or "RuntimeException: Too many open files".
            // https://crbug.com/1399777
            // TODO(gsennton): add a UMA metric here to ensure we aren't losing
            // too many minidumps because of this.
        }
        deleteMinidumps(minidumpFiles);
        for (ParcelFileDescriptor fd : minidumpFds) {
            StreamUtil.closeQuietly(fd);
        }
    }

    /**
     * Pass Minidumps to a separate Service declared in the WebView provider package. That Service
     * will copy the Minidumps to its own data directory - at which point we can delete our copies
     * in the app directory.
     *
     * @param userApproved whether we have user consent to upload crash data - if we do, copy the
     *     minidumps, if we don't, delete them.
     */
    public static void handleMinidumps(boolean userApproved) {
        sSequencedTaskRunner.execute(() -> handleMinidumpsInternal(userApproved));
    }

    private static void handleMinidumpsInternal(final boolean userApproved) {
        try {
            final Context appContext = ContextUtils.getApplicationContext();
            final File cacheDir = new File(PathUtils.getCacheDirectory());
            final CrashFileManager crashFileManager = new CrashFileManager(cacheDir);

            // The lifecycle of a minidump in the app directory is very simple:
            // foo.dmpNNNNN --
            // where NNNNN is a Process ID (PID) -- gets created, and is either deleted
            // or
            // copied over to the shared crash directory for all WebView-using apps.
            Map<String, Map<String, String>> crashesInfoMap =
                    crashFileManager.importMinidumpsCrashKeys();
            final File[] minidumpFiles = crashFileManager.getCurrentMinidumpsSansLogcat();
            if (minidumpFiles.length == 0) return;

            // Delete the minidumps if the user doesn't allow crash data uploading.
            if (!userApproved) {
                deleteMinidumps(minidumpFiles);
                return;
            }

            final Intent intent = new Intent();
            intent.setClassName(getWebViewPackageName(), ServiceNames.CRASH_RECEIVER_SERVICE);

            ServiceConnection connection =
                    new ServiceConnection() {
                        private boolean mHasConnected;

                        @Override
                        public void onServiceConnected(ComponentName className, IBinder service) {
                            if (mHasConnected) return;
                            mHasConnected = true;
                            // onServiceConnected is called on the UI thread, so punt
                            // this back to the background thread.
                            sSequencedTaskRunner.execute(
                                    () -> {
                                        transmitMinidumps(
                                                minidumpFiles,
                                                crashesInfoMap,
                                                ICrashReceiverService.Stub.asInterface(service));
                                        appContext.unbindService(this);
                                    });
                        }

                        @Override
                        public void onServiceDisconnected(ComponentName className) {}
                    };
            if (!ServiceHelper.bindService(
                    appContext, intent, connection, Context.BIND_AUTO_CREATE)) {
                Log.w(TAG, "Could not bind to Minidump-copying Service " + intent);
            }
        } catch (RuntimeException e) {
            // We don't want to crash the app if we hit an unexpected exception during
            // minidump uploading as this could potentially put the app into a
            // persistently bad state.
            // Just log it.
            Log.e(TAG, "Exception during minidump uploading process!", e);
        }
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        TransmissionResult.SUCCESS,
        TransmissionResult.MALFORMED_PROTOBUF,
        TransmissionResult.REMOTE_EXCEPTION
    })
    private @interface TransmissionResult {
        int SUCCESS = 0;
        int MALFORMED_PROTOBUF = 1;
        int REMOTE_EXCEPTION = 2;
        int COUNT = 3;
    }

    private static void logTransmissionResult(@TransmissionResult int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.NonEmbeddedMetrics.TransmissionResult",
                sample,
                TransmissionResult.COUNT);
    }

    /**
     * Record very long times UMA histogram up to 4 days.
     *
     * @param name histogram name.
     * @param time time sample in millis.
     */
    private static void recordVeryLongTimesHistogram(String name, long time) {
        long timeMins = TimeUnit.MILLISECONDS.toMinutes(time);
        int sample;
        // Safely convert to int to avoid positive or negative overflow.
        if (timeMins > Integer.MAX_VALUE) {
            sample = Integer.MAX_VALUE;
        } else if (timeMins < Integer.MIN_VALUE) {
            sample = Integer.MIN_VALUE;
        } else {
            sample = (int) timeMins;
        }
        RecordHistogram.recordCustomCountHistogram(name, sample, 1, 4 * MINUTES_PER_DAY, 50);
    }

    /**
     * Connect to {@link org.chromium.android_webview.services.MetricsBridgeService} to retrieve any
     * recorded UMA metrics from nonembedded WebView services and transmit them back using UMA APIs.
     */
    public static void collectNonembeddedMetrics() {
        if (ManifestMetadataUtil.isAppOptedOutFromMetricsCollection()) {
            Log.d(TAG, "App opted out from metrics collection, not connecting to metrics service");
            return;
        }

        final Intent intent = new Intent();
        intent.setClassName(getWebViewPackageName(), ServiceNames.METRICS_BRIDGE_SERVICE);

        ServiceConnectionDelayRecorder connection =
                new ServiceConnectionDelayRecorder() {
                    private boolean mHasConnected;

                    @Override
                    public void onServiceConnectedImpl(ComponentName className, IBinder service) {
                        if (mHasConnected) return;
                        mHasConnected = true;
                        // onServiceConnected is called on the UI thread, so punt this back to the
                        // background thread.
                        PostTask.postTask(
                                TaskTraits.BEST_EFFORT,
                                () -> {
                                    sendMetricsToService(service);
                                    ContextUtils.getApplicationContext().unbindService(this);
                                });
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName className) {}
                };

        Context appContext = ContextUtils.getApplicationContext();
        if (!connection.bind(appContext, intent, Context.BIND_AUTO_CREATE)) {
            Log.d(TAG, "Could not bind to MetricsBridgeService " + intent);
        }
    }

    private static void sendMetricsToService(IBinder service) {
        try {
            IMetricsBridgeService metricsService = IMetricsBridgeService.Stub.asInterface(service);

            List<byte[]> data = metricsService.retrieveNonembeddedMetrics();
            RecordHistogram.recordCount1000Histogram(
                    "Android.WebView.NonEmbeddedMetrics.NumHistograms", data.size());
            long systemTime = System.currentTimeMillis();
            for (byte[] recordData : data) {
                HistogramRecord record = HistogramRecord.parseFrom(recordData);
                AwNonembeddedUmaReplayer.replayMethodCall(record);
                if (record.hasMetadata()) {
                    long timeRecorded = record.getMetadata().getTimeRecorded();
                    recordVeryLongTimesHistogram(
                            "Android.WebView.NonEmbeddedMetrics.HistogramRecordAge",
                            systemTime - timeRecorded);
                }
            }
            logTransmissionResult(TransmissionResult.SUCCESS);
        } catch (InvalidProtocolBufferException e) {
            Log.d(TAG, "Malformed metrics log proto", e);
            logTransmissionResult(TransmissionResult.MALFORMED_PROTOBUF);
        } catch (Exception e) {
            // RemoteException, IllegalArgumentException
            // (https://crbug.com/1403976)
            Log.d(TAG, "Remote Exception in MetricsBridgeService#retrieveMetrics", e);
            logTransmissionResult(TransmissionResult.REMOTE_EXCEPTION);
        }
    }

    /**
     * Load components files from {@link
     * org.chromium.android_webview.services.ComponentsProviderService}.
     */
    public static void loadComponents() {
        try (DualTraceEvent e = DualTraceEvent.scoped("AwBrowserProcess.loadComponents")) {
            ComponentLoaderPolicyBridge[] componentPolicies =
                    AwBrowserProcessJni.get().getComponentLoaderPolicies();
            // Don't connect to the service if there are no components to load.
            if (componentPolicies.length == 0) {
                return;
            }

            // The origin trial component was the only component we were
            // fetching, and we're in the process of disabling the component
            // updater entirely. So, if fetching the origin trial component is
            // disabled, we expect there to be no components to fetch, as no
            // new ones should be being added to WebView.
            // If we get here there was at least one component registered:
            // crash on debug builds, otherwise no-op.
            boolean componentLoadingAllowed =
                    AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_FETCH_ORIGIN_TRIALS_COMPONENT);
            assert componentLoadingAllowed;
            if (!componentLoadingAllowed) {
                Log.w(TAG, "Components were registered but component loading is disabled!");
                return;
            }

            EmbeddedComponentLoader loader =
                    new EmbeddedComponentLoader(Arrays.asList(componentPolicies));
            final Intent intent = new Intent();
            intent.setClassName(
                    getWebViewPackageName(),
                    EmbeddedComponentLoader.AW_COMPONENTS_PROVIDER_SERVICE);
            loader.connect(
                    intent,
                    AwFeatureMap.isEnabled(
                            AwFeatures.WEBVIEW_CONNECT_TO_COMPONENT_PROVIDER_IN_BACKGROUND));
        }
    }

    /** Initialize the metrics uploader. */
    public static void initializeMetricsLogUploader() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("AwBrowserProcess.initializeMetricsLogUploader")) {
            boolean metricServiceEnabledOnlySdkRuntime =
                    ContextUtils.isSdkSandboxProcess()
                            && AwFeatureMap.isEnabled(
                                    AwFeatures.WEBVIEW_USE_METRICS_UPLOAD_SERVICE_ONLY_SDK_RUNTIME);

            if (metricServiceEnabledOnlySdkRuntime) {
                boolean isAsync =
                        AwFeatureMap.isEnabled(AwFeatures.ANDROID_METRICS_ASYNC_METRIC_LOGGING);
                AwMetricsLogUploader uploader = new AwMetricsLogUploader(isAsync);
                // Open a connection during startup while connecting to other services such as
                // ComponentsProviderService and VariationSeedServer to try to avoid spinning the
                // nonembedded ":webview_service" twice.
                uploader.initialize();
                AndroidMetricsLogUploader.setConsumer(new MetricsFilteringDecorator(uploader));
            } else {
                AndroidMetricsLogConsumer directUploader =
                        data -> {
                            PlatformServiceBridge.getInstance().logMetrics(data);
                            return HttpURLConnection.HTTP_OK;
                        };
                AndroidMetricsLogUploader.setConsumer(
                        new MetricsFilteringDecorator(directUploader));
            }
        }
    }

    public static void doNetworkInitializations(Context applicationContext) {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("AwBrowserProcess.doNetworkInitializations")) {
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

    /**
     * Post tasks that need to run in the background thread after the browser process has started.
     */
    public static void postBackgroundTasks(boolean isSafeModeEnabled, SharedPreferences prefs) {
        if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_VERBOSE_LOGGING)) {
            // Log extra information, for debugging purposes.
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT,
                    () -> {
                        // TODO(ntfschr): CommandLine can change at any time. For simplicity, only
                        // log
                        // it once during startup.
                        AwContentsStatics.logCommandLineForDebugging();
                        // Field trials can be activated at any time. We'll continue logging them as
                        // they're activated.
                        FieldTrialList.logActiveTrials();
                        // SafeMode was already determined earlier during the startup sequence, this
                        // just fetches the cached boolean state. If SafeMode was enabled, we
                        // already
                        // logged detailed information about the SafeMode config.
                        Log.i(TAG, "SafeMode enabled: " + isSafeModeEnabled);
                    });
        }

        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    WebViewCachedFlags.get().onStartupCompleted(prefs);
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

    /**
     * Notify the native code that the embedder is done with startup. In WebView's case, this is
     * when we are done running the startup tasks.
     */
    public static void onStartupComplete() {
        AwBrowserProcessJni.get().onStartupComplete();
    }

    /**
     * Start Perfetto initialization.
     *
     * <p>This must only be called <em>before</em> Content startup. If Content Main has already been
     * called, perfetto will already be initialized, and this method will crash.
     *
     * @param enableSystemConsumer Set to {@code true} in order to send Perfetto traces to the
     *     Android system consumer. Equivalent to enabling {@link
     *     org.chromium.services.tracing.TracingServiceFeatures.ENABLE_PERFETTO_SYSTEM_TRACING}
     */
    public static void initPerfetto(boolean enableSystemConsumer) {
        AwBrowserProcessJni.get().initPerfetto(enableSystemConsumer);
    }

    private static void configureDisplayAndroidManager() {
        DisplayAndroidManager.disableHdrSdrRatioCallback();
    }

    // Do not instantiate this class.
    private AwBrowserProcess() {}

    @NativeMethods
    interface Natives {
        void setProcessNameCrashKey(@JniType("std::string") String processName);

        ComponentLoaderPolicyBridge[] getComponentLoaderPolicies();

        void onStartupComplete();

        void initPerfetto(@JniType("bool") boolean enableSystemConsumer);
    }
}
