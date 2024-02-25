// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.android_webview.nonembedded.WebViewApkApplication;
import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.base.SplitCompatApplication;

/**
 * This is Application class for Monochrome.
 *
 * You shouldn't add anything else in this file, this class is split off from
 * normal chrome in order to access Android system API through Android WebView
 * glue layer and have monochrome specific code.
 *
 * This class is NOT used by Trichrome. Do not add anything here which is only
 * related to Monochrome's minimum SDK level or APK packaging decisions, because
 * those are likely to apply to Trichrome as well - this must only be used for
 * things specific to functioning as a WebView implementation.
 */
public class MonochromeApplicationImpl extends ChromeApplicationImpl {
    public MonochromeApplicationImpl() {}

    @Override
    public void onCreate() {
        super.onCreate();
        if (!VersionInfo.isStableBuild()) {
            // Performing Monochrome WebView DevTools Launcher icon showing/hiding logic in
            // onCreate rather than in attachBaseContext() because it depends on application
            // context being initiatied.
            if (getApplication().isWebViewProcess()) {
                // Whenever a monochrome webview process is launched (WebView service or
                // developer UI), post a background task to show/hide the DevTools icon.
                WebViewApkApplication.postDeveloperUiLauncherIconTask();
            } else if (SplitCompatApplication.isBrowserProcess()) {
                // Frequently check current system webview provider and show/hide the icon
                // accordingly by listening to Monochrome browser Activities status (whenever a
                // browser activity comes to the foreground).
                ApplicationStatus.registerStateListenerForAllActivities(
                        (activity, state) -> {
                            if (state == ActivityState.STARTED) {
                                WebViewApkApplication.postDeveloperUiLauncherIconTask();
                            }
                        });
            }
        }
    }
}
