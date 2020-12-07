// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;

import com.android.webview.chromium.MonochromeLibraryPreloader;

import org.chromium.android_webview.nonembedded.WebViewApkApplication;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.version.ChromeVersionInfo;

/**
 * Application class to use for Monochrome when //chrome code is in an isolated split. See {@link
 * SplitChromeApplication} for more info.
 */
public class SplitMonochromeApplication extends SplitChromeApplication {
    private static class NonBrowserMonochromeApplication extends MainDexApplicationImpl {
        @Override
        public void attachBaseContext(Context context) {
            super.attachBaseContext(context);
            initializeMonochromeProcessCommon();
        }

        @Override
        public void onCreate() {
            super.onCreate();
            // TODO(crbug.com/1126301): This matches logic in MonochromeApplication.java.
            // Deduplicate if chrome split launches.
            if (!ChromeVersionInfo.isStableBuild() && isWebViewProcess()) {
                WebViewApkApplication.postDeveloperUiLauncherIconTask();
            }
        }

        @Override
        public boolean isWebViewProcess() {
            return WebViewApkApplication.isWebViewProcess();
        }
    }

    public SplitMonochromeApplication() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.MonochromeApplication$MonochromeApplicationImpl"));
    }

    @Override
    protected MainDexApplicationImpl createNonBrowserApplication() {
        return new NonBrowserMonochromeApplication();
    }

    public static void initializeMonochromeProcessCommon() {
        WebViewApkApplication.maybeInitProcessGlobals();
        if (!LibraryLoader.getInstance().isLoadedByZygote()) {
            LibraryLoader.getInstance().setNativeLibraryPreloader(new MonochromeLibraryPreloader());
        }
    }
}
