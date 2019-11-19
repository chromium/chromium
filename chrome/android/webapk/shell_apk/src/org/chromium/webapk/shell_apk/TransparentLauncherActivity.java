// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;

/**
 * UI-less activity which launches host browser.
 */
public class TransparentLauncherActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        long activityStartTimeMs = SystemClock.elapsedRealtime();
        super.onCreate(savedInstanceState);

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
                                        TransparentLauncherActivity.this, getIntent(),
                                        hostBrowserPackageName, dialogShown, activityStartTimeMs,
                                        -1 /* splashShownTimeMs */);

                        onHostBrowserSelected(params);
                        finish();
                    }
                });
    }

    protected void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params != null) {
            WebApkUtils.grantUriPermissionToHostBrowserIfShare(getApplicationContext(), params);
            HostBrowserLauncher.launch(this, params);
        }
    }
}
