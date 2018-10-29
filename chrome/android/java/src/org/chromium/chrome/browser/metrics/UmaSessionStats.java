// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.content_public.browser.WebContents;

/**
 * Mainly sets up session stats for chrome. A session is defined as the duration when the
 * application is in the foreground.  Also used to communicate information between Chrome
 * and the framework's MetricService.
 */
public class UmaSessionStats {
    private static final String SAMSUNG_MULTWINDOW_PACKAGE = "com.sec.feature.multiwindow";

    private static long sNativeUmaSessionStats;

    // TabModelSelector is needed to get the count of open tabs. We want to log the number of open
    // tabs on every page load.
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private final Context mContext;
    private final boolean mIsMultiWindowCapable;
    private ComponentCallbacks mComponentCallbacks;

    private boolean mKeyboardConnected;

    public UmaSessionStats(Context context) {
        mContext = context;
        mIsMultiWindowCapable = context.getPackageManager().hasSystemFeature(
                SAMSUNG_MULTWINDOW_PACKAGE);
    }

    private void recordPageLoadStats(Tab tab) {
        WebContents webContents = tab.getWebContents();
        boolean isDesktopUserAgent = webContents != null
                && webContents.getNavigationController().getUseDesktopUserAgent();
        nativeRecordPageLoaded(isDesktopUserAgent);
        if (mKeyboardConnected) {
            nativeRecordPageLoadedWithKeyboard();
        }

        String url = tab.getUrl();
        if (!TextUtils.isEmpty(url) && UrlUtilities.isHttpOrHttps(url)) {
            AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> {
                boolean isEligible =
                        InstantAppsHandler.getInstance().getInstantAppIntentForUrl(url) != null;
                RecordHistogram.recordBooleanHistogram(
                        "Android.InstantApps.EligiblePageLoaded", isEligible);
            });
        }

        // If the session has ended (i.e. chrome is in the background), escape early. Ideally we
        // could track this number as part of either the previous or next session but this isn't
        // possible since the TabSelector is needed to figure out the current number of open tabs.
        if (mTabModelSelector == null) return;

