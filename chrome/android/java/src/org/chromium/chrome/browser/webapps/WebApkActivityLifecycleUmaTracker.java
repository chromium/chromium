// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import dagger.Lazy;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUkmRecorder;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.metrics.LegacyTabStartupMetricsTracker;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.metrics.WebApkSplashscreenMetrics;

import javax.inject.Inject;
import javax.inject.Named;

/** Handles recording user metrics for WebAPK activities. */
@ActivityScope
public class WebApkActivityLifecycleUmaTracker
        implements ActivityStateListener, InflationObserver, PauseResumeWithNativeObserver {
    private final Activity mActivity;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final SplashController mSplashController;
    private final Lazy<LegacyTabStartupMetricsTracker> mLegacyTabStartupMetricsTracker;
    private final Lazy<StartupMetricsTracker> mStartupMetricsTracker;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;

    /** The start time that the activity becomes focused in milliseconds since boot. */
    private long mStartTime;

    @Inject
    public WebApkActivityLifecycleUmaTracker(
            Activity activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            SplashController splashController,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            Lazy<LegacyTabStartupMetricsTracker> legacyStartupMetricsTracker,
            Lazy<StartupMetricsTracker> startupMetricsTracker,
            @Named(SAVED_INSTANCE_SUPPLIER) Supplier<Bundle> savedInstanceStateSupplier) {
        mActivity = activity;
        mIntentDataProvider = intentDataProvider;
        mSplashController = splashController;
        mLegacyTabStartupMetricsTracker = legacyStartupMetricsTracker;
        mStartupMetricsTracker = startupMetricsTracker;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;

        lifecycleDispatcher.register(this);
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);

        // Add UMA recording task at the front of the deferred startup queue as it has a higher
        // priority than other deferred startup tasks like checking for a WebAPK update.
        deferredStartupWithStorageHandler.addTaskToFront(
                (storage, didCreateStorage) -> {
                    if (lifecycleDispatcher.isActivityFinishingOrDestroyed()) return;

                    WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
                    WebApkUmaRecorder.recordShellApkVersion(
                            webApkExtras.shellApkVersion, webApkExtras.distributor);
                });
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.RESUMED) {
            mStartTime = SystemClock.elapsedRealtime();
        }
    }

    @Override
    public void onPreInflationStartup() {
        // Decide whether to record startup UMA histograms. This is a similar check to the one done
        // in ChromeTabbedActivity.performPreInflationStartup refer to the comment there for why.
        if (!LibraryLoader.getInstance().isInitialized()) {
            mLegacyTabStartupMetricsTracker.get().setHistogramSuffix(ActivityType.WEB_APK);
            mStartupMetricsTracker.get().setHistogramSuffix(ActivityType.WEB_APK);
            // If there is a saved instance state, then the intent (and its stored timestamp) might
            // be stale (Android replays intents if there is a recents entry for the activity).
            if (mSavedInstanceStateSupplier.get() == null) {
                Intent intent = mActivity.getIntent();
                // Splash observers are removed once the splash screen is hidden.
                mSplashController.addObserver(
                        new WebApkSplashscreenMetrics(
                                WebappIntentUtils.getWebApkShellLaunchTime(intent),
                                WebappIntentUtils.getNewStyleWebApkSplashShownTime(intent)));
            }
        }
    }

    @Override
    public void onPostInflationStartup() {}

    @Override
    public void onResumeWithNative() {
    }

    @Override
    public void onPauseWithNative() {
        WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
        long sessionDuration = SystemClock.elapsedRealtime() - mStartTime;
        WebApkUmaRecorder.recordWebApkSessionDuration(webApkExtras.distributor, sessionDuration);
        WebApkUkmRecorder.recordWebApkSessionDuration(
                webApkExtras.manifestId,
                webApkExtras.distributor,
                webApkExtras.webApkVersionCode,
                sessionDuration);
    }
}
