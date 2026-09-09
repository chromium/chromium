// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.services.ICrashReceiverService;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Handles minidump copying, deletion, and uploading to the crash receiver service. */
@JNINamespace("android_webview")
// This class was created by copy pasting code from other old files during a refactor. We need this
// annotation to bypass presubmit error cause by a new class not having nullaway annotations.
@NullUnmarked
public final class AwMinidumpUploader {
    private static final String TAG = "AwMinidumpUploader";

    // To avoid any potential synchronization issues we post all minidump-copying actions to
    // the same sequence to be run serially.
    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    /** Trigger minidump copying, which in turn triggers minidump uploading. */
    @CalledByNative
    public static void triggerMinidumpUploading() {
        handleMinidumpsAndSetMetricsConsent(/* updateMetricsConsent= */ false);
    }

    /**
     * Trigger minidump uploading, and optionally also update the metrics-consent value depending on
     * whether the Android Checkbox is toggled on.
     *
     * @param updateMetricsConsent whether to update the metrics-consent value to represent the
     *     Android Checkbox toggle.
     */
    public static void handleMinidumpsAndSetMetricsConsent(final boolean updateMetricsConsent) {
        try (DualTraceEvent e1 =
                DualTraceEvent.scoped("AwMinidumpUploader.handleMinidumpsAndSetMetricsConsent")) {
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
        List<Map<String, String>> crashInfos = new ArrayList<>(fileCount);
        for (int i = 0; i < fileCount; ++i) {
            File file = minidumpFiles[i];
            ParcelFileDescriptor p = null;
            try {
                p = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
            } catch (IOException e) {
            }
            minidumpFds[i] = p;
            crashInfos.add(crashesInfoMap.get(getCrashUuid(file)));
        }

        try {
            // AIDL does not support arrays of objects, so use a List here.
            service.transmitCrashes(minidumpFds, crashInfos);
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
            intent.setClassName(
                    AwBrowserProcess.getWebViewPackageName(), ServiceNames.CRASH_RECEIVER_SERVICE);

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

    private AwMinidumpUploader() {}
}
