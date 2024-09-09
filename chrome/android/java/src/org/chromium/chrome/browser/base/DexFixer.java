// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.base;

import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.system.ErrnoException;
import android.system.Os;
import android.system.StructStat;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import dalvik.system.DexFile;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.io.File;
import java.io.IOException;

/** Performs work-arounds for Android bugs which result in invalid or unreadable dex. */
@RequiresApi(Build.VERSION_CODES.O)
public class DexFixer {
    private static final String TAG = "DexFixer";
    private static boolean sHasIsolatedSplits;

    @WorkerThread
    public static void fixDexInBackground() {
        if (shouldSkipDexFix()) {
            return;
        }

        fixDexIfNecessary(Runtime.getRuntime());
    }

    static void setHasIsolatedSplits(boolean value) {
        sHasIsolatedSplits = value;
    }

    static void scheduleDexFix() {
        if (shouldSkipDexFix()) {
            return;
        }

        // Wait until startup completes so this doesn't slow down early startup or mess with
        // compiled dex files before they get loaded initially.
        DeferredStartupHandler.getInstance()
                .addDeferredTask(
                        () -> {
                            // BEST_EFFORT will only affect when the task runs, the dexopt will run
                            // with normal priority (but in a separate process, due to using
                            // Runtime.exec()).
                            PostTask.postTask(
                                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                                    () -> {
                                        fixDexIfNecessary(Runtime.getRuntime());
                                    });
                        });
    }

    @WorkerThread
    @VisibleForTesting
    static @DexFixerReason int fixDexIfNecessary(Runtime runtime) {
        ApplicationInfo appInfo = ContextUtils.getApplicationContext().getApplicationInfo();
        @DexFixerReason int reason = needsDexCompile(appInfo);
        if (reason > DexFixerReason.NOT_NEEDED) {
            Log.w(TAG, "Triggering dex compile. Reason=%d", reason);
            try {
                String cmd = "/system/bin/cmd package compile -r shared ";
                if (reason == DexFixerReason.NOT_READABLE && BuildConfig.IS_BUNDLE) {
                    // Isolated processes need only access the base split.
                    String apkBaseName = new File(appInfo.sourceDir).getName();
                    cmd += String.format("--split %s ", apkBaseName);
                }
                cmd += ContextUtils.getApplicationContext().getPackageName();
                runtime.exec(cmd);
            } catch (IOException e) {
                // Don't crash.
            }
        }
        return reason;
    }

    private static boolean shouldSkipDexFix() {
        ApplicationInfo appInfo = ContextUtils.getApplicationContext().getApplicationInfo();
        // All bugs are fixed after Q.
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.Q) {
            return true;
        }
        // Fixes are never required for system image installs, since we can trust those to be valid
        // and world-readable.
        if ((appInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
            return true;
        }
        // Skip the workaround on local builds to avoid affecting perf bots.
        // https://bugs.chromium.org/p/chromium/issues/detail?id=1160070
        if (VersionInfo.isLocalBuild() && VersionInfo.isOfficialBuild()) {
            return true;
        }
        return false;
    }

    private static String odexPathFromApkPath(String apkPath) {
        // Based on https://cs.android.com/search?q=OatFileAssistant::DexLocationToOdexNames
        String isaName = BuildInfo.getArch();
        // E.g. /data/app/org.chromium.chrome-qtmmjyN79ucfPKm0ZVZMHg==/base.apk
        File apkFile = new File(apkPath);
        String baseName = apkFile.getName();
        baseName = baseName.substring(0, baseName.lastIndexOf('.'));
        return String.format("%s/oat/%s/%s.odex", apkFile.getParent(), isaName, baseName);
    }

    private static @DexFixerReason int needsDexCompile(ApplicationInfo appInfo) {
        // Android O MR1 has a bug where bg-dexopt-job will break optimized dex files for isolated
        // splits. This leads to *very* slow startup on those devices. To mitigate this, we attempt
        // to force a dex compile if necessary.
        if (sHasIsolatedSplits && Build.VERSION.SDK_INT == Build.VERSION_CODES.O_MR1) {
            // If the app has just been updated, it will be compiled with quicken. The next time
            // bg-dexopt-job runs it will break the optimized dex for splits. If we force compile
            // now, then bg-dexopt-job won't mess up the splits, and we save the user a slow
            // startup.
            SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
            long versionCode = BuildConfig.VERSION_CODE;
            // The default value is always lesser than any non-negative versionCode. This prevents
            // some tests from failing when application's versionCode is stuck at 0.
            if (prefManager.readLong(
                            ChromePreferenceKeys.ISOLATED_SPLITS_DEX_COMPILE_VERSION,
                            versionCode - 1)
                    != versionCode) {
                // Compiling the dex is an asynchronous operation anyways, so update the pref here
                // rather than attempting to wait.
                prefManager.writeLong(
                        ChromePreferenceKeys.ISOLATED_SPLITS_DEX_COMPILE_VERSION, versionCode);
                return DexFixerReason.O_MR1_AFTER_UPDATE;
            }

            // Check for corrupt dex.
            String[] splitNames = appInfo.splitNames;
            if (splitNames != null) {
                for (int i = 0; i < splitNames.length; i++) {
                    // Ignore config splits like "config.en".
                    if (splitNames[i].contains(".")) {
                        continue;
                    }
                    try {
                        if (DexFile.isDexOptNeeded(appInfo.splitSourceDirs[i])) {
                            return DexFixerReason.O_MR1_CORRUPTED;
                        }
                    } catch (IOException e) {
                        return DexFixerReason.O_MR1_IO_EXCEPTION;
                    }
                }
            }
        }

        String oatPath = odexPathFromApkPath(appInfo.sourceDir);
        try {
            StructStat st = Os.stat(oatPath);
            if ((st.st_mode & 0007) == 0) {
                return DexFixerReason.NOT_READABLE;
            }
        } catch (ErrnoException e) {
            return DexFixerReason.STAT_FAILED;
        }
        return DexFixerReason.NOT_NEEDED;
    }
}
