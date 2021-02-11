// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.CallSuper;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildInfo;
import org.chromium.base.BundleUtils;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.JNIUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.chrome.browser.crash.ApplicationStatusTracker;
import org.chromium.chrome.browser.crash.FirebaseConfig;
import org.chromium.chrome.browser.crash.PureJavaExceptionHandler;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.components.embedder_support.application.FontPreloadingWorkaround;
import org.chromium.components.module_installer.util.ModuleUtil;
import org.chromium.ui.base.ResourceBundle;

/**
 * Application base class which will call through to the given {@link Impl}. Application classes
 * which extend this class should also extend {@link Impl}, and call {@link #setImpl(Impl)} before
 * calling {@link attachBaseContext(Context)}.
 *
 * This is the base class of all Chrome applications. Logic specific to isolated splits should go in
 * {@link SplitChromeApplication}.
 */
public class SplitCompatApplication extends Application {
    private static final String COMMAND_LINE_FILE = "chrome-command-line";
    private static final String ATTACH_BASE_CONTEXT_EVENT = "ChromeApplication.attachBaseContext";
    // Public to allow use in ChromeBackupAgent
    public static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    private Supplier<Impl> mImplSupplier;
    private Impl mImpl;

    /**
     * Holds the implementation of application logic. Will be called by {@link
     * SplitCompatApplication}.
     */
    public static class Impl {
        private SplitCompatApplication mApplication;

        private final void setApplication(SplitCompatApplication application) {
            mApplication = application;
        }

        protected final SplitCompatApplication getApplication() {
            return mApplication;
        }

        @CallSuper
        public void startActivity(Intent intent, Bundle options) {
            mApplication.superStartActivity(intent, options);
        }

        public void onCreate() {}

        public void onTrimMemory(int level) {}
        public void onConfigurationChanged(Configuration newConfig) {}
    }

    public final void setImplSupplier(Supplier<Impl> implSupplier) {
        assert mImpl == null;
        assert mImplSupplier == null;
        mImplSupplier = implSupplier;
    }

    private Impl getImpl() {
        if (mImpl == null) {
            mImpl = mImplSupplier.get();
            mImpl.setApplication(this);
        }
        return mImpl;
    }

    /**
     * This exposes the super method so it can be called inside the Impl class code instead of just
     * at the start.
     */
    private void superStartActivity(Intent intent, Bundle options) {
        super.startActivity(intent, options);
    }

    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        boolean isBrowserProcess = isBrowserProcess();

        if (isBrowserProcess) {
            UmaUtils.recordMainEntryPointTime();
            performBrowserProcessPreloading(context);

            // If the app locale override preference is set, create a new override
            // context to use as the base context for the application.
            // Must be initialized early to override Application level localizations.
            if (GlobalAppLocaleController.getInstance().init(context)) {
                Configuration config =
                        GlobalAppLocaleController.getInstance().getOverrideConfig(context);
                LocaleUtils.setDefaultLocalesFromConfiguration(config);
                context = context.createConfigurationContext(config);
            }
        }

        super.attachBaseContext(context);
        // Perform initialization of globals common to all processes.
        ContextUtils.initApplicationContext(this);
        maybeInitProcessType();
        BundleUtils.setIsBundle(ProductConfig.IS_BUNDLE);

        // Write installed modules to crash keys. This needs to be done as early as possible so
        // that these values are set before any crashes are reported.
        ModuleUtil.updateCrashKeys();

        AsyncTask.takeOverAndroidThreadPool();
        JNIUtils.setClassLoader(getClassLoader());
        ResourceBundle.setAvailablePakLocales(ProductConfig.LOCALES);
        LibraryLoader.getInstance().setLinkerImplementation(
                ProductConfig.USE_CHROMIUM_LINKER, ProductConfig.USE_MODERN_LINKER);
        LibraryLoader.getInstance().enableJniChecks();

        if (!isBrowserProcess) {
            EarlyTraceEvent.earlyEnableInChildWithoutCommandLine();
            TraceEvent.begin(ATTACH_BASE_CONTEXT_EVENT);
        } else {
            checkAppBeingReplaced();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                // Fixes are never required before O (where "cmd package compile" does not exist).
                DexFixer.scheduleDexFix();
            }

            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            // Renderer and GPU processes have command line passed to them via IPC
            // (see ChildProcessService.java).
            CommandLineInitUtil.initCommandLine(
                    COMMAND_LINE_FILE, SplitCompatApplication::shouldUseDebugFlags);

