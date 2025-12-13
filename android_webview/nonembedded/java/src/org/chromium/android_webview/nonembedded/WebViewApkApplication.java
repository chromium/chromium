// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import android.app.Application;
import android.content.Context;

import com.android.webview.chromium.WebViewLibraryPreloader;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.AwLocaleConfig;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.services.NonembeddedSafeModeActionsList;
import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.build.BuildConfig;
import org.chromium.components.crash.CustomAssertionHandler;
import org.chromium.components.crash.PureJavaExceptionHandler;
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
    private static final String TAG = "WebViewApkApp";

    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    //
    // Logic here is specific for standalone WebView and trichrome but does not run by
    // Monochrome. Common logic between all WebView flavours should be go into
    // maybeInitProcessGlobals instead which is called by Monochrome too.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        ContextUtils.initApplicationContext(this);

        Log.i(
                TAG,
                "version=%s (%s) minSdkVersion=%s processName=%s splits=%s",
                VersionConstants.PRODUCT_VERSION,
                BuildConfig.VERSION_CODE,
                BuildConfig.MIN_SDK_VERSION,
                ContextUtils.getProcessName(),
                // BundleUtils uses getApplicationContext, so logging after we init it.
                BundleUtils.getInstalledSplitNamesForLogging());

        maybeSetPreloader();
        maybeInitProcessGlobals();

        // MonochromeApplication has its own locale configuration already, so call this here
        // rather than in maybeInitProcessGlobals.
        ResourceBundle.setAvailablePakLocales(AwLocaleConfig.getWebViewSupportedPakLocales());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        checkForAppRecovery();
        FontPreloadingWorkaround.maybeInstallWorkaround(this);
    }

    public static void checkForAppRecovery() {
        if (ContextUtils.getProcessName().contains(":webview_service")) {
            PlatformServiceBridge.getInstance().checkForAppRecovery();
        }
    }

    /**
     * Initializes globals needed for components that run in the "webview_apk" or "webview_service"
     * process.
     *
     * <p>This is also called by MonochromeApplication, so the initialization here will run for
     * those processes regardless of whether the WebView is standalone or Monochrome.
     */
    public static void maybeInitProcessGlobals() {
        if (isWebViewProcess()) {
            PathUtils.setPrivateDataDirectorySuffix("webview", "WebView");
            CommandLineUtil.initCommandLine();

            PureJavaExceptionHandler.installHandler(AwPureJavaExceptionReporter::new);
            CustomAssertionHandler.installPreNativeHandler(AwPureJavaExceptionReporter::new);

            // TODO(crbug.com/40751605): Do set up a native UMA recorder once we support recording
            // metrics from native nonembedded code.
            UmaRecorderHolder.setUpNativeUmaRecorder(false);

            UmaRecorderHolder.setNonNativeDelegate(new AwNonembeddedUmaRecorder());

            // Only register nonembedded SafeMode actions for webview_apk or webview_service
            // processes.
            SafeModeController controller = SafeModeController.getInstance();
            controller.registerActions(NonembeddedSafeModeActionsList.sList);
        }
    }

    /**
     * Sets the native library preloader.
     *
     * <p>This is also called by MonochromeApplication, so the initialization here will run for
     * those processes regardless of whether the WebView is standalone or Monochrome.
     */
    public static void maybeSetPreloader() {
        if (!LibraryLoader.getInstance().isLoadedByZygote()) {
            LibraryLoader.getInstance().setNativeLibraryPreloader(new WebViewLibraryPreloader());
        }
    }

    // Returns true if running in the "webview_apk" or "webview_service" process.
    public static boolean isWebViewProcess() {
        // Either "webview_service", or "webview_apk".
        // "webview_service" is meant to be very light-weight and never load the native library.
        return ContextUtils.getProcessName().contains(":webview_");
    }

    /**
     * Performs minimal native library initialization required when running as a stand-alone APK.
     *
     * @return True if the library was loaded, false if running as webview stub.
     */
    static synchronized boolean ensureNativeInitialized() {
        assert ThreadUtils.runningOnUiThread()
                : "WebViewApkApplication#ensureNativeInitialized should only be called on the"
                        + " UIThread";
        try {
            if (LibraryLoader.getInstance().isInitialized()) {
                return true;
            }
            // Should not call LibraryLoader.initialize() since this will reset UmaRecorder
            // delegate.
            LibraryLoader.getInstance()
                    .setLibraryProcessType(LibraryProcessType.PROCESS_WEBVIEW_NONEMBEDDED);
            LibraryLoader.getInstance().ensureInitialized();
            LibraryLoader.getInstance().switchCommandLineForWebView();
            WebViewApkApplicationJni.get().initializeGlobalsAndResources();
            return true;
        } catch (Throwable unused) {
            // Happens for WebView Stub. Throws NoClassDefFoundError because of no
            // NativeLibraries.java being generated.
            return false;
        }
    }

    @NativeMethods
    interface Natives {
        void initializeGlobalsAndResources();
    }
}
