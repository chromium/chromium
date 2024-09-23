// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded;

import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import com.android.webview.chromium.WebViewLibraryPreloader;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.AwLocaleConfig;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.services.NonembeddedSafeModeActionsList;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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
        Log.i(
                TAG,
                "version=%s (%s) minSdkVersion=%s isBundle=%s processName=%s",
                VersionConstants.PRODUCT_VERSION,
                BuildConfig.VERSION_CODE,
                BuildConfig.MIN_SDK_VERSION,
                BuildConfig.IS_BUNDLE,
                ContextUtils.getProcessName());

        ContextUtils.initApplicationContext(this);
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
     * This is also called by MonochromeApplication, so the initialization here will run
     * for those processes regardless of whether the WebView is standalone or Monochrome.
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
     * Post a non-blocking, low priority background task that shows a launcher icon for WebView
     * DevTools if this Monochrome package is the current selected WebView provider for the system
     * otherwise it hides that icon. This works only for Monochrome and shouldn't be used for other
     * WebView providers. Other WebView Providers (Standalone and Trichrome) will always have
     * launcher icons whether they are the current selected providers or not.
     *
     * Should be guarded by process type checks and should only be called if it's a webview process
     * or a browser process.
     */
    public static void postDeveloperUiLauncherIconTask() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    Context context = ContextUtils.getApplicationContext();
                    try {
                        ComponentName devToolsLauncherActivity =
                                new ComponentName(
                                        context,
                                        "org.chromium.android_webview.devui.MonochromeLauncherActivity");
                        int oldIconState =
                                context.getPackageManager()
                                        .getComponentEnabledSetting(devToolsLauncherActivity);

                        // Enable the icon if this is the current WebView provider, otherwise set
                        // the icon back to default (disabled) state.
                        boolean shouldShowIcon =
                                WebViewPackageHelper.isCurrentSystemWebViewImplementation(context);
                        int newIconState =
                                shouldShowIcon
                                        ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                                        : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;

                        if (oldIconState == newIconState) return;

                        context.getPackageManager()
                                .setComponentEnabledSetting(
                                        devToolsLauncherActivity,
                                        newIconState,
                                        PackageManager.DONT_KILL_APP);
                    } catch (IllegalArgumentException e) {
                        // If MonochromeLauncherActivity doesn't exist, Dynamically showing/hiding
                        // DevTools launcher icon is not enabled in this package; e.g when it is a
                        // stable channel.
                    }
                });
    }

    /**
     * Performs minimal native library initialization required when running as a stand-alone APK.
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
