// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.InputDevice;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.variations.SyntheticTrialAnnotationMode;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

/**
 * Mainly sets up session stats for chrome. A session is defined as the duration when the
 * application is in the foreground.  Also used to communicate information between Chrome
 * and the framework's MetricService.
 */
public class UmaSessionStats {
    private static final String TAG = "UmaSessionStats";

    private static long sNativeUmaSessionStats;

    // TabModelSelector is needed to get the count of open tabs. We want to log the number of open
    // tabs on every page load.
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private final Context mContext;
    private ComponentCallbacks mComponentCallbacks;

    private boolean mKeyboardConnected;

    private static final String TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM =
            "Session.Android.TabbedSessionContainedGoogleSearch";
    private @ActivityType int mCurrentActivityType = ActivityType.PRE_FIRST_TAB;

    private boolean mTabbedSessionContainedGoogleSearch;

    public UmaSessionStats(Context context) {
        mContext = context;
    }

    private void recordPageLoadStats(Tab tab) {
        WebContents webContents = tab.getWebContents();
        boolean isDesktopUserAgent =
                webContents != null
                        && webContents.getNavigationController().getUseDesktopUserAgent();
        UmaSessionStatsJni.get().recordPageLoaded(isDesktopUserAgent);
        var connectedDevices = DeviceUtils.getConnectedDevices();
        if (!connectedDevices.isEmpty()) {
            UmaSessionStatsJni.get().recordPageLoadedWithAccessory();
        }
        if (mKeyboardConnected) {
            UmaSessionStatsJni.get().recordPageLoadedWithKeyboard();
        }
        if (connectedDevices.contains(InputDevice.SOURCE_MOUSE)) {
            UmaSessionStatsJni.get().recordPageLoadedWithMouse();
        }
        if (EdgeToEdgeUtils.isLegacyWebsiteOptInEnabled()
                && EdgeToEdgeUtils.isPageOptedIntoEdgeToEdge(tab)) {
            UmaSessionStatsJni.get().recordPageLoadedWithToEdge();
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
     *
     * @param activityType The type of the Activity.
     * @param tabModelSelector A TabModelSelector instance for recording tab counts on page loads.
     *     If null, UmaSessionStats does not record page loads and tab counts.
     * @param permissionDelegate The AndroidPermissionDelegate used for querying permission status.
     *     If null, UmaSessionStats will not record permission status.
     */
    public void startNewSession(
            @ActivityType int activityType,
            TabModelSelector tabModelSelector,
            AndroidPermissionDelegate permissionDelegate) {
        ensureNativeInitialized();
        mTabbedSessionContainedGoogleSearch = false;
        mCurrentActivityType = activityType;

        mTabModelSelector = tabModelSelector;
        if (mTabModelSelector != null) {
            mComponentCallbacks =
                    new ComponentCallbacks() {
                        @Override
                        public void onLowMemory() {
                            // Not required
                        }

                        @Override
                        public void onConfigurationChanged(Configuration newConfig) {
                            mKeyboardConnected =
                                    newConfig.keyboard != Configuration.KEYBOARD_NOKEYS;
                        }
                    };
            mContext.registerComponentCallbacks(mComponentCallbacks);
            mKeyboardConnected =
                    mContext.getResources().getConfiguration().keyboard
                            != Configuration.KEYBOARD_NOKEYS;
            mTabModelSelectorTabObserver =
                    new TabModelSelectorTabObserver(mTabModelSelector) {
                        @Override
                        public void onPageLoadFinished(Tab tab, GURL url) {
                            recordPageLoadStats(tab);
                        }

                        @Override
                        public void onDidFinishNavigationInPrimaryMainFrame(
                                Tab tab, NavigationHandle navigation) {
                            if (!navigation.hasCommitted()) return;
                            if (UrlUtilitiesJni.get().isGoogleSearchUrl(tab.getUrl().getSpec())) {
                                mTabbedSessionContainedGoogleSearch = true;
                            }
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

    /** Logs the current session. */
    public void logAndEndSession() {
        if (mTabModelSelector != null) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelector = null;
        }
        if (mCurrentActivityType == ActivityType.TABBED) {
            RecordHistogram.recordBooleanHistogram(
                    TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM,
                    mTabbedSessionContainedGoogleSearch);
        }

        UmaSessionStatsJni.get().umaEndSession(sNativeUmaSessionStats, UmaSessionStats.this);
    }

    /**
     * Updates the metrics services based on a change of consent. This can happen during first-run
     * flow, and when the user changes their preferences.
     */
    public static void changeMetricsReportingConsent(
            boolean consent, @ChangeMetricsReportingStateCalledFrom int calledFrom) {
        PrivacyPreferencesManagerImpl privacyManager = PrivacyPreferencesManagerImpl.getInstance();
        // Update the metrics reporting preference.
        privacyManager.setUsageAndCrashReporting(consent);

        // Perform native changes needed to reflect the new consent value.
        UmaSessionStatsJni.get().changeMetricsReportingConsent(consent, calledFrom);

        updateMetricsServiceState();
    }

    /** Initializes the metrics consent bit to false. Used only for testing. */
    public static void initMetricsAndCrashReportingForTesting() {
        UmaSessionStatsJni.get().initMetricsAndCrashReportingForTesting();
    }

    /**
     * Clears the metrics consent bit used for testing to original setting. Used only for testing.
     */
    public static void unSetMetricsAndCrashReportingForTesting() {
        UmaSessionStatsJni.get().unsetMetricsAndCrashReportingForTesting();
    }

    /** Updates the metrics consent bit to |consent|. Used only for testing. */
    public static void updateMetricsAndCrashReportingForTesting(boolean consent) {
        UmaSessionStatsJni.get().updateMetricsAndCrashReportingForTesting(consent);
    }

    /** Updates the state of MetricsService to account for the user's preferences. */
    public static void updateMetricsServiceState() {
        PrivacyPreferencesManagerImpl privacyManager = PrivacyPreferencesManagerImpl.getInstance();

        // Ensure Android and Chrome local state prefs are in sync.
        privacyManager.syncUsageAndCrashReportingPrefs();

        boolean mayUploadStats = privacyManager.isMetricsUploadPermitted();

        // Re-start the MetricsService with the given parameter, and current consent.
        UmaSessionStatsJni.get().updateMetricsServiceState(mayUploadStats);
    }

    /** Updates relevant Android and native preferences. */
    private void updatePreferences() {
        PrivacyPreferencesManagerImpl prefManager = PrivacyPreferencesManagerImpl.getInstance();

        // Update the metrics sampling state so it's available before the native feature list is
        // available.
        prefManager.setClientInSampleForMetrics(UmaUtils.isClientInSampleForMetrics());

        // Update the crash sampling state so it's available before the native feature list is
        // available.
        prefManager.setClientInSampleForCrashes(UmaUtils.isClientInSampleForCrashes());

        // Make sure preferences are in sync.
        prefManager.syncUsageAndCrashReportingPrefs();
    }

    public static void registerExternalExperiment(String fallbackStudyName, int[] experimentIds) {
        // TODO(crbug.com/40142802): Remove this method once all callers have moved onto
        // the overload below.
        registerExternalExperiment(experimentIds, true);
    }

    public static void registerExternalExperiment(
            int[] experimentIds, boolean overrideExistingIds) {
        assert isMetricsServiceAvailable();
        UmaSessionStatsJni.get().registerExternalExperiment(experimentIds, overrideExistingIds);
    }

    public static void registerSyntheticFieldTrial(String trialName, String groupName) {
        registerSyntheticFieldTrial(trialName, groupName, SyntheticTrialAnnotationMode.NEXT_LOG);
    }

    /**
     * Registers a synthetic field trial with the given trial name, group name, and annotation mode.
     * If {@code annotationMode} is set to {@link SyntheticTrialAnnotationMode#CURRENT_LOG}, the
     * metrics logs that are open at the time this method is called (if any) will be annotated with
     * the synthetic trial. Otherwise, only logs that opened after the registration of the synthetic
     * trial will be annotated. See C++ SyntheticTrialRegistry::RegisterSyntheticFieldTrial() for
     * more details.
     */
    public static void registerSyntheticFieldTrial(
            String trialName, String groupName, @SyntheticTrialAnnotationMode int annotationMode) {
        Log.d(TAG, "registerSyntheticFieldTrial(%s, %s, %d)", trialName, groupName, annotationMode);
        assert isMetricsServiceAvailable();
        UmaSessionStatsJni.get().registerSyntheticFieldTrial(trialName, groupName, annotationMode);
    }

    /**
     * UmaSessionStats exposes two static methods on the metrics service. Namely {@link
     * #registerExternalExperiment} and {@link #registerSyntheticFieldTrial}. However those can only
     * be used in full-browser mode and as such you must check this before calling them.
     */
    public static boolean isMetricsServiceAvailable() {
        return BrowserStartupController.getInstance().isFullBrowserStarted();
    }

    /** Returns whether there is a visible activity. */
    @CalledByNative
    private static boolean hasVisibleActivity() {
        return ApplicationStatus.hasVisibleActivities();
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        long init();

        void changeMetricsReportingConsent(boolean consent, int calledFrom);

        void initMetricsAndCrashReportingForTesting();

        void unsetMetricsAndCrashReportingForTesting();

        void updateMetricsAndCrashReportingForTesting(boolean consent);

        void updateMetricsServiceState(boolean mayUpload);

        void umaResumeSession(long nativeUmaSessionStats, UmaSessionStats caller);

        void umaEndSession(long nativeUmaSessionStats, UmaSessionStats caller);

        void registerExternalExperiment(int[] experimentIds, boolean overrideExistingIds);

        void registerSyntheticFieldTrial(
                String trialName,
                String groupName,
                @SyntheticTrialAnnotationMode int annotationMode);

        void recordTabCountPerLoad(int numTabsOpen);

        void recordPageLoaded(boolean isDesktopUserAgent);

        void recordPageLoadedWithKeyboard();

        void recordPageLoadedWithMouse();

        void recordPageLoadedWithAccessory();

        void recordPageLoadedWithToEdge();
    }
}
