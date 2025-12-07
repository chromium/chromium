// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.base;

import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.system.ErrnoException;
import android.system.Os;
import android.system.StructStat;

import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.DeferredStartupHandler;

import java.io.File;
import java.io.IOException;

/** Performs work-arounds for Android bugs which result in invalid or unreadable dex. */
@NullMarked
public class DexFixer {
    private static final String TAG = "DexFixer";

    @WorkerThread
    public static void fixDexInBackground() {
        if (shouldSkipDexFix()) {
            return;
        }

        fixDexIfNecessary(Runtime.getRuntime());
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
                StringBuilder cmdBuilder =
                        new StringBuilder("/system/bin/cmd package compile -r shared ");
                if (reason == DexFixerReason.NOT_READABLE && BundleUtils.hasAnyInstalledSplits()) {
                    // Isolated processes need only access the base split.
                    String apkBaseName = new File(appInfo.sourceDir).getName();
                    cmdBuilder.append("--split ").append(apkBaseName).append(" ");
                }
                cmdBuilder.append(ContextUtils.getApplicationContext().getPackageName());
                runtime.exec(cmdBuilder.toString());
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
        String isaName = DeviceInfo.getArch();
        // E.g. /data/app/org.chromium.chrome-qtmmjyN79ucfPKm0ZVZMHg==/base.apk
        File apkFile = new File(apkPath);
        String baseName = apkFile.getName();
        baseName = baseName.substring(0, baseName.lastIndexOf('.'));
        return String.format("%s/oat/%s/%s.odex", apkFile.getParent(), isaName, baseName);
    }

    private static @DexFixerReason int needsDexCompile(ApplicationInfo appInfo) {
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
