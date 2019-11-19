// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.metrics.WebApkSplashscreenMetrics;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * An Activity is designed for WebAPKs (native Android apps) and displays a webapp in a nearly
 * UI-less Chrome.
 */
public class WebApkActivity extends WebappActivity {
    /** Manages whether to check update for the WebAPK, and starts update check if needed. */
    private WebApkUpdateManager mUpdateManager;

    /** The start time that the activity becomes focused in milliseconds since boot. */
    private long mStartTime;

    private static final String TAG = "WebApkActivity";

    @VisibleForTesting
    public static final String STARTUP_UMA_HISTOGRAM_SUFFIX = ".WebApk";

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
        WebContents webContents = getActivityTab().getWebContents();
        if (webContents != null) webContents.notifyRendererPreferenceUpdate();
    }

    @Override
    public boolean shouldPreferLightweightFre(Intent intent) {
        // We cannot use getWebApkPackageName() because
        // {@link WebappActivity#performPreInflationStartup()} may not have been called yet.
        String webApkPackageName =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);

        // Use the lightweight FRE for unbound WebAPKs.
        return webApkPackageName != null
                && !webApkPackageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX);
    }

    @Override
    public String getWebApkPackageName() {
        return getWebApkInfo().webApkPackageName();
    }

    @Override
    public void onResume() {
        super.onResume();
        mStartTime = SystemClock.elapsedRealtime();
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram("MobileStartup.IntentToCreationTime.WebApk", timeMs);
    }

    @Override
    protected void onDeferredStartupWithStorage(WebappDataStorage storage) {
        super.onDeferredStartupWithStorage(storage);

        WebApkInfo info = getWebApkInfo();
        WebApkUma.recordShellApkVersion(info.shellApkVersion(), info.distributor());
        storage.incrementLaunchCount();

        mUpdateManager = new WebApkUpdateManager(storage);
        mUpdateManager.updateIfNeeded(getActivityTab(), info);
    }

    @Override
    protected void onDeferredStartupWithNullStorage(
            WebappDisclosureSnackbarController disclosureSnackbarController) {
        // WebappDataStorage objects are cleared if a user clears Chrome's data. Recreate them
        // for WebAPKs since we need to store metadata for updates and disclosure notifications.
        WebappRegistry.getInstance().register(
                getWebApkInfo().id(), new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                        if (isActivityFinishingOrDestroyed()) return;

                        onDeferredStartupWithStorage(storage);
                        // Set force == true to indicate that we need to show a privacy
                        // disclosure for the newly installed unbound WebAPKs which
                        // have no storage yet. We can't simply default to a showing if the
                        // storage has a default value as we don't want to show this disclosure
                        // for pre-existing unbound WebAPKs.
                        disclosureSnackbarController.maybeShowDisclosure(
                                WebApkActivity.this, storage, true /* force */);
                    }
                });
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
        WebApkInfo info = getWebApkInfo();
        long sessionDuration = SystemClock.elapsedRealtime() - mStartTime;
        WebApkUma.recordWebApkSessionDuration(info.distributor(), sessionDuration);
        WebApkUkmRecorder.recordWebApkSessionDuration(
                info.manifestUrl(), info.distributor(), info.webApkVersionCode(), sessionDuration);
        super.onPauseWithNative();
    }

    @Override
    protected void onDestroyInternal() {
        if (mUpdateManager != null) {
            mUpdateManager.destroy();
        }

        // The common case is to be connected to just one WebAPK's services. For the sake of
        // simplicity disconnect from the services of all WebAPKs.
        ChromeWebApkHost.disconnectFromAllServices(true /* waitForPendingWork */);

        super.onDestroyInternal();
    }

    /**
     * Returns structure containing data about the WebApk currently displayed.
     * The return value should not be cached.
     */
    public WebApkInfo getWebApkInfo() {
        return (WebApkInfo) getWebappInfo();
    }

    @Override
    protected boolean loadUrlIfPostShareTarget(WebappInfo webappInfo) {
        WebApkInfo webApkInfo = (WebApkInfo) webappInfo;
        WebApkInfo.ShareData shareData = webApkInfo.shareData();
        if (shareData == null) {
            return false;
        }
        return new WebApkPostShareTargetNavigator().navigateIfPostShareTarget(
                webApkInfo.url(), webApkInfo.shareTarget(), shareData,
                getActivityTab().getWebContents());
    }

    @Override
    protected void initSplash() {
        super.initSplash();

        // Decide whether to record startup UMA histograms. This is a similar check to the one done
        // in ChromeTabbedActivity.performPreInflationStartup refer to the comment there for why.
        if (!LibraryLoader.getInstance().isInitialized()) {
            getActivityTabStartupMetricsTracker().trackStartupMetrics(STARTUP_UMA_HISTOGRAM_SUFFIX);
            // If there is a saved instance state, then the intent (and its stored timestamp) might
            // be stale (Android replays intents if there is a recents entry for the activity).
            if (getSavedInstanceState() == null) {
                Intent intent = getIntent();
                // Splash observers are removed once the splash screen is hidden.
                addSplashscreenObserver(new WebApkSplashscreenMetrics(
                        WebApkIntentDataProvider.getWebApkShellLaunchTime(intent),
                        WebApkIntentDataProvider.getNewStyleWebApkSplashShownTime(intent)));
            }
        }
    }

    @Override
    protected void handleFinishAndClose() {
        if (getWebApkInfo().isSplashProvidedByWebApk() && isSplashShowing()) {
            // When the WebAPK provides the splash screen, the splash screen activity is stacked
            // underneath the WebAPK. The splash screen finishes itself in
            // {@link Activity#onResume()}. When finishing the WebApkActivity, there is sometimes a
            // frame of the splash screen drawn prior to the splash screen activity finishing
            // itself. There are no glitches when the activity stack is finished via
            // {@link ActivityManager.AppTask#finishAndRemoveTask()}.
            WebApkServiceClient.getInstance().finishAndRemoveTaskSdk23(this);
            return;
        }
        finish();
    }
}