        TabModel regularModel = mTabModelSelector.getModel(false);
        nativeRecordTabCountPerLoad(getTabCountFromModel(regularModel));
    }

    private int getTabCountFromModel(TabModel model) {
        return model == null ? 0 : model.getCount();
    }

    /**
     * Starts a new session for logging.
     * @param tabModelSelector A TabModelSelector instance for recording tab counts on page loads.
     * If null, UmaSessionStats does not record page loads and tab counts.
     */
    public void startNewSession(TabModelSelector tabModelSelector) {
        ensureNativeInitialized();

        mTabModelSelector = tabModelSelector;
        if (mTabModelSelector != null) {
            mComponentCallbacks = new ComponentCallbacks() {
                @Override
                public void onLowMemory() {
                    // Not required
                }

                @Override
                public void onConfigurationChanged(Configuration newConfig) {
                    mKeyboardConnected = newConfig.keyboard != Configuration.KEYBOARD_NOKEYS;
                }
            };
            mContext.registerComponentCallbacks(mComponentCallbacks);
            mKeyboardConnected = mContext.getResources().getConfiguration()
                    .keyboard != Configuration.KEYBOARD_NOKEYS;
            mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
                @Override
                public void onPageLoadFinished(Tab tab) {
                    recordPageLoadStats(tab);
                }
            };
        }

        nativeUmaResumeSession(sNativeUmaSessionStats);
        updatePreferences();
        updateMetricsServiceState();
        DefaultBrowserInfo.logDefaultBrowserStats();
    }

    private static void ensureNativeInitialized() {
        // Lazily create the native object and the notification handler. These objects are never
        // destroyed.
        if (sNativeUmaSessionStats == 0) {
            sNativeUmaSessionStats = nativeInit();
        }
    }

    /**
     * Logs screen ratio on Samsung MultiWindow devices.
     */
    public void logMultiWindowStats(int windowArea, int displayArea, int instanceCount) {
        if (mIsMultiWindowCapable) {
            if (displayArea == 0) return;
            int areaPercent = (windowArea * 100) / displayArea;
            int safePercent = areaPercent > 0 ? areaPercent : 0;
            nativeRecordMultiWindowSession(safePercent, instanceCount);
        }
    }

    /**
     * Logs the current session.
     */
    public void logAndEndSession() {
        if (mTabModelSelector != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelector = null;
        }

        nativeUmaEndSession(sNativeUmaSessionStats);
    }

    /**
     * Updates the metrics services based on a change of consent. This can happen during first-run
     * flow, and when the user changes their preferences.
     */
    public static void changeMetricsReportingConsent(boolean consent) {
        PrivacyPreferencesManager privacyManager = PrivacyPreferencesManager.getInstance();
        // Update the metrics reporting preference.
        privacyManager.setUsageAndCrashReporting(consent);

        // Perform native changes needed to reflect the new consent value.
        nativeChangeMetricsReportingConsent(consent);

        updateMetricsServiceState();
    }

    /**
     * Initializes the metrics consent bit to false. Used only for testing.
     */
    public static void initMetricsAndCrashReportingForTesting() {
        nativeInitMetricsAndCrashReportingForTesting();
    }

    /**
     * Clears the metrics consent bit used for testing to original setting. Used only for testing.
     */
    public static void unSetMetricsAndCrashReportingForTesting() {
        nativeUnsetMetricsAndCrashReportingForTesting();
    }

    /**
     * Updates the metrics consent bit to |consent|. Used only for testing.
     */
    public static void updateMetricsAndCrashReportingForTesting(boolean consent) {
        nativeUpdateMetricsAndCrashReportingForTesting(consent);
    }

    /**
     * Updates the state of MetricsService to account for the user's preferences.
     */
    public static void updateMetricsServiceState() {
        PrivacyPreferencesManager privacyManager = PrivacyPreferencesManager.getInstance();

        // Ensure Android and Chrome local state prefs are in sync.
        privacyManager.syncUsageAndCrashReportingPrefs();

        boolean mayUploadStats = privacyManager.isMetricsUploadPermitted();

        // Re-start the MetricsService with the given parameter, and current consent.
        nativeUpdateMetricsServiceState(mayUploadStats);
    }

    /**
     * Updates relevant Android and native preferences.
     */
    private void updatePreferences() {
        PrivacyPreferencesManager prefManager = PrivacyPreferencesManager.getInstance();
        prefManager.migrateUsageAndCrashPreferences();

        // Update the metrics sampling state so it's available before the native feature list is
        // available.
        prefManager.setClientInMetricsSample(UmaUtils.isClientInMetricsReportingSample());

        // Make sure preferences are in sync.
        prefManager.syncUsageAndCrashReportingPrefs();
    }

    public static void registerExternalExperiment(String studyName, int[] experimentIds) {
        nativeRegisterExternalExperiment(studyName, experimentIds);
    }

    public static void registerSyntheticFieldTrial(String trialName, String groupName) {
        nativeRegisterSyntheticFieldTrial(trialName, groupName);
    }

    private static native long nativeInit();
    private static native void nativeChangeMetricsReportingConsent(boolean consent);
    private static native void nativeInitMetricsAndCrashReportingForTesting();
    private static native void nativeUnsetMetricsAndCrashReportingForTesting();
    private static native void nativeUpdateMetricsAndCrashReportingForTesting(boolean consent);
    private static native void nativeUpdateMetricsServiceState(boolean mayUpload);
    private native void nativeUmaResumeSession(long nativeUmaSessionStats);
    private native void nativeUmaEndSession(long nativeUmaSessionStats);
    private static native void nativeRegisterExternalExperiment(
            String studyName, int[] experimentIds);
    private static native void nativeRegisterSyntheticFieldTrial(
            String trialName, String groupName);
    private static native void nativeRecordMultiWindowSession(int areaPercent, int instanceCount);
    private static native void nativeRecordTabCountPerLoad(int numTabsOpen);
    private static native void nativeRecordPageLoaded(boolean isDesktopUserAgent);
    private static native void nativeRecordPageLoadedWithKeyboard();

}
