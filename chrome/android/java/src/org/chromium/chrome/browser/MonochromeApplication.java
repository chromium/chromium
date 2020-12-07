// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import org.chromium.android_webview.nonembedded.WebViewApkApplication;
import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.base.SplitMonochromeApplication;
import org.chromium.chrome.browser.version.ChromeVersionInfo;

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
public class MonochromeApplication extends ChromeApplication {
    public MonochromeApplication() {
        super(new MonochromeApplicationImpl());
    }

    /** Monochrome application logic. */
    public static class MonochromeApplicationImpl extends ChromeApplicationImpl {
        public MonochromeApplicationImpl() {}

        @Override
        public void onCreate() {
            super.onCreate();
            if (!ChromeVersionInfo.isStableBuild()) {
                // Performing Monochrome WebView DevTools Launcher icon showing/hiding logic in
                // onCreate rather than in attachBaseContext() because it depends on application
                // context being initiatied.
                if (getApplication().isWebViewProcess()) {
                    // Whenever a monochrome webview process is launched (WebView service or
                    // developer UI), post a background task to show/hide the DevTools icon.
                    WebViewApkApplication.postDeveloperUiLauncherIconTask();
                } else if (isBrowserProcess()) {
                    // Frequently check current system webview provider and show/hide the icon
                    // accordingly by listening to Monochrome browser Activities status (whenever a
                    // browser activity comes to the foreground).
                    ApplicationStatus.registerStateListenerForAllActivities((activity, state) -> {
                        if (state == ActivityState.STARTED) {
                            WebViewApkApplication.postDeveloperUiLauncherIconTask();
                        }
                    });
                }
            }
        }
    }

    @Override
    public void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        SplitMonochromeApplication.initializeMonochromeProcessCommon(getPackageName());
    }

    @Override
    public boolean isWebViewProcess() {
        return WebViewApkApplication.isWebViewProcess();
    }
}
