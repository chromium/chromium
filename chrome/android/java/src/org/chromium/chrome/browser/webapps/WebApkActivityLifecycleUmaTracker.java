// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUkmRecorder;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.metrics.LegacyTabStartupMetricsTracker;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.metrics.WebApkSplashscreenMetrics;

import java.util.function.Supplier;

/** Handles recording user metrics for WebAPK activities. */
@NullMarked
public class WebApkActivityLifecycleUmaTracker
        implements ActivityStateListener, InflationObserver, PauseResumeWithNativeObserver {
    private final Activity mActivity;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Supplier<SplashController> mSplashController;
    private final LegacyTabStartupMetricsTracker mLegacyTabStartupMetricsTracker;
    private final StartupMetricsTracker mStartupMetricsTracker;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;

    /** The start time that the activity becomes focused in milliseconds since boot. */
    private long mStartTime;

    private boolean isColdStart() {
        return ColdStartTracker.wasColdOnFirstActivityCreationOrNow()
                && SimpleStartupForegroundSessionDetector.runningCleanForegroundSession();
    }

    public WebApkActivityLifecycleUmaTracker(
            Activity activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            Supplier<SplashController> splashController,
            LegacyTabStartupMetricsTracker legacyTabStartupMetricsTracker,
            StartupMetricsTracker startupMetricsTracker,
            Supplier<Bundle> savedInstanceStateSupplier,
            WebappDeferredStartupWithStorageHandler webappDeferredStartupWithStorageHandler,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mIntentDataProvider = intentDataProvider;
        mSplashController = splashController;
        mLegacyTabStartupMetricsTracker = legacyTabStartupMetricsTracker;
        mStartupMetricsTracker = startupMetricsTracker;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;

        lifecycleDispatcher.register(this);
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);

        // Add UMA recording task at the front of the deferred startup queue as it has a higher
        // priority than other deferred startup tasks like checking for a WebAPK update.
        webappDeferredStartupWithStorageHandler.addTaskToFront(
                (storage, didCreateStorage) -> {
                    if (lifecycleDispatcher.isActivityFinishingOrDestroyed()) {
                        return;
                    }

                    WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
                    assumeNonNull(webApkExtras);
                    WebApkUmaRecorder.recordShellApkVersion(
                            webApkExtras.shellApkVersion, webApkExtras.distributor);
                });

        // Decide whether to record startup UMA histograms. This is a similar check to the one done
        // in ChromeTabbedActivity.performPreInflationStartup refer to the comment there for why.
        if (isColdStart()) {
            mLegacyTabStartupMetricsTracker.setHistogramSuffix(ActivityType.WEB_APK);
            mStartupMetricsTracker.setHistogramSuffix(ActivityType.WEB_APK);
            // If there is a saved instance state, then the intent (and its stored timestamp) might
            // be stale (Android replays intents if there is a recents entry for the activity).
            if (mSavedInstanceStateSupplier.get() == null) {
                Intent intent = mActivity.getIntent();
                // Splash observers are removed once the splash screen is hidden.
                mSplashController
                        .get()
                        .addObserver(
                                new WebApkSplashscreenMetrics(
                                        WebappIntentUtils.getWebApkShellLaunchTime(intent),
                                        WebappIntentUtils.getNewStyleWebApkSplashShownTime(
                                                intent)));
            }
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.RESUMED) {
            mStartTime = SystemClock.elapsedRealtime();
        }
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {}

    @Override
    public void onResumeWithNative() {}

    @Override
    public void onPauseWithNative() {
        WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
        assumeNonNull(webApkExtras);

        long sessionDuration = SystemClock.elapsedRealtime() - mStartTime;
        WebApkUmaRecorder.recordWebApkSessionDuration(webApkExtras.distributor, sessionDuration);
        WebApkUkmRecorder.recordWebApkSessionDuration(
                webApkExtras.manifestId,
                webApkExtras.distributor,
                webApkExtras.webApkVersionCode,
                sessionDuration);
    }
}
