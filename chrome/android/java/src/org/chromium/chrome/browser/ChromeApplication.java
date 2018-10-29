// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildConfig;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.BuildHooks;
import org.chromium.build.BuildHooksAndroid;
import org.chromium.build.BuildHooksConfig;
import org.chromium.chrome.browser.crash.ApplicationStatusTracker;
import org.chromium.chrome.browser.crash.PureJavaExceptionHandler;
import org.chromium.chrome.browser.crash.PureJavaExceptionReporter;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ChromeAppComponent;
import org.chromium.chrome.browser.dependency_injection.ChromeAppModule;
import org.chromium.chrome.browser.dependency_injection.DaggerChromeAppComponent;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.init.InvalidStartupDialog;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.vr.OnExitVrRequestListener;
import org.chromium.chrome.browser.vr.VrModuleProvider;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 */
public class ChromeApplication extends Application {
    private static final String COMMAND_LINE_FILE = "chrome-command-line";
    private static final String TAG = "ChromiumApplication";

    private DiscardableReferencePool mReferencePool;

    @Nullable
    private static ChromeAppComponent sComponent;

    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        boolean isBrowserProcess = !ContextUtils.getProcessName().contains(":");
        if (isBrowserProcess) UmaUtils.recordMainEntryPointTime();
        super.attachBaseContext(context);
        ContextUtils.initApplicationContext(this);

        if (isBrowserProcess) {
            if (BuildConfig.IS_MULTIDEX_ENABLED) {
                ChromiumMultiDexInstaller.install(this);
            }
            checkAppBeingReplaced();

            // Renderer and GPU processes have command line passed to them via IPC
            // (see ChildProcessService.java).
            CommandLineInitUtil.initCommandLine(
                    COMMAND_LINE_FILE, ChromeApplication::shouldUseDebugFlags);

            // Requires command-line flags.
            TraceEvent.maybeEnableEarlyTracing();
            TraceEvent.begin("ChromeApplication.attachBaseContext");

            // Register for activity lifecycle callbacks. Must be done before any activities are
            // created and is needed only by processes that use the ApplicationStatus api (which for
            // Chrome is just the browser process).
            ApplicationStatus.initialize(this);

            // Register and initialize application status listener for crashes, this needs to be
            // done as early as possible so that this value is set before any crashes are reported.
            ApplicationStatusTracker tracker = new ApplicationStatusTracker();
            tracker.onApplicationStateChange(ApplicationStatus.getStateForApplication());
            ApplicationStatus.registerApplicationStateListener(tracker);

            // Only browser process requires custom resources.
            BuildHooksAndroid.initCustomResources(this);

            // Disable MemoryPressureMonitor polling when Chrome goes to the background.
            ApplicationStatus.registerApplicationStateListener(
                    ChromeApplication::updateMemoryPressurePolling);

            // Not losing much to not cover the below conditional since it just has simple setters.
            TraceEvent.end("ChromeApplication.attachBaseContext");
        }

        MemoryPressureMonitor.INSTANCE.registerComponentCallbacks();

        if (!ContextUtils.isIsolatedProcess()) {
            // Incremental install disables process isolation, so things in this block will actually
            // be run for incremental apks, but not normal apks.
            PureJavaExceptionHandler.installHandler();
            if (BuildHooksConfig.REPORT_JAVA_ASSERT) {
                BuildHooks.setReportAssertionCallback(
                        PureJavaExceptionReporter::reportJavaException);
            }
        }
        AsyncTask.takeOverAndroidThreadPool();
    }

    private static Boolean shouldUseDebugFlags() {
        return ChromePreferenceManager.getInstance().readBoolean(
                ChromePreferenceManager.COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY, false);
    }

    private static void updateMemoryPressurePolling(@ApplicationState int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            MemoryPressureMonitor.INSTANCE.enablePolling();
        } else if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            MemoryPressureMonitor.INSTANCE.disablePolling();
        }
    }

    /** Ensure this application object is not out-of-date. */
    private void checkAppBeingReplaced() {
        // During app update the old apk can still be triggered by broadcasts and spin up an
        // out-of-date application. Kill old applications in this bad state. See
        // http://crbug.com/658130 for more context and http://b.android.com/56296 for the bug.
        if (ContextUtils.getApplicationAssets() == null) {
            Log.e(TAG, "getResources() null, closing app.");
            System.exit(0);
        }
    }

    @MainDex
    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (isSevereMemorySignal(level) && mReferencePool != null) mReferencePool.drain();
        CustomTabsConnection.onTrimMemory(level);
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

    /**
     * Shows an error dialog following a startup error, and then exits the application.
     * @param e The exception reported by Chrome initialization.
     */
    public static void reportStartupErrorAndExit(final ProcessInitException e) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (ApplicationStatus.getStateForActivity(activity) == ActivityState.DESTROYED) {
            return;
        }
        InvalidStartupDialog.show(activity, e.getErrorCode());
    }

    /**
     * @return The DiscardableReferencePool for the application.
     */
    @MainDex
    public DiscardableReferencePool getReferencePool() {
        ThreadUtils.assertOnUiThread();
        if (mReferencePool == null) {
            mReferencePool = new DiscardableReferencePool();
        }
        return mReferencePool;
    }

    @Override
    public void startActivity(Intent intent) {
        startActivity(intent, null);
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

    /** Returns the application-scoped component. */
    public static ChromeAppComponent getComponent() {
        if (sComponent == null) {
            sComponent = createComponent();
        }
        return sComponent;
    }

    private static ChromeAppComponent createComponent() {
        ChromeAppModule.Factory overriddenFactory =
                ModuleFactoryOverrides.getOverrideFor(ChromeAppModule.Factory.class);
        ChromeAppModule module =
                overriddenFactory == null ? new ChromeAppModule() : overriddenFactory.create();
        return DaggerChromeAppComponent.builder().chromeAppModule(module).build();
    }
}
