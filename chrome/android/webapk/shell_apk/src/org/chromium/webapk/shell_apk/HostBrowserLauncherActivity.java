// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;

/**
 * Base class for activity which launches host browser.
 */
public abstract class HostBrowserLauncherActivity extends Activity {
    private long mActivityStartTimeMs;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mActivityStartTimeMs = SystemClock.elapsedRealtime();
        super.onCreate(savedInstanceState);

        showSplashScreen();

        new LaunchHostBrowserSelector(this).selectHostBrowser(
                new LaunchHostBrowserSelector.Callback() {
                    @Override
                    public void onBrowserSelected(
                            String hostBrowserPackageName, boolean dialogShown) {
                        if (hostBrowserPackageName == null) {
                            finish();
                            return;
                        }
                        HostBrowserLauncherParams params =
                                HostBrowserLauncherParams.createForIntent(
                                        HostBrowserLauncherActivity.this, getIntent(),
                                        hostBrowserPackageName, dialogShown, mActivityStartTimeMs);
                        onHostBrowserSelected(params);
                    }
                });
    }

    protected abstract void showSplashScreen();

    protected abstract void onHostBrowserSelected(HostBrowserLauncherParams params);
}
