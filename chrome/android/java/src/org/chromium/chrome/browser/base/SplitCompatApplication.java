// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Process;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BundleUtils;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.IntentUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.chrome.browser.crash.ApplicationStatusTracker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.components.crash.CustomAssertionHandler;
import org.chromium.components.crash.PureJavaExceptionHandler;
import org.chromium.components.crash.PureJavaExceptionHandler.JavaExceptionReporter;
import org.chromium.components.crash.PureJavaExceptionHandler.JavaExceptionReporterFactory;
import org.chromium.components.embedder_support.application.FontPreloadingWorkaround;
import org.chromium.components.module_installer.util.ModuleUtil;
import org.chromium.ui.base.ResourceBundle;

import java.util.function.Supplier;

/**
 * Application base class which will call through to the given {@link Impl}. Application classes
 * which extend this class should also extend {@link Impl}, and call {@link #setImpl(Impl)} before
 * calling {@link attachBaseContext(Context)}.
 *
 * <p>This is the base class of all Chrome applications. Logic specific to isolated splits should go
 * in {@link SplitChromeApplication}.
 */
@NullMarked
public class SplitCompatApplication extends Application {
    public static final String CHROME_SPLIT_NAME = "chrome";
    public static final String ON_DEMAND_SPLIT_NAME = "on_demand";
    private static final String TAG = "SplitCompatApp";
    private static final String COMMAND_LINE_FILE = "chrome-command-line";
    private static final String ATTACH_BASE_CONTEXT_EVENT = "ChromeApplication.attachBaseContext";
    // Public to allow use in ChromeBackupAgent
    public static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    @VisibleForTesting
    public static final String LAUNCH_FAILED_ACTIVITY_CLASS_NAME =
            "org.chromium.chrome.browser.init.LaunchFailedActivity";

    private Supplier<Impl> mImplSupplier;
    private @Nullable Impl mImpl;

    /**
     * Holds the implementation of application logic. Will be called by {@link
     * SplitCompatApplication}.
     */
    public static class Impl {
        private SplitCompatApplication mApplication;

        @Initializer
        private void setApplication(SplitCompatApplication application) {
            mApplication = application;
        }

        protected final SplitCompatApplication getApplication() {
            return mApplication;
        }

        @CallSuper
        public void startActivity(Intent intent, @Nullable Bundle options) {
            mApplication.superStartActivity(intent, options);
        }

        public void onCreate() {}

        public void onTrimMemory(int level) {}

        public void onConfigurationChanged(Configuration newConfig) {}
    }

    @Initializer
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
    private void superStartActivity(Intent intent, @Nullable Bundle options) {
        super.startActivity(intent, options);
    }

    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        boolean isIsolatedProcess = ContextUtils.isIsolatedProcess();
        boolean isBrowserProcess = isBrowserProcess();

        if (isBrowserProcess) {
            UmaUtils.recordMainEntryPointTime();

            // *** The Application Context should not be used before the locale override is set ***
            if (GlobalAppLocaleController.getInstance().init(context)) {
                // If the app locale override preference is set, create a new override
                // context to use as the base context for the application.
                // Must be initialized early to override Application level localizations.
                Configuration config =
                        GlobalAppLocaleController.getInstance().getOverrideConfig(context);
                LocaleUtils.setDefaultLocalesFromConfiguration(config);
                context = context.createConfigurationContext(config);
            }
        }

        super.attachBaseContext(context);
        // Perform initialization of globals common to all processes.
        ContextUtils.initApplicationContext(this);

        Log.i(
                TAG,
                "version=%s (%s) minSdkVersion=%s processName=%s isIsolatedProcess=%s splits=%s",
                VersionConstants.PRODUCT_VERSION,
                BuildConfig.VERSION_CODE,
                BuildConfig.MIN_SDK_VERSION,
                ContextUtils.getProcessName(),
                isIsolatedProcess,
                // BundleUtils uses getApplicationContext, so logging after we init it.
                BundleUtils.getInstalledSplitNamesForLogging());

        if (isBrowserProcess) {
            // This must come as early as possible to avoid early loading of the native library from
            // failing unnoticed.
            LibraryLoader.sLoadFailedCallback =
                    unsatisfiedLinkError -> {
                        Intent newIntent = new Intent();
                        newIntent.setComponent(
                                new ComponentName(
                                        ContextUtils.getApplicationContext(),
                                        LAUNCH_FAILED_ACTIVITY_CLASS_NAME));
                        newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        IntentUtils.safeStartActivity(
                                ContextUtils.getApplicationContext(), newIntent);
                        if (cannotLoadIn64Bit()) {
                            throw new RuntimeException(
                                    "Starting in 64-bit mode requires the 64-bit native library. If"
                                            + " the device is 64-bit only, see alternatives here: "
                                            + "https://crbug.com/1303857#c7.",
                                    unsatisfiedLinkError);
                        } else if (cannotLoadIn32Bit()) {
                            throw new RuntimeException(
                                    "Starting in 32-bit mode requires the 32-bit native library.",
                                    unsatisfiedLinkError);
                        }
                        throw unsatisfiedLinkError;
                    };
        }

