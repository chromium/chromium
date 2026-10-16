// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.WebApkSharedPreferences;
import org.chromium.webapk.shell_apk.WebApkUtils;

import java.util.List;

/** Contains methods for launching host browser where ShellAPK shows the splash screen. */
@NullMarked
public class H2OLauncher {
    private static final String TAG = "cr_H2OLauncher";

    /** Returns whether the WebAPK requested a relaunch within the last {@link deltaMs}. */
    public static boolean didRequestRelaunchFromHostBrowserWithinLastMs(
            Context context, long deltaMs) {
        SharedPreferences sharedPrefs = WebApkSharedPreferences.getPrefs(context);
        long now = System.currentTimeMillis();
        long lastRequestTimestamp =
                sharedPrefs.getLong(
                        WebApkSharedPreferences.PREF_REQUEST_HOST_BROWSER_RELAUNCH_TIMESTAMP, -1);
        return (now - lastRequestTimestamp) <= deltaMs;
    }

    /**
     * Changes which components are enabled.
     *
     * @param enableComponent Component to enable.
     * @param disableComponent Component to disable.
     */
    public static void changeEnabledComponentsAndKillShellApk(
            Context context, ComponentName enableComponent, ComponentName disableComponent) {
        // Force any pending SharedPreference writes to occur.
        WebApkSharedPreferences.flushPendingWrites(context);

        PackageManager pm = context.getPackageManager();
        // The state change takes seconds if we do not let PackageManager kill the ShellAPK.
        pm.setComponentEnabledSetting(
                enableComponent,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                PackageManager.DONT_KILL_APP);
        pm.setComponentEnabledSetting(
                disableComponent, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0);
    }

    /** Launches the host browser in WebAPK-transparent-splashscreen mode. */
    public static void launch(Activity splashActivity, HostBrowserLauncherParams params) {
        Log.v(TAG, "WebAPK Launch URL: " + params.getStartUrl());

        Bundle extraExtras = new Bundle();
        extraExtras.putBoolean(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);
        HostBrowserLauncher.launchBrowserInWebApkModeIfSupported(
                splashActivity,
                params,
                extraExtras,
                Intent.FLAG_ACTIVITY_NO_ANIMATION,
                /* expectResult= */ true);
    }

    /**
     * Launches the given component, passing extras from the given intent.
     *
     * <p>For shares, pass the read grant on to the next activity. The grant from the sharing app is
     * owned by the activity that received the share, which finishes right after this hop. Chrome
     * verifies that the WebAPK uid still holds a read grant when it launches the web app
     * (checkUriPermission on Android 14 and below; on 15+ the ComponentCaller snapshot taken when
     * SplashActivity starts Chrome), so an activity that is still alive at that point must own one.
     * See crbug.com/561685220.
     *
     * @param intentToCopy Intent whose extras should be copied.
     * @param selectedShareTargetActivity Class name of the share activity that the user selected.
     * @param launchTimeMs Timestamp of when WebAPK's initial activity was launched. -1 if the time
     *     is unknown.
     * @param launchComponent Component to launch.
     */
    public static void copyIntentExtrasAndLaunch(
            Context context,
            Intent intentToCopy,
            @Nullable String selectedShareTargetActivity,
            long launchTimeMs,
            ComponentName launchComponent) {
        Intent intent = new Intent(Intent.ACTION_VIEW, intentToCopy.getData());
        intent.setComponent(launchComponent);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Bundle copiedExtras = intentToCopy.getExtras();
        if (copiedExtras != null) {
            intent.putExtras(copiedExtras);
        }

        // If the intent is a share, propagate which share target activity the user selected.
        if (selectedShareTargetActivity != null) {
            intent.putExtra(
                    WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                    selectedShareTargetActivity);
        }

        if (launchTimeMs != -1) {
            intent.putExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, launchTimeMs);
        }

        // For shares, pass the read grant on to the next activity. The grant from the sharing app
        // is tied to the receiving activity.
        boolean attachedSharedUris = false;
        if (selectedShareTargetActivity != null) {
            List<Uri> readableUris = WebApkUtils.getReadableSharedUris(context, intentToCopy);
            WebApkUtils.attachUrisWithReadGrant(intent, readableUris);
            attachedSharedUris = !readableUris.isEmpty();
        }

        // The platform throws SecurityException if access to a shared URI is revoked between the
        // check above and this launch, in which case it fallbacks to launching again without the
        // grants. Any other failure would recur on a second launch and should propagate as it did
        // before grants were attached.
        try {
            context.startActivity(intent);
        } catch (SecurityException e) {
            if (!attachedSharedUris) throw e;
            Log.w(TAG, "Unable to pass shared URI grants along, launching without them.", e);
            WebApkUtils.detachUrisWithReadGrant(intent);
            context.startActivity(intent);
        }
    }

    /** Sends intent to host browser to request that the host browser relaunch the WebAPK. */
    public static void requestRelaunchFromHostBrowser(
            Activity activity, HostBrowserLauncherParams params) {
        long timestamp = System.currentTimeMillis();
        SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(activity).edit();
        editor.putLong(
                WebApkSharedPreferences.PREF_REQUEST_HOST_BROWSER_RELAUNCH_TIMESTAMP, timestamp);
        editor.commit();

        Bundle extraExtras = new Bundle();
        extraExtras.putBoolean(WebApkConstants.EXTRA_RELAUNCH, true);
        HostBrowserLauncher.launchBrowserInWebApkModeIfSupported(
                activity,
                params,
                extraExtras,
                Intent.FLAG_ACTIVITY_NEW_TASK,
                /* expectResult= */ false);
    }
}
