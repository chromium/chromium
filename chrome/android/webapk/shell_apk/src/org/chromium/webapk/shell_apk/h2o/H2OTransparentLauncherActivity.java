// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.ComponentName;
import android.content.Context;

import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.HostBrowserUtils;
import org.chromium.webapk.shell_apk.TransparentLauncherActivity;
import org.chromium.webapk.shell_apk.WebApkUtils;

/**
 * UI-less activity which launches host browser. Relaunches itself if the android.intent.action.MAIN
 * intent handler needs to be switched.
 */
public class H2OTransparentLauncherActivity extends TransparentLauncherActivity {
    @Override
    protected void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params == null) {
            return;
        }

        WebApkUtils.grantUriPermissionToHostBrowserIfShare(getApplicationContext(), params);

        boolean shouldLaunchSplash = HostBrowserUtils.shouldIntentLaunchSplashActivity(params);
        if (relaunchIfNeeded(params, shouldLaunchSplash)) {
            return;
        }

        if (shouldLaunchSplash) {
            // Launch {@link SplashActivity} first instead of directly launching the host
            // browser so that for a WebAPK launched via {@link
            // H2OTransparentHostBrowserLauncherActivity}, tapping the app icon
            // brings the WebAPK activity stack to the foreground and does not create a
            // new activity stack.
            Context appContext = getApplicationContext();
            H2OLauncher.copyIntentExtrasAndLaunch(
                    appContext,
                    getIntent(),
                    params.getSelectedShareTargetActivityClassName(),
                    params.getLaunchTimeMs(),
                    new ComponentName(appContext, SplashActivity.class));
            return;
        }

        HostBrowserLauncher.launch(this, params);
    }

    /**
     * Launches the android.intent.action.MAIN intent handler if the activity which currently
     * handles the android.intent.action.MAIN intent needs to be switched. Launching the main intent
     * handler is done to minimize the number of places which enable/disable components.
     */
    private boolean relaunchIfNeeded(HostBrowserLauncherParams params, boolean shouldLaunchSplash) {
        Context appContext = getApplicationContext();

        // {@link H2OLauncher#changeEnabledComponentsAndKillShellApk()} enables one
        // component, THEN disables the other. Relaunch if the wrong component is disabled (vs
        // if the wrong component is enabled) to handle the case where both components are enabled.
        ComponentName relaunchComponent = null;
        if (shouldLaunchSplash) {
            // Relaunch if H2OOpaqueMainActivity is disabled.
            if (!H2OOpaqueMainActivity.checkComponentEnabled(
                    appContext, params.isNewStyleWebApk())) {
                relaunchComponent = new ComponentName(appContext, H2OMainActivity.class);
            }
        } else {
            // Relaunch if H2OMainActivity is disabled.
            if (!H2OMainActivity.checkComponentEnabled(appContext, params.isNewStyleWebApk())) {
                relaunchComponent = new ComponentName(appContext, SplashActivity.class);
            }
        }

        if (relaunchComponent == null) {
            return false;
        }

        H2OLauncher.copyIntentExtrasAndLaunch(
                getApplicationContext(),
                getIntent(),
                params.getSelectedShareTargetActivityClassName(),
                /* launchTimeMs= */ -1,
                relaunchComponent);
        return true;
    }
}
