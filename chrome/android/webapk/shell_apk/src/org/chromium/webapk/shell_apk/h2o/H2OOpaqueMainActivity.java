// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.SystemClock;

/**
 * Launches {@link SplashActivity}. SplashActivity does not handle android.intent.action.MAIN
 * because when the root activity is singleTask and the root activity handles
 * android.intent.action.MAIN, Android destroys any activities which are stacked above the
 * singleTask activity when a user taps the app icon in the app drawer. This bad behavior does not
 * occur if a non-root activity is singleTask.
 */
public class H2OOpaqueMainActivity extends Activity {
    /** Returns whether {@link InitialSplashActivity} is enabled. */
    public static boolean checkComponentEnabled(Context context) {
        PackageManager pm = context.getPackageManager();
        ComponentName component = new ComponentName(context, H2OOpaqueMainActivity.class);
        int enabledSetting = pm.getComponentEnabledSetting(component);
        // Component is enabled by default.
        return enabledSetting == PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                || enabledSetting == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        final long launchTimeMs = SystemClock.elapsedRealtime();
        super.onCreate(savedInstanceState);
        Context appContext = getApplicationContext();
        overridePendingTransition(0, 0);
        H2OLauncher.copyIntentExtrasAndLaunch(appContext, getIntent(), null, launchTimeMs,
                new ComponentName(appContext, SplashActivity.class));
        finish();
    }
}
