// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;

/** Contains methods for launching host browser. */
public class HostBrowserLauncher {
    private static final String TAG = "cr_HostBrowserLauncher";

    // Action for launching {@link WebappLauncherActivity}.
    // TODO(hanxi): crbug.com/737556. Replaces this string with the new WebAPK launch action after
    // it is propagated to all the Chrome's channels.
    public static final String ACTION_START_WEBAPK =
            "com.google.android.apps.chrome.webapps.WebappManager.ACTION_START_WEBAPP";

    // Must stay in sync with {@link
    // org.chromium.chrome.browser.ShortcutHelper#REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB}.
    private static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";

    /**
     * Launches host browser in WebAPK mode if the browser is WebAPK-compatible. Otherwise, launches
     * the host browser in tabbed mode.
     */
    public static void launch(Activity activity, HostBrowserLauncherParams params) {
        launchBrowserInWebApkModeIfSupported(
                activity, params, null, Intent.FLAG_ACTIVITY_NEW_TASK, /* expectResult= */ false);
    }

    /**
     * Launches host browser in WebAPK mode if the WebAPK is bound to a host browser via its
     * AndroidManifest. Otherwise, launches a VIEW intent to the default browser, which will launch
     * in WebAPK mode if it supports WebAPKs.
     */
    public static void launchBrowserInWebApkModeIfSupported(
            Activity activity,
            HostBrowserLauncherParams params,
            Bundle extraExtras,
            int flags,
            boolean expectResult) {
        ManageDataLauncherActivity.updateSiteSettingsShortcut(
                activity.getApplicationContext(), params);
        Intent intent;
        if (HostBrowserUtils.isHostBrowserFromManifest(
                activity.getApplicationContext(), params.getHostBrowserPackageName())) {
            intent = new Intent();
            intent.setAction(ACTION_START_WEBAPK);
        } else {
            intent = new Intent(Intent.ACTION_VIEW, Uri.parse(params.getStartUrl()));
        }
        intent.setPackage(params.getHostBrowserPackageName());
        intent.setFlags(flags);

        Bundle copiedExtras = params.getOriginalIntent().getExtras();
        if (copiedExtras != null) {
            intent.putExtras(copiedExtras);
        }

        // {@link WebApkConstants.EXTRA_RELAUNCH} causes the browser the relaunch the WebAPK. Avoid
        // an infinite relaunch loop by explicity removing the extra and adding it back only if it
        // is in {@link extraExtras}.
        intent.removeExtra(WebApkConstants.EXTRA_RELAUNCH);

        intent.putExtra(WebApkConstants.EXTRA_URL, params.getStartUrl())
                .putExtra(WebApkConstants.EXTRA_SOURCE, params.getSource())
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, activity.getPackageName())
                .putExtra(
                        WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                        params.getSelectedShareTargetActivityClassName())
                .putExtra(WebApkConstants.EXTRA_FORCE_NAVIGATION, params.getForceNavigation());

        if (extraExtras != null) {
            intent.putExtras(extraExtras);
        }

        // Only pass on the start time if:
        // - The WebAPK is not already running.
        // - No user action was required between launching the webapk and chrome starting up.
        //   See https://crbug.com/842023
        if (!params.wasDialogShown() && params.getLaunchTimeMs() >= 0) {
            intent.putExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, params.getLaunchTimeMs());
        }

        if (params.getSplashShownTimeMs() >= 0) {
            intent.putExtra(
                    WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME,
                    params.getSplashShownTimeMs());
        }

        try {
            if (expectResult) {
                // requestCode is arbitrary.
                activity.startActivityForResult(intent, 0);
            } else {
                activity.startActivity(intent);
            }
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            e.printStackTrace();
        }
    }
}
