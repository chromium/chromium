// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.webapk.shell_apk.HostBrowserUtils.PackageNameAndComponentName;

/** UI-less activity which launches host browser. */
public class TransparentLauncherActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        long activityStartTimeMs = SystemClock.elapsedRealtime();
        super.onCreate(savedInstanceState);

        new LaunchHostBrowserSelector(this)
                .selectHostBrowser(
                        new LaunchHostBrowserSelector.Callback() {
                            @Override
                            public void onBrowserSelected(
                                    @Nullable
                                            PackageNameAndComponentName
                                                    hostBrowserPackageNameAndComponentName,
                                    boolean dialogShown) {
                                if (hostBrowserPackageNameAndComponentName == null) {
                                    finish();
                                    return;
                                }
                                HostBrowserLauncherParams params =
                                        HostBrowserLauncherParams.createForIntent(
                                                TransparentLauncherActivity.this,
                                                getIntent(),
                                                hostBrowserPackageNameAndComponentName,
                                                dialogShown,
                                                activityStartTimeMs,
                                                /* splashShownTimeMs= */ -1);

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
