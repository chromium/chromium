// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;

/** Contains methods for launching host browser where ShellAPK shows the splash screen. */
public class H2OLauncher {
    // Lowest version of Chromium which supports ShellAPK showing the splash screen.
    private static final int MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH = Integer.MAX_VALUE;

    private static final String TAG = "cr_H2OLauncher";

    /**
     * Returns whether the main intent should launch SplashActivity.class for the given host browser
     * params.
     */
    public static boolean shouldMainIntentLaunchSplashActivity(HostBrowserLauncherParams params) {
        return params.getHostBrowserMajorChromiumVersion()
                >= MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
    }

    /**
     * Changes which components are enabled.
     *
     * @param context
     * @param enableComponent Component to enable.
     * @param disableComponent Component to disable.
     */
    public static void changeEnabledComponentsAndKillShellApk(
            Context context, ComponentName enableComponent, ComponentName disableComponent) {
        PackageManager pm = context.getPackageManager();
        // The state change takes seconds if we do not let PackageManager kill the ShellAPK.
        pm.setComponentEnabledSetting(enableComponent,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
        pm.setComponentEnabledSetting(
                disableComponent, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0);
    }

    /** Launches the host browser in WebAPK-transparent-splashscreen mode. */
    public static void launch(Activity splashActivity, HostBrowserLauncherParams params) {
        Log.v(TAG, "WebAPK Launch URL: " + params.getStartUrl());

        Intent intent = HostBrowserLauncher.createLaunchInWebApkModeIntent(
                splashActivity.getApplicationContext(), params);
        intent.putExtra(WebApkConstants.EXTRA_USE_TRANSPARENT_SPLASH, true);

        // Clear Intent.FLAG_ACTIVITY_NEW_TASK flag set by
        // {@link HostBrowserLauncher#createLaunchInWebApkModeIntent()}.
        intent.setFlags(0);

        try {
            splashActivity.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            e.printStackTrace();
        }
    }
}
