// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.ComponentName;
import android.content.Context;

import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.TransparentHostBrowserLauncherActivity;

/**
 * Handles android.intent.action.MAIN intents if the host browser does not support "showing a
 * transparent window in WebAPK mode till the URL has been loaded".
 */
public class H2OMainActivity extends TransparentHostBrowserLauncherActivity {
    @Override
    protected void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params == null) {
            finish();
            return;
        }

        Context appContext = getApplicationContext();
        HostBrowserLauncher.launch(appContext, params);

        if (H2OLauncher.shouldMainIntentLaunchSplashActivity(params)) {
            H2OLauncher.changeEnabledComponentsAndKillShellApk(appContext,
                    new ComponentName(appContext, SplashActivity.class), getComponentName());
        }

        finish();
    }
}
