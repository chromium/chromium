// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.chrome.browser.background_task_scheduler.ChromeBackgroundTaskFactory;
import org.chromium.chrome.browser.base.MainDexApplicationImpl;
import org.chromium.chrome.browser.base.SplitCompatApplication;
import org.chromium.chrome.browser.crash.ApplicationStatusTracker;
import org.chromium.chrome.browser.crash.FirebaseConfig;
import org.chromium.chrome.browser.crash.PureJavaExceptionHandler;
import org.chromium.chrome.browser.crash.PureJavaExceptionReporter;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ChromeAppComponent;
import org.chromium.chrome.browser.dependency_injection.ChromeAppModule;
import org.chromium.chrome.browser.dependency_injection.DaggerChromeAppComponent;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.vr.OnExitVrRequestListener;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.module_installer.util.ModuleUtil;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.url.GURL;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 *
 * Note: All application logic should be added to {@link ChromeApplicationImpl}, which will be
 * called from the superclass. See {@link SplitCompatApplication} for more info.
 */
public class ChromeApplication extends SplitCompatApplication {
    private static final String COMMAND_LINE_FILE = "chrome-command-line";
    // Public to allow use in ChromeBackupAgent
    public static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";

    /** Lock on creation of sComponent. */
    private static final Object sLock = new Object();
    @Nullable
    private static volatile ChromeAppComponent sComponent;

    /** Chrome application logic. */
    public static class ChromeApplicationImpl extends MainDexApplicationImpl {
        public ChromeApplicationImpl() {}

