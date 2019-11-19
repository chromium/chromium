// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.multidex;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.support.multidex.MultiDex;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.MainDex;

/**
 *  Performs multidex installation for non-isolated processes.
 */
@MainDex
public class ChromiumMultiDexInstaller {
    private static final String TAG = "base_multidex";

    /**
     * Suffix for the meta-data tag in the AndroidManifext.xml that determines whether loading
     * secondary dexes should be skipped for a given process name.
     */
    private static final String IGNORE_MULTIDEX_KEY = ".ignore_multidex";

    /**
     *  Installs secondary dexes if possible/necessary.
     *
     *  Isolated processes (e.g. renderer processes) can't load secondary dex files on
     *  K and below, so we don't even try in that case.
     *
     *  In release builds of app apks (as opposed to test apks), this is a no-op because:
     *    - multidex isn't necessary in release builds because we run proguard there and
     *      thus aren't threatening to hit the dex limit; and
     *    - calling MultiDex.install, even in the absence of secondary dexes, causes a
     *      significant regression in start-up time (crbug.com/525695).
     *
     *  @param context The application context.
     */
    @VisibleForTesting
    public static void install(Context context) {
        // No-op on platforms that support multidex natively.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return;
        }
        // TODO(jbudorick): Back out this version check once support for K & below works.
        // http://crbug.com/512357
        if (!shouldInstallMultiDex(context)) {
            Log.i(TAG, "Skipping multidex installation: not needed for process.");
        } else {
            MultiDex.install(context);
            Log.i(TAG, "Completed multidex installation.");
        }
    }

    // Determines whether MultiDex should be installed for the current process.  Isolated
    // Processes should skip MultiDex as they can not actually access the files on disk.
    // Privileged processes need ot have all of their dependencies in the MainDex for
    // performance reasons.
    private static boolean shouldInstallMultiDex(Context context) {
        if (ContextUtils.isIsolatedProcess()) {
            return false;
        }
        String currentProcessName = ContextUtils.getProcessName();
        PackageManager packageManager = context.getPackageManager();
        try {
            ApplicationInfo appInfo = packageManager.getApplicationInfo(context.getPackageName(),
                    PackageManager.GET_META_DATA);
            if (appInfo == null || appInfo.metaData == null) return true;
            return !appInfo.metaData.getBoolean(currentProcessName + IGNORE_MULTIDEX_KEY, false);
        } catch (PackageManager.NameNotFoundException e) {
            return true;
        }
    }

}
