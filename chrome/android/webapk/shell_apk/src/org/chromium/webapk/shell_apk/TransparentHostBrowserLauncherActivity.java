// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

/**
 * UI-less activity which launches host browser.
 */
public class TransparentHostBrowserLauncherActivity extends HostBrowserLauncherActivity {
    @Override
    protected void showSplashScreen() {}

    @Override
    protected void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params == null) {
            finish();
            return;
        }
        HostBrowserLauncher.launch(getApplicationContext(), params);
        finish();
    }
}