        // Called by the framework for ALL processes. Runs before ContentProviders are created.
        // Quirk: context.getApplicationContext() returns null during this method.
        @Override
        public void attachBaseContext(Context context) {
            boolean isBrowserProcess = SplitCompatApplication.isBrowserProcess();

            if (isBrowserProcess) {
                UmaUtils.recordMainEntryPointTime();

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
            if (isBrowserProcess) {
                checkAppBeingReplaced();

                PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
                // Renderer and GPU processes have command line passed to them via IPC
                // (see ChildProcessService.java).
                CommandLineInitUtil.initCommandLine(
                        COMMAND_LINE_FILE, ChromeApplicationImpl::shouldUseDebugFlags);

                // Enable ATrace on debug OS or app builds.
                int applicationFlags = context.getApplicationInfo().flags;
                boolean isAppDebuggable = (applicationFlags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
                boolean isOsDebuggable = BuildInfo.isDebugAndroid();
                // Requires command-line flags.
                TraceEvent.maybeEnableEarlyTracing(
                        (isAppDebuggable || isOsDebuggable) ? TraceEvent.ATRACE_TAG_APP : 0,
                        /*readCommandLine=*/true);
                TraceEvent.begin("ChromeApplication.attachBaseContext");

                // Register for activity lifecycle callbacks. Must be done before any activities are
                // created and is needed only by processes that use the ApplicationStatus api (which
                // for Chrome is just the browser process).
                ApplicationStatus.initialize(getApplication());

                // Register and initialize application status listener for crashes, this needs to be
                // done as early as possible so that this value is set before any crashes are
                // reported.
                ApplicationStatusTracker tracker = new ApplicationStatusTracker();
                tracker.onApplicationStateChange(ApplicationStatus.getStateForApplication());
                ApplicationStatus.registerApplicationStateListener(tracker);

                // Disable MemoryPressureMonitor polling when Chrome goes to the background.
                ApplicationStatus.registerApplicationStateListener(
                        ChromeApplicationImpl::updateMemoryPressurePolling);

                // Initializes the support for dynamic feature modules (browser only).
                ModuleUtil.initApplication();

                // Set Chrome factory for mapping BackgroundTask classes to TaskIds.
                ChromeBackgroundTaskFactory.setAsDefault();

                AppHooks.get().getChimeDelegate().initialize();

                if (VersionConstants.CHANNEL == Channel.CANARY) {
                    GURL.setReportDebugThrowableCallback(
                            PureJavaExceptionReporter::reportJavaException);
                }
            }

            BuildInfo.setFirebaseAppId(FirebaseConfig.getFirebaseAppId());

            if (!ContextUtils.isIsolatedProcess()) {
                // Incremental install disables process isolation, so things in this block will
                // actually be run for incremental apks, but not normal apks.
                PureJavaExceptionHandler.installHandler();
            }

            if (isBrowserProcess) {
                TraceEvent.end("ChromeApplication.attachBaseContext");
            }
        }

        @Override
        public void onCreate() {
            super.onCreate();

            if (SplitCompatApplication.isBrowserProcess()
                    && CachedFeatureFlags.isEnabled(ChromeFeatureList.EARLY_LIBRARY_LOAD)) {
                // Kick off library loading in a separate thread so it's ready when we need it.
                new Thread(() -> LibraryLoader.getInstance().ensureMainDexInitialized()).start();
            }
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

        @MainDex
        @Override
        public void onTrimMemory(int level) {
            super.onTrimMemory(level);
            if (isSevereMemorySignal(level)
                    && GlobalDiscardableReferencePool.getReferencePool() != null) {
                GlobalDiscardableReferencePool.getReferencePool().drain();
            }
            CustomTabsConnection.onTrimMemory(level);
        }

        @Override
        public void startActivity(Intent intent, Bundle options) {
            if (VrModuleProvider.getDelegate().canLaunch2DIntents()
                    || VrModuleProvider.getIntentDelegate().isVrIntent(intent)) {
                super.startActivity(intent, options);
                return;
            }

            VrModuleProvider.getDelegate().requestToExitVr(new OnExitVrRequestListener() {
                @Override
                public void onSucceeded() {
                    if (!VrModuleProvider.getDelegate().canLaunch2DIntents()) {
                        throw new IllegalStateException("Still in VR after having exited VR.");
                    }
                    startActivity(intent, options);
                }

                @Override
                public void onDenied() {}
            });
        }

        @Override
        public void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            // TODO(huayinz): Add observer pattern for application configuration changes.
            if (SplitCompatApplication.isBrowserProcess()) {
                SystemNightModeMonitor.getInstance().onApplicationConfigurationChanged();
            }
        }
    }

    public ChromeApplication(Impl impl) {
        setImpl(impl);
    }

    public ChromeApplication() {
        this(new ChromeApplicationImpl());
    }

    /**
     * Determines whether the given memory signal is considered severe.
     * @param level The type of signal as defined in {@link android.content.ComponentCallbacks2}.
     */
    public static boolean isSevereMemorySignal(int level) {
        // The conditions are expressed using ranges to capture intermediate levels possibly added
        // to the API in the future.
        return (level >= TRIM_MEMORY_RUNNING_LOW && level < TRIM_MEMORY_UI_HIDDEN)
                || level >= TRIM_MEMORY_MODERATE;
    }

    /** Returns the application-scoped component. */
    public static ChromeAppComponent getComponent() {
        if (sComponent == null) {
            synchronized (sLock) {
                if (sComponent == null) {
                    sComponent = createComponent();
                }
            }
        }
        return sComponent;
    }

    private static ChromeAppComponent createComponent() {
        ChromeAppModule.Factory overriddenFactory =
                ModuleFactoryOverrides.getOverrideFor(ChromeAppModule.Factory.class);
        ChromeAppModule module =
                overriddenFactory == null ? new ChromeAppModule() : overriddenFactory.create();

        AppHooksModule.Factory appHooksFactory =
                ModuleFactoryOverrides.getOverrideFor(AppHooksModule.Factory.class);
        AppHooksModule appHooksModule =
                appHooksFactory == null ? new AppHooksModule() : appHooksFactory.create();

        return DaggerChromeAppComponent.builder().chromeAppModule(module)
                .appHooksModule(appHooksModule).build();
    }
}
