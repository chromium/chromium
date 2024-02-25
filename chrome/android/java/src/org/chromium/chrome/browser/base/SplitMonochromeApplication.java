// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;

import org.chromium.android_webview.nonembedded.WebViewApkApplication;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.content_public.browser.ChildProcessCreationParams;

/**
 * Application class to use for Monochrome when //chrome code is in an isolated split. See {@link
 * SplitChromeApplication} for more info.
 */
public class SplitMonochromeApplication extends SplitChromeApplication {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.MonochromeApplicationImpl";

    private static class NonBrowserMonochromeApplication extends Impl {
        @Override
        public void onCreate() {
            super.onCreate();
            if (getApplication().isWebViewProcess()) {
                WebViewApkApplication.checkForAppRecovery();
            }
            if (!VersionInfo.isStableBuild() && getApplication().isWebViewProcess()) {
                WebViewApkApplication.postDeveloperUiLauncherIconTask();
            }
        }
    }

    public SplitMonochromeApplication() {
        super(sImplClassName);
    }

    @Override
    public void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        initializeMonochromeProcessCommon(getPackageName());
    }

    @Override
    protected Impl createNonBrowserApplication() {
        return new NonBrowserMonochromeApplication();
    }

    public static void initializeMonochromeProcessCommon(String packageName) {
        WebViewApkApplication.maybeInitProcessGlobals();

        // ChildProcessCreationParams is only needed for browser process, though it is
        // created and set in all processes. We must set isExternalService to true for
        // Monochrome because Monochrome's renderer services are shared with WebView
        // and are external, and will fail to bind otherwise.
        boolean bindToCaller = false;
        boolean ignoreVisibilityForImportance = false;
        ChildProcessCreationParams.set(
                packageName,
                /* privilegedServicesName= */ null,
                packageName,
                /* sandboxedServicesName= */ null,
                /* isExternalService= */ true,
                LibraryProcessType.PROCESS_CHILD,
                bindToCaller,
                ignoreVisibilityForImportance);
    }

    @Override
    public boolean isWebViewProcess() {
        return WebViewApkApplication.isWebViewProcess();
    }
}
