// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Mainly sets up session stats for chrome. A session is defined as the duration when the
 * application is in the foreground.  Also used to communicate information between Chrome
 * and the framework's MetricService.
 */
public class UmaSessionStats {
    private static final String TAG = "UmaSessionStats";
    private static final String SAMSUNG_MULTWINDOW_PACKAGE = "com.sec.feature.multiwindow";

    private static long sNativeUmaSessionStats;

    // TabModelSelector is needed to get the count of open tabs. We want to log the number of open
    // tabs on every page load.
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private final Context mContext;
    private ComponentCallbacks mComponentCallbacks;

    private boolean mKeyboardConnected;

    public UmaSessionStats(Context context) {
        mContext = context;
    }

    private void recordPageLoadStats(Tab tab) {
        WebContents webContents = tab.getWebContents();
        boolean isDesktopUserAgent = webContents != null
                && webContents.getNavigationController().getUseDesktopUserAgent();
        UmaSessionStatsJni.get().recordPageLoaded(isDesktopUserAgent);
        if (mKeyboardConnected) {
            UmaSessionStatsJni.get().recordPageLoadedWithKeyboard();
        }

        String url = tab.getUrlString();
        if (!TextUtils.isEmpty(url) && UrlUtilities.isHttpOrHttps(url)) {
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
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
        UmaSessionStatsJni.get().recordTabCountPerLoad(getTabCountFromModel(regularModel));
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
                public void onPageLoadFinished(Tab tab, GURL url) {
                    recordPageLoadStats(tab);
                }
            };
        }

        UmaSessionStatsJni.get().umaResumeSession(sNativeUmaSessionStats, UmaSessionStats.this);
        updatePreferences();
        updateMetricsServiceState();
        DefaultBrowserInfo.logDefaultBrowserStats();
    }

    private static void ensureNativeInitialized() {
        // Lazily create the native object and the notification handler. These objects are never
        // destroyed.
        if (sNativeUmaSessionStats == 0) {
            sNativeUmaSessionStats = UmaSessionStatsJni.get().init();
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

        UmaSessionStatsJni.get().umaEndSession(sNativeUmaSessionStats, UmaSessionStats.this);
    }

    /**
     * Updates the metrics services based on a change of consent. This can happen during first-run
     * flow, and when the user changes their preferences.
     */
    public static void changeMetricsReportingConsent(boolean consent) {
        PrivacyPreferencesManagerImpl privacyManager = PrivacyPreferencesManagerImpl.getInstance();
        // Update the metrics reporting preference.
        privacyManager.setUsageAndCrashReporting(consent);

        // Perform native changes needed to reflect the new consent value.
        UmaSessionStatsJni.get().changeMetricsReportingConsent(consent);

        updateMetricsServiceState();
    }

    /**
     * Initializes the metrics consent bit to false. Used only for testing.
     */
    public static void initMetricsAndCrashReportingForTesting() {
        UmaSessionStatsJni.get().initMetricsAndCrashReportingForTesting();
    }

    /**
     * Clears the metrics consent bit used for testing to original setting. Used only for testing.
     */
    public static void unSetMetricsAndCrashReportingForTesting() {
        UmaSessionStatsJni.get().unsetMetricsAndCrashReportingForTesting();
    }

    /**
     * Updates the metrics consent bit to |consent|. Used only for testing.
     */
    public static void updateMetricsAndCrashReportingForTesting(boolean consent) {
        UmaSessionStatsJni.get().updateMetricsAndCrashReportingForTesting(consent);
    }

    /**
     * Updates the state of MetricsService to account for the user's preferences.
     */
    public static void updateMetricsServiceState() {
        PrivacyPreferencesManagerImpl privacyManager = PrivacyPreferencesManagerImpl.getInstance();

        // Ensure Android and Chrome local state prefs are in sync.
        privacyManager.syncUsageAndCrashReportingPrefs();

        boolean mayUploadStats = privacyManager.isMetricsUploadPermitted();

        // Re-start the MetricsService with the given parameter, and current consent.
        UmaSessionStatsJni.get().updateMetricsServiceState(mayUploadStats);
    }

    /**
     * Updates relevant Android and native preferences.
     */
    private void updatePreferences() {
        PrivacyPreferencesManagerImpl prefManager = PrivacyPreferencesManagerImpl.getInstance();

        // Update the metrics sampling state so it's available before the native feature list is
        // available.
        prefManager.setClientInMetricsSample(UmaUtils.isClientInMetricsReportingSample());

        // Make sure preferences are in sync.
        prefManager.syncUsageAndCrashReportingPrefs();
    }

    public static void registerExternalExperiment(String fallbackStudyName, int[] experimentIds) {
        // TODO(https://crbug.com/1111941): Remove this method once all callers have moved onto
        // the overload below.
        registerExternalExperiment(fallbackStudyName, experimentIds, true);
    }

    public static void registerExternalExperiment(
            String fallbackStudyName, int[] experimentIds, boolean overrideExistingIds) {
        assert isMetricsServiceAvailable();
        UmaSessionStatsJni.get().registerExternalExperiment(
                fallbackStudyName, experimentIds, overrideExistingIds);
    }

    public static void registerSyntheticFieldTrial(String trialName, String groupName) {
        Log.d(TAG, "registerSyntheticFieldTrial(%s, %s)", trialName, groupName);
        assert isMetricsServiceAvailable();
        UmaSessionStatsJni.get().registerSyntheticFieldTrial(trialName, groupName);
    }

    /**
     * UmaSessionStats exposes two static methods on the metrics service. Namely {@link
     * #registerExternalExperiment} and {@link #registerSyntheticFieldTrial}. However those can only
     * be used in full-browser mode and as such you must check this before calling them.
     */
    public static boolean isMetricsServiceAvailable() {
        return BrowserStartupController.getInstance().isFullBrowserStarted();
    }

    /**
     * Returns whether there is a visible activity.
     */
    @CalledByNative
    private static boolean hasVisibleActivity() {
        return ApplicationStatus.hasVisibleActivities();
    }

    @NativeMethods
    interface Natives {
        long init();
        void changeMetricsReportingConsent(boolean consent);
        void initMetricsAndCrashReportingForTesting();
        void unsetMetricsAndCrashReportingForTesting();
        void updateMetricsAndCrashReportingForTesting(boolean consent);
        void updateMetricsServiceState(boolean mayUpload);
        void umaResumeSession(long nativeUmaSessionStats, UmaSessionStats caller);
        void umaEndSession(long nativeUmaSessionStats, UmaSessionStats caller);
        void registerExternalExperiment(
                String studyName, int[] experimentIds, boolean overrideExistingIds);
        void registerSyntheticFieldTrial(String trialName, String groupName);
        void recordTabCountPerLoad(int numTabsOpen);
        void recordPageLoaded(boolean isDesktopUserAgent);
        void recordPageLoadedWithKeyboard();
    }
}
