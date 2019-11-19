// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import android.app.Application;
import android.content.Context;

import org.chromium.android_webview.AwLocaleConfig;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.components.embedder_support.application.FontPreloadingWorkaround;
import org.chromium.ui.base.ResourceBundle;

/**
 * Application subclass for SystemWebView and Trichrome.
 *
 * Application subclass is used by renderer processes, services, and content providers that run
 * under the WebView APK's package.
 *
 * None of this code runs in an application which simply uses WebView.
 */
@JNINamespace("android_webview")
public class WebViewApkApplication extends Application {
    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        ContextUtils.initApplicationContext(this);
        maybeInitProcessGlobals();

        // MonochromeApplication has its own locale configuration already, so call this here
        // rather than in maybeInitProcessGlobals.
        ResourceBundle.setAvailablePakLocales(
                new String[] {}, AwLocaleConfig.getWebViewSupportedPakLocales());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        FontPreloadingWorkaround.maybeInstallWorkaround(this);
    }

    /**
     * Initializes globals needed for components that run in the "webview_apk" or "webview_service"
     * process.
     *
     * This is also called by MonochromeApplication, so the initialization here will run
     * for those processes regardless of whether the WebView is standalone or Monochrome.
     */
    public static void maybeInitProcessGlobals() {
        // Either "webview_service", or "webview_apk".
        // "webview_service" is meant to be very light-weight and never load the native library.
        if (ContextUtils.getProcessName().contains(":webview_")) {
            PathUtils.setPrivateDataDirectorySuffix("webview", "WebView");
            CommandLineUtil.initCommandLine();
        }
    }

    /**
     * Performs minimal native library initialization required when running as a stand-alone APK.
     * @return True if the library was loaded, false if running as webview stub.
     */
    static synchronized boolean initializeNative() {
        try {
            if (LibraryLoader.getInstance().isInitialized()) {
                return true;
            }
            LibraryLoader.getInstance().loadNow();
        } catch (Throwable unused) {
            // Happens for WebView Stub. Throws NoClassDefFoundError because of no
            // NativeLibraries.java being generated.
            return false;
        }
        LibraryLoader.getInstance().switchCommandLineForWebView();
        WebViewApkApplicationJni.get().initializePakResources();
        return true;
    }

    @NativeMethods
    interface Natives {
        void initializePakResources();
    }
}
