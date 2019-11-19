// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.StrictMode;

import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.services.ICrashReceiverService;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;
import org.chromium.policy.CombinedPolicyProvider;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Wrapper for the steps needed to initialize the java and native sides of webview chromium.
 */
@JNINamespace("android_webview")
public final class AwBrowserProcess {
    private static final String TAG = "AwBrowserProcess";

    private static final String WEBVIEW_DIR_BASENAME = "webview";
    private static final String EXCLUSIVE_LOCK_FILE = "webview_data.lock";

    // To avoid any potential synchronization issues we post all minidump-copying actions to
    // the same sequence to be run serially.
    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    private static RandomAccessFile sLockFile;
    private static FileLock sExclusiveFileLock;
    private static String sWebViewPackageName;

    /**
     * Loads the native library, and performs basic static construction of objects needed
     * to run webview in this process. Does not create threads; safe to call from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     *
     * @param processDataDirSuffix The suffix to use when setting the data directory for this
     *                             process; null to use no suffix.
     */
    public static void loadLibrary(String processDataDirSuffix) {
        if (processDataDirSuffix == null) {
            PathUtils.setPrivateDataDirectorySuffix(WEBVIEW_DIR_BASENAME, "WebView");
        } else {
            String processDataDirName = WEBVIEW_DIR_BASENAME + "_" + processDataDirSuffix;
            PathUtils.setPrivateDataDirectorySuffix(processDataDirName, processDataDirName);
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
        ChildProcessCreationParams.set(getWebViewPackageName(), isExternalService,
                LibraryProcessType.PROCESS_WEBVIEW_CHILD, bindToCaller,
                ignoreVisibilityForImportance, null /* privilegedServicesName */,
                null /* sandboxedServicesName */);
    }

    /**
     * Starts the chromium browser process running within this process. Creates threads
     * and performs other per-app resource allocations; must not be called from zygote.
     * Note: it is up to the caller to ensure this is only called once.
     */
    public static void start() {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped("AwBrowserProcess.start")) {
            final Context appContext = ContextUtils.getApplicationContext();
            tryObtainingDataDirLock(appContext);
            // We must post to the UI thread to cover the case that the user
            // has invoked Chromium startup by using the (thread-safe)
            // CookieManager rather than creating a WebView.
            ThreadUtils.runOnUiThreadBlocking(() -> {
                boolean multiProcess =
                        CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
                if (multiProcess) {
                    ChildProcessLauncherHelper.warmUp(appContext, true);
                }
                // The policies are used by browser startup, so we need to register the policy
                // providers before starting the browser process. This only registers java objects
                // and doesn't need the native library.
                CombinedPolicyProvider.get().registerProvider(new AwPolicyProvider(appContext));

                // Check android settings but only when safebrowsing is enabled.
                try (ScopedSysTraceEvent e2 =
                                ScopedSysTraceEvent.scoped("AwBrowserProcess.maybeEnable")) {
                    AwSafeBrowsingConfigHelper.maybeEnableSafeBrowsingFromManifest(appContext);
                }

                try (ScopedSysTraceEvent e2 = ScopedSysTraceEvent.scoped(
                             "AwBrowserProcess.startBrowserProcessesSync")) {
                    BrowserStartupController.get(LibraryProcessType.PROCESS_WEBVIEW)
                            .startBrowserProcessesSync(!multiProcess);
                }
            });
        }
    }

