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
    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
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
        // Ensure that we don't try to load the native library until after attachBaseContext, since
        // Monochrome attempts to call loadWebViewNativeLibraryFromPackage, which will fail until
        // ActivityThread has an application set on it, which happens after attachBaseContext
        // finishes. See crbug.com/390730928.
        mPreloadLibraryAttachBaseContext = false;
    }

    @Override
    public void attachBaseContext(Context context) {
        // Preloader has to happen first since we may load the native library in the super's
        // attachBaseContext.
        WebViewApkApplication.maybeSetPreloader();
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
                /* isExternalSandboxedService= */ true,
                LibraryProcessType.PROCESS_CHILD,
                bindToCaller,
                ignoreVisibilityForImportance);
    }

    @Override
    public boolean isWebViewProcess() {
        return WebViewApkApplication.isWebViewProcess();
    }
}