            // Enable ATrace on debug OS or app builds.
            int applicationFlags = context.getApplicationInfo().flags;
            boolean isAppDebuggable = (applicationFlags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
            boolean isOsDebuggable = BuildInfo.isDebugAndroid();
            // Requires command-line flags.
            TraceEvent.maybeEnableEarlyTracing(
                    (isAppDebuggable || isOsDebuggable) ? TraceEvent.ATRACE_TAG_APP : 0,
                    /*readCommandLine=*/true);
            TraceEvent.begin(ATTACH_BASE_CONTEXT_EVENT);

            // Register for activity lifecycle callbacks. Must be done before any activities are
            // created and is needed only by processes that use the ApplicationStatus api (which
            // for Chrome is just the browser process).
            ApplicationStatus.initialize(this);

            // Register and initialize application status listener for crashes, this needs to be
            // done as early as possible so that this value is set before any crashes are
            // reported.
            ApplicationStatusTracker tracker = new ApplicationStatusTracker();
            tracker.onApplicationStateChange(ApplicationStatus.getStateForApplication());
            ApplicationStatus.registerApplicationStateListener(tracker);

            // Disable MemoryPressureMonitor polling when Chrome goes to the background.
            ApplicationStatus.registerApplicationStateListener(
                    SplitCompatApplication::updateMemoryPressurePolling);
        }

        BuildInfo.setFirebaseAppId(FirebaseConfig.getFirebaseAppId());

        if (!ContextUtils.isIsolatedProcess()) {
            // Incremental install disables process isolation, so things in this block will
            // actually be run for incremental apks, but not normal apks.
            PureJavaExceptionHandler.installHandler();
        }

        TraceEvent.end(ATTACH_BASE_CONTEXT_EVENT);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        // These can't go in attachBaseContext because Context.getApplicationContext() (which
        // they use under-the-hood) does not work until after it returns.
        FontPreloadingWorkaround.maybeInstallWorkaround(this);
        MemoryPressureMonitor.INSTANCE.registerComponentCallbacks();

        getImpl().onCreate();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        getImpl().onTrimMemory(level);
    }

    /** Forward all startActivity() calls to the two argument version. */
    @Override
    public void startActivity(Intent intent) {
        startActivity(intent, null);
    }

    @Override
    public void startActivity(Intent intent, Bundle options) {
        getImpl().startActivity(intent, options);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        getImpl().onConfigurationChanged(newConfig);
    }

    /**
     * Called right after main entry point time is recorded in the browser process. This is called
     * as early as possible to maximize preload time.
     */
    protected void performBrowserProcessPreloading(Context context) {}

    public boolean isWebViewProcess() {
        return false;
    }

    public static boolean isBrowserProcess() {
        return !ContextUtils.getProcessName().contains(":");
    }

    private void maybeInitProcessType() {
        if (isBrowserProcess()) {
            LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
            return;
        }
        // WebView initialization sets the correct process type.
        if (isWebViewProcess()) return;

        // Child processes set their own process type when bound.
        String processName = ContextUtils.getProcessName();
        if (processName.contains("privileged_process")
                || processName.contains("sandboxed_process")) {
            return;
        }

        // We must be in an isolated service process.
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_CHILD);
    }

    private static Boolean shouldUseDebugFlags() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED);
    }

    private static void updateMemoryPressurePolling(@ApplicationState int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            MemoryPressureMonitor.INSTANCE.enablePolling();
        } else if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            MemoryPressureMonitor.INSTANCE.disablePolling();
        }
    }

    /** Ensure this application object is not out-of-date. */
    private static void checkAppBeingReplaced() {
        // During app update the old apk can still be triggered by broadcasts and spin up an
        // out-of-date application. Kill old applications in this bad state. See
        // http://crbug.com/658130 for more context and http://b.android.com/56296 for the bug.
        if (ContextUtils.getApplicationAssets() == null) {
            throw new RuntimeException("App out of date, getResources() null, closing app.");
        }
    }
}