    private static void tryObtainingDataDirLock(final Context appContext) {
        try (ScopedSysTraceEvent e1 =
                        ScopedSysTraceEvent.scoped("AwBrowserProcess.tryObtainingDataDirLock")) {
            // Many existing apps rely on this even though it's known to be unsafe.
            // Make it fatal when on P for apps that target P or higher
            boolean dieOnFailure = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                    && appContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P;

            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            try {
                String dataPath = PathUtils.getDataDirectory();
                File lockFile = new File(dataPath, EXCLUSIVE_LOCK_FILE);
                boolean success = false;
                try {
                    // Note that the file is kept open intentionally.
                    sLockFile = new RandomAccessFile(lockFile, "rw");
                    sExclusiveFileLock = sLockFile.getChannel().tryLock();
                    success = sExclusiveFileLock != null;
                } catch (IOException e) {
                    Log.w(TAG, "Failed to create lock file " + lockFile, e);
                }
                if (!success) {
                    final String error =
                            "Using WebView from more than one process at once with the "
                            + "same data directory is not supported. https://crbug.com/558377";
                    if (dieOnFailure) {
                        throw new RuntimeException(error);
                    } else {
                        Log.w(TAG, error);
                    }
                }
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
    }

    public static void setWebViewPackageName(String webViewPackageName) {
        assert sWebViewPackageName == null || sWebViewPackageName.equals(webViewPackageName);
        sWebViewPackageName = webViewPackageName;
    }

    public static String getWebViewPackageName() {
        if (sWebViewPackageName == null) return ""; // May be null in testing.
        return sWebViewPackageName;
    }

    /**
     * Trigger minidump copying, which in turn triggers minidump uploading.
     */
    @CalledByNative
    private static void triggerMinidumpUploading() {
        handleMinidumpsAndSetMetricsConsent(false /* updateMetricsConsent */);
    }

    /**
     * Trigger minidump uploading, and optionaly also update the metrics-consent value depending on
     * whether the Android Checkbox is toggled on.
     * @param updateMetricsConsent whether to update the metrics-consent value to represent the
     * Android Checkbox toggle.
     */
    public static void handleMinidumpsAndSetMetricsConsent(final boolean updateMetricsConsent) {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped(
                     "AwBrowserProcess.handleMinidumpsAndSetMetricsConsent")) {
            final boolean enableMinidumpUploadingForTesting = CommandLine.getInstance().hasSwitch(
                    CommandLineUtil.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH);
            if (enableMinidumpUploadingForTesting) {
                handleMinidumps(true /* enabled */);
            }

            PlatformServiceBridge.getInstance().queryMetricsSetting(enabled -> {
                ThreadUtils.assertOnUiThread();
                if (updateMetricsConsent) {
                    AwMetricsServiceClient.setConsentSetting(
                            ContextUtils.getApplicationContext(), enabled);
                }

                if (!enableMinidumpUploadingForTesting) {
                    handleMinidumps(enabled);
                }
            });
        }
    }

    /**
     * Make a list of crash key-value pairs for in the same order as minidump file array.
     * These crash key-value pairs are passed from native-code while generating the minidump files.
     * This is basically reordering the crash-key maps in @code{crashesInfo} in the same order as
     * minidump files, ignoring crashkeys for files that are not in the @code{minidumps} array and
     * null for minidumps that don't have crash key-value maps in @code{crashesInfo}.
     *
     * @param minidumps array of minidump files to get crash-keys for.
     * @param crashesInfo crash key-value pairs grouped/mapped by crash report uuid.
     * @return list of crash key-value pairs map corresponding for each minidumps file.
     */
    private static List<Map<String, String>> getCrashKeysForCrashFiles(
            File[] minidumps, Map<String, Map<String, String>> crashesInfo) {
        List<Map<String, String>> crashesInfoList = new ArrayList<>(minidumps.length);
        for (int i = 0; i < minidumps.length; i++) {
            String fileName = minidumps[i].getName();
            // crash report uuid is the minidump file name without any extensions.
            int firstDotIndex = fileName.indexOf('.');
            if (firstDotIndex == -1) {
                firstDotIndex = fileName.length();
            }
            String crashUuid = fileName.substring(0, firstDotIndex);
            if (crashesInfo == null) {
                crashesInfoList.add(null);
            } else {
                crashesInfoList.add(crashesInfo.get(crashUuid));
            }
        }
        return crashesInfoList;
    }

    private static void deleteMinidumps(final File[] minidumpFiles) {
        for (File minidump : minidumpFiles) {
            if (!minidump.delete()) {
                Log.w(TAG, "Couldn't delete file " + minidump.getAbsolutePath());
            }
        }
    }

    private static void transmitMinidumps(final File[] minidumpFiles,
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
        final ParcelFileDescriptor[] minidumpFds = new ParcelFileDescriptor[minidumpFiles.length];
        try {
            for (int i = 0; i < minidumpFiles.length; ++i) {
                try {
                    minidumpFds[i] = ParcelFileDescriptor.open(
                            minidumpFiles[i], ParcelFileDescriptor.MODE_READ_ONLY);
                } catch (FileNotFoundException e) {
                    minidumpFds[i] = null; // This is slightly ugly :)
                }
            }
            try {
                List<Map<String, String>> crashesInfoList =
                        getCrashKeysForCrashFiles(minidumpFiles, crashesInfoMap);
                service.transmitCrashes(minidumpFds, crashesInfoList);
            } catch (RemoteException e) {
                // TODO(gsennton): add a UMA metric here to ensure we aren't losing
                // too many minidumps because of this.
            }
        } finally {
            deleteMinidumps(minidumpFiles);
            // Close FDs
            for (int i = 0; i < minidumpFds.length; ++i) {
                try {
                    if (minidumpFds[i] != null) minidumpFds[i].close();
                } catch (IOException e) {
                }
            }
        }
    }

    /**
     * Pass Minidumps to a separate Service declared in the WebView provider package.
     * That Service will copy the Minidumps to its own data directory - at which point we can delete
     * our copies in the app directory.
     * @param userApproved whether we have user consent to upload crash data - if we do, copy the
     * minidumps, if we don't, delete them.
     */
    public static void handleMinidumps(final boolean userApproved) {
        sSequencedTaskRunner.postTask(() -> {
            final Context appContext = ContextUtils.getApplicationContext();
            final File crashSpoolDir = new File(appContext.getCacheDir().getPath(), "WebView");
            if (!crashSpoolDir.isDirectory()) return;
            final CrashFileManager crashFileManager = new CrashFileManager(crashSpoolDir);

            // The lifecycle of a minidump in the app directory is very simple: foo.dmpNNNNN --
            // where NNNNN is a Process ID (PID) -- gets created, and is either deleted or
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

            ServiceConnection connection = new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    // onServiceConnected is called on the UI thread, so punt this back to the
                    // background thread.
                    sSequencedTaskRunner.postTask(() -> {
                        transmitMinidumps(minidumpFiles, crashesInfoMap,
                                ICrashReceiverService.Stub.asInterface(service));
                        appContext.unbindService(this);
                    });
                }

                @Override
                public void onServiceDisconnected(ComponentName className) {}
            };
            if (!appContext.bindService(intent, connection, Context.BIND_AUTO_CREATE)) {
                Log.w(TAG, "Could not bind to Minidump-copying Service " + intent);
            }
        });
    }

    // Do not instantiate this class.
    private AwBrowserProcess() {}
}
