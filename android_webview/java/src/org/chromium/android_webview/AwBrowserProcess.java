// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.StrictMode;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.accessibility.AwAccessibilityStateVisibilityManager;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ChildProcessCreationParams;

/** Wrapper for the steps needed to initialize the java and native sides of webview chromium. */
@JNINamespace("android_webview")
@Lifetime.Singleton
public final class AwBrowserProcess {
    private static final String WEBVIEW_DIR_BASENAME = "webview";

    private static String sWebViewPackageName;
    private static @Nullable String sProcessDataDirSuffix;
    private static boolean sDataDirBasePathOverridden;

    /**
     * Loads the native library, and performs basic static construction of objects needed to run
     * webview in this process. Does not create threads; safe to call from zygote. Note: it is up to
     * the caller to ensure this is only called once.
     *
     * @param processDataDirSuffix The suffix to use when setting the data directory for this
     *     process; null to use no suffix.
     */
    public static void loadLibrary(String processDataDirSuffix) {
        loadLibrary(null, null, processDataDirSuffix);
    }

    /**
     * Loads the native library, and performs basic static construction of objects needed to run
     * webview in this process. Does not create threads; safe to call from zygote. Note: it is up to
     * the caller to ensure this is only called once.
     *
     * @param processDataDirBasePath The base path to use when setting the data directory for this
     *     process; null to use default base path.
     * @param processCacheDirBasePath The base path to use when setting the cache directory for this
     *     process; null to use default base path.
     * @param processDataDirSuffix The suffix to use when setting the data directory for this
     *     process; null to use no suffix.
     */
    public static void loadLibrary(
            String processDataDirBasePath,
            String processCacheDirBasePath,
            String processDataDirSuffix) {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_WEBVIEW);
        sProcessDataDirSuffix = processDataDirSuffix;
        sDataDirBasePathOverridden = (processDataDirBasePath != null);
        if (processDataDirSuffix == null) {
            PathUtils.setPrivateDirectoryPath(
                    processDataDirBasePath,
                    processCacheDirBasePath,
                    WEBVIEW_DIR_BASENAME,
                    "WebView");
        } else {
            String processDataDirName = WEBVIEW_DIR_BASENAME + "_" + processDataDirSuffix;
            PathUtils.setPrivateDirectoryPath(
                    processDataDirBasePath,
                    processCacheDirBasePath,
                    processDataDirName,
                    processDataDirName);
        }
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            LibraryLoader.getInstance().loadNow();
            // Switch the command line implementation from Java to native.
            // It's okay for the WebView to do this before initialization because we have
            // setup the JNI bindings by this point.
            LibraryLoader.getInstance().switchCommandLineForWebView();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Configures child process launcher for tests. This is required for multiprocess mode to ensure
     * the process type of the child process is WebView, but many of the other fields from
     * configureChildProcessLauncher do not work in testing, so tests need a customized version of
     * that method.
     */
    public static void configureChildProcessLauncherForTesting() {
        final boolean isExternalService = false;
        final boolean bindToCaller = false;
        final boolean ignoreVisibilityForImportance = false;
        final boolean isNativeWebViewZygoteEnabled = false;
        ChildProcessCreationParams.set(
                ContextUtils.getApplicationContext().getPackageName(),
                ContextUtils.getApplicationContext().getPackageName(),
                isExternalService,
                LibraryProcessType.PROCESS_WEBVIEW_CHILD,
                bindToCaller,
                ignoreVisibilityForImportance,
                isNativeWebViewZygoteEnabled);
    }

    /**
     * onStartupComplete performs the final steps of Chromium startup, e.g enabling the task
     * runners. It's called when WebViewChromiumAwInit startup tasks are done. Tests that start the
     * browser process directly should use this.
     */
    public static void startForTesting() {
        StartupTasks.preBrowserProcessStartStepTwo();
        StartupTasks.finishBrowserProcessStart();
        AwAccessibilityStateVisibilityManager.initializeOnStartup();
        onStartupComplete();
    }

