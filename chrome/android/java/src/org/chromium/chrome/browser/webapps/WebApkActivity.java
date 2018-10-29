// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;

import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.metrics.WebApkSplashscreenMetrics;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.concurrent.TimeUnit;

/**
 * An Activity is designed for WebAPKs (native Android apps) and displays a webapp in a nearly
 * UI-less Chrome.
 */
public class WebApkActivity extends WebappActivity {
    /** Manages whether to check update for the WebAPK, and starts update check if needed. */
    private WebApkUpdateManager mUpdateManager;

    /** The start time that the activity becomes focused. */
    private long mStartTime;

    private WebApkSplashscreenMetrics mWebApkSplashscreenMetrics;

    private static final String TAG = "cr_WebApkActivity";

    @VisibleForTesting
    public static final String STARTUP_UMA_HISTOGRAM_SUFFIX = ".WebApk";

    /**
     * Tries extracting the WebAPK short name from the passed in intent. Returns null if the intent
     * does not launch a WebApkActivity. This method is slow. It makes several PackageManager calls.
     */
    public static String slowExtractNameFromIntentIfTargetIsWebApk(Intent intent) {
        // Check for intents targetted at WebApkActivity and WebApkActivity0-9.
        if (!intent.getComponent().getClassName().startsWith(WebApkActivity.class.getName())) {
            return null;
        }

        WebApkInfo info = WebApkInfo.create(intent);
        return (info != null) ? info.shortName() : null;
    }

    @Override
    public int getActivityType() {
        return ActivityType.WEBAPK;
    }

    @Override
    public @WebappScopePolicy.Type int scopePolicy() {
        return WebappScopePolicy.Type.STRICT;
    }

    @Override
    protected WebappInfo createWebappInfo(Intent intent) {
        return (intent == null) ? WebApkInfo.createEmpty() : WebApkInfo.create(intent);
    }

    @Override
    protected void initializeUI(Bundle savedInstance) {
        super.initializeUI(savedInstance);
        getActivityTab().setWebappManifestScope(getWebappInfo().scopeUri().toString());
    }

    @Override
    public boolean shouldPreferLightweightFre(Intent intent) {
        // We cannot use getWebApkPackageName() because {@link WebappActivity#preInflationStartup()}
        // may not have been called yet.
        String webApkPackageName =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);

        // Use the lightweight FRE for unbound WebAPKs.
        return webApkPackageName != null && !webApkPackageName.startsWith(WEBAPK_PACKAGE_PREFIX);
    }

    @Override
    public String getNativeClientPackageName() {
        return getWebappInfo().apkPackageName();
    }

    @Override
    public void onResume() {
        super.onResume();
        mStartTime = SystemClock.elapsedRealtime();
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.WebApk", timeMs, TimeUnit.MILLISECONDS);
    }

    @Override
    protected void onDeferredStartupWithStorage(WebappDataStorage storage) {
        super.onDeferredStartupWithStorage(storage);

        WebApkInfo info = (WebApkInfo) getWebappInfo();
        WebApkUma.recordShellApkVersion(info.shellApkVersion(), info.distributor());

        mUpdateManager = new WebApkUpdateManager(storage);
        mUpdateManager.updateIfNeeded(getActivityTab(), info);
    }

    @Override
    protected void onUpdatedLastUsedTime(
            WebappDataStorage storage, boolean previouslyLaunched, long previousUsageTimestamp) {
        if (previouslyLaunched) {
            WebApkUma.recordLaunchInterval(storage.getLastUsedTimeMs() - previousUsageTimestamp);
        }
    }

    @Override
    public void onPauseWithNative() {
        WebApkInfo info = (WebApkInfo) getWebappInfo();
        WebApkUma.recordWebApkSessionDuration(
                info.distributor(), SystemClock.elapsedRealtime() - mStartTime);
        super.onPauseWithNative();
    }

    @Override
    protected void onDestroyInternal() {
        if (mUpdateManager != null) {
            mUpdateManager.destroy();
        }
        if (mWebApkSplashscreenMetrics != null) {
            mWebApkSplashscreenMetrics = null;
        }
        super.onDestroyInternal();
    }

    @Override
    public void preInflationStartup() {
        // Decide whether to record startup UMA histograms. This is a similar check to the one done
        // in ChromeTabbedActivity.preInflationStartup refer to the comment there for why.
        if (!LibraryLoader.getInstance().isInitialized()) {
            getActivityTabStartupMetricsTracker().trackStartupMetrics(STARTUP_UMA_HISTOGRAM_SUFFIX);
            // If there is a saved instance state, then the intent (and its stored timestamp) might
            // be stale (Android replays intents if there is a recents entry for the activity).
            if (getSavedInstanceState() == null) {
                long shellLaunchTimestampMs =
                        IntentHandler.getWebApkShellLaunchTimestampFromIntent(getIntent());
                mWebApkSplashscreenMetrics.trackSplashscreenMetrics(shellLaunchTimestampMs);
            }
        }
        super.preInflationStartup();
    }

    @Override
    protected void initializeStartupMetrics() {
        super.initializeStartupMetrics();
        mWebApkSplashscreenMetrics = new WebApkSplashscreenMetrics();
        addSplashscreenObserver(mWebApkSplashscreenMetrics);
    }
}