        maybeInitProcessType();
        LibraryLoader.getInstance().setLinkerImplementation(ProductConfig.USE_CHROMIUM_LINKER);

        // Renderer and GPU processes have command line passed to them via IPC
        // (see ChildProcessService.java).
        if (isBrowserProcess && ChromeFeatureList.sLoadNativeEarly.isEnabled()) {
            CommandLineInitUtil.initCommandLine(
                    COMMAND_LINE_FILE, SplitCompatApplication::shouldUseDebugFlags);
        }

        if (isBrowserProcess) {
            performBrowserProcessPreloading(context);
        }

        AsyncTask.takeOverAndroidThreadPool();
        ResourceBundle.setAvailablePakLocales(ProductConfig.LOCALES);

        if (!isBrowserProcess) {
            EarlyTraceEvent.earlyEnableInChildWithoutCommandLine();
            TraceEvent.begin(ATTACH_BASE_CONTEXT_EVENT);
        } else {
            checkAppBeingReplaced();
            DexFixer.scheduleDexFix();

            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            // Renderer and GPU processes have command line passed to them via IPC
            // (see ChildProcessService.java).
            if (!ChromeFeatureList.sLoadNativeEarly.isEnabled()) {
                CommandLineInitUtil.initCommandLine(
                        COMMAND_LINE_FILE, SplitCompatApplication::shouldUseDebugFlags);
            }

            TraceEvent.maybeEnableEarlyTracing(/* readCommandLine= */ true);
            TraceEvent.begin(ATTACH_BASE_CONTEXT_EVENT);

            // Register for activity lifecycle callbacks. Must be done before any activities are
            // created and is needed only by processes that use the ApplicationStatus api (which
            // for Chrome is just the browser process).
            ApplicationStatus.initialize(this);
            ColdStartTracker.initialize();

            // Register and initialize application status listener for crashes, this needs to be
            // done as early as possible so that this value is set before any crashes are
            // reported.
            ApplicationStatusTracker tracker = new ApplicationStatusTracker();
            tracker.onApplicationStateChange(ApplicationStatus.getStateForApplication());
            ApplicationStatus.registerApplicationStateListener(tracker);

            // Disable MemoryPressureMonitor polling when Chrome goes to the background.
            ApplicationStatus.registerApplicationStateListener(
                    SplitCompatApplication::updateMemoryPressurePolling);

            if (AppLocaleUtils.shouldUseSystemManagedLocale()) {
                AppLocaleUtils.maybeMigrateOverrideLanguage();
            }
        }

        // Write installed modules to crash keys.
        ModuleUtil.updateCrashKeys();

        // WebView installs its own PureJavaExceptionHandler.
        // Incremental install disables process isolation, so things in this block will
        // actually be run for incremental apks, but not normal apks.
        if (!isIsolatedProcess && !isWebViewProcess()) {
            JavaExceptionReporterFactory factory =
                    new JavaExceptionReporterFactory() {
                        @Override
                        public JavaExceptionReporter createJavaExceptionReporter() {
                            // ChromePureJavaExceptionReporter may be in the chrome module, so load
                            // by reflection from there.
                            return (JavaExceptionReporter)
                                    BundleUtils.newInstance(
                                            "org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter",
                                            CHROME_SPLIT_NAME);
                        }
                    };
            PureJavaExceptionHandler.installHandler(factory);
            CustomAssertionHandler.installPreNativeHandler(factory);
        }

        ApplicationInitHook initHook = ServiceLoaderUtil.maybeCreate(ApplicationInitHook.class);
        if (initHook != null) {
            initHook.onAttachBaseContext(isBrowserProcess, isIsolatedProcess);
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
    public void startActivity(Intent intent, @Nullable Bundle options) {
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

    public static boolean cannotLoadIn64Bit() {
        if (LibraryLoader.sOverrideNativeLibraryCannotBeLoadedForTesting) {
            return true;
        }
        return Process.is64Bit() && !NativeLibraries.sSupport64Bit;
    }

    public static boolean cannotLoadIn32Bit() {
        return !Process.is64Bit() && !NativeLibraries.sSupport32Bit;
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
        return ChromeFeatureList.sCommandLineOnNonRooted.isEnabled();
    }

    private static void updateMemoryPressurePolling(@ApplicationState int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            MemoryPressureMonitor.INSTANCE.enablePolling(
                    ChromeFeatureList.sPostGetMyMemoryStateToBackground.isEnabled());
        } else if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            MemoryPressureMonitor.INSTANCE.disablePolling();
        }
    }

    /** Ensure this application object is not out-of-date. */
    private static void checkAppBeingReplaced() {
        // During app update the old apk can still be triggered by broadcasts and spin up an
        // out-of-date application. Kill old applications in this bad state. See
        // http://crbug.com/658130 for more context and http://b.android.com/56296 for the bug.
        if (ContextUtils.getApplicationContext().getAssets() == null) {
            throw new RuntimeException("App out of date, getResources() null, closing app.");
        }
    }
}