    public static void setWebViewPackageName(String webViewPackageName) {
        assert sWebViewPackageName == null || sWebViewPackageName.equals(webViewPackageName);
        sWebViewPackageName = webViewPackageName;
    }

    public static void setNativeWebViewZygoteEnabled(boolean enabled) {
        AwBrowserProcessJni.get().setNativeWebViewZygoteEnabled(enabled);
    }

    public static void setProcessNameCrashKey(String processName) {
        AwBrowserProcessJni.get().setProcessNameCrashKey(processName);
    }

    public static String getWebViewPackageName() {
        if (sWebViewPackageName == null) return ""; // May be null in testing.
        return sWebViewPackageName;
    }

    public static void setProcessDataDirSuffixForTesting(@Nullable String processDataDirSuffix) {
        sProcessDataDirSuffix = processDataDirSuffix;
    }

    @Nullable
    public static String getProcessDataDirSuffix() {
        return sProcessDataDirSuffix;
    }

    public static boolean isDataDirBasePathOverridden() {
        return sDataDirBasePathOverridden;
    }

    /**
     * Notify the native code that the embedder is done with startup. In WebView's case, this is
     * when we are done running the startup tasks.
     */
    public static void onStartupComplete() {
        AwBrowserProcessJni.get().onStartupComplete();
    }

    /**
     * Read the command line flags required for tracing init.
     *
     * <p>This method must be called on the main thread, to ensure there is no cross-thread access
     * to the native CommandLine instance.
     *
     * <p>Must be called before {@link #initTracing(boolean, boolean)}.
     */
    public static void readTracingCommandLineOnMainThread() {
        AwBrowserProcessJni.get().readTracingCommandLineOnMainThread();
    }

    /**
     * Start tracing initialization.
     *
     * <p>This requires {@link #readTracingCommandLineOnMainThread()} to be called before calling
     * this method.
     *
     * <p>This must only be called <em>before</em> Content startup. If Content Main has already been
     * called, tracing will already be initialized, and this method will crash.
     *
     * @param enableSystemConsumer Set to {@code true} in order to send Perfetto traces to the
     *     Android system consumer. Equivalent to enabling {@link
     *     org.chromium.services.tracing.TracingServiceFeatures.ENABLE_PERFETTO_SYSTEM_TRACING}
     * @param runningOnBackgroundThread Indicates that tracing is being initialized on a background
     *     thread, which will set up the mechanism for Startup to wait for initialization to finish
     *     before proceeding.
     */
    public static void initTracing(
            boolean enableSystemConsumer, boolean runningOnBackgroundThread) {
        AwBrowserProcessJni.get().initTracing(enableSystemConsumer, runningOnBackgroundThread);
    }

    /**
     * Sets a flag to indicate that tracing will be initialized on a background thread. Should be
     * set if {@link #initTracing(boolean, boolean)} is called from a background thread.
     */
    public static void markTracingInitializedOnBackground() {
        AwBrowserProcessJni.get().markTracingInitializedOnBackground();
    }

    /**
     * Sets a flag to disable tracing init during normal browser main. Should be set if tracing is
     * initialized earlier during startup.
     */
    public static void disableTracingInitDuringBrowserMain() {
        AwBrowserProcessJni.get().disableTracingInitDuringBrowserMain();
    }

    // Do not instantiate this class.
    private AwBrowserProcess() {}

    @NativeMethods
    interface Natives {
        void setNativeWebViewZygoteEnabled(boolean enabled);

        void setProcessNameCrashKey(@JniType("std::string") String processName);

        void onStartupComplete();

        void readTracingCommandLineOnMainThread();

        void initTracing(
                @JniType("bool") boolean enableSystemConsumer,
                @JniType("bool") boolean runningOnBackgroundThread);

        void markTracingInitializedOnBackground();

        void disableTracingInitDuringBrowserMain();
    }
}
