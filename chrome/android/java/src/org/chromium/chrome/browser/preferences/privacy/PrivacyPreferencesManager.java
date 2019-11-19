// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.survey.SurveyController;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;
import org.chromium.content_public.browser.BrowserStartupController;

/**
 * Reads, writes, and migrates preferences related to network usage and privacy.
 */
public class PrivacyPreferencesManager implements CrashReportingPermissionManager{
    // "crash_dump_upload", "crash_dump_upload_no_cellular" - Deprecated prefs used for
    // 3-option setting for usage and crash reporting. Last used in M55, removed in M78.

    // "cellular_experiment" - Deprecated pref corresponding to the finch experiment
    // controlling migration from 3-option setting to ON/OFF toggle for usage and crash
    // reporting. Last used in M55, removed in M78.

    private static final String DEPRECATED_PREF_PHYSICAL_WEB = "physical_web";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_SHARING = "physical_web_sharing";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_HAS_DEFERRED_METRICS_KEY =
            "PhysicalWeb.HasDeferredMetrics";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_OPT_IN_DECLINE_BUTTON_PRESS_COUNT =
            "PhysicalWeb.OptIn.DeclineButtonPressed";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_OPT_IN_ENABLE_BUTTON_PRESS_COUNT =
            "PhysicalWeb.OptIn.EnableButtonPressed";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PREFS_FEATURE_DISABLED_COUNT =
            "PhysicalWeb.Prefs.FeatureDisabled";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PREFS_FEATURE_ENABLED_COUNT =
            "PhysicalWeb.Prefs.FeatureEnabled";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PREFS_LOCATION_DENIED_COUNT =
            "PhysicalWeb.Prefs.LocationDenied";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PREFS_LOCATION_GRANTED_COUNT =
            "PhysicalWeb.Prefs.LocationGranted";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PWS_BACKGROUND_RESOLVE_TIMES =
            "PhysicalWeb.ResolveTime.Background";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PWS_FOREGROUND_RESOLVE_TIMES =
            "PhysicalWeb.ResolveTime.Foreground";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PWS_REFRESH_RESOLVE_TIMES =
            "PhysicalWeb.ResolveTime.Refresh";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_URL_SELECTED_COUNT =
            "PhysicalWeb.UrlSelected";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_TOTAL_URLS_INITIAL_COUNTS =
            "PhysicalWeb.TotalUrls.OnInitialDisplay";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_TOTAL_URLS_REFRESH_COUNTS =
            "PhysicalWeb.TotalUrls.OnRefresh";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_ACTIVITY_REFERRALS =
            "PhysicalWeb.ActivityReferral";
    private static final String DEPRECATED_PREF_PHYSICAL_WEB_PHYSICAL_WEB_STATE =
            "PhysicalWeb.State";

    public static final String PREF_METRICS_REPORTING = "metrics_reporting";
    private static final String PREF_METRICS_IN_SAMPLE = "in_metrics_sample";
    private static final String PREF_NETWORK_PREDICTIONS = "network_predictions";
    private static final String PREF_BANDWIDTH_OLD = "prefetch_bandwidth";
    private static final String PREF_BANDWIDTH_NO_CELLULAR_OLD = "prefetch_bandwidth_no_cellular";
    private static final String ALLOW_PRERENDER_OLD = "allow_prefetch";

    @SuppressLint("StaticFieldLeak")
    private static PrivacyPreferencesManager sInstance;

    private final Context mContext;
    private final SharedPreferences mSharedPreferences;

    @VisibleForTesting
    PrivacyPreferencesManager(Context context) {
        mContext = context;
        mSharedPreferences = ContextUtils.getAppSharedPreferences();

        migratePhysicalWebPreferences();
    }

    public static PrivacyPreferencesManager getInstance() {
        if (sInstance == null) {
            sInstance = new PrivacyPreferencesManager(ContextUtils.getApplicationContext());
        }
        return sInstance;
    }

    // TODO(https://crbug.com/826540): Remove some time after 5/2019.
    public void migratePhysicalWebPreferences() {
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.remove(DEPRECATED_PREF_PHYSICAL_WEB)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_SHARING)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_HAS_DEFERRED_METRICS_KEY)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_OPT_IN_DECLINE_BUTTON_PRESS_COUNT)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_OPT_IN_ENABLE_BUTTON_PRESS_COUNT)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PREFS_FEATURE_DISABLED_COUNT)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PREFS_FEATURE_ENABLED_COUNT)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PREFS_LOCATION_DENIED_COUNT)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PREFS_LOCATION_GRANTED_COUNT)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PWS_BACKGROUND_RESOLVE_TIMES)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PWS_FOREGROUND_RESOLVE_TIMES)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PWS_REFRESH_RESOLVE_TIMES)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_URL_SELECTED_COUNT)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_TOTAL_URLS_INITIAL_COUNTS)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_TOTAL_URLS_REFRESH_COUNTS)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_ACTIVITY_REFERRALS)
                .remove(DEPRECATED_PREF_PHYSICAL_WEB_PHYSICAL_WEB_STATE)
                .apply();
    }

    /**
     * Migrate and delete old preferences.  Note that migration has to happen in Android-specific
     * code because we need to access ALLOW_PRERENDER sharedPreference.
     * TODO(bnc) https://crbug.com/394845. This change is planned for M38. After a year or so, it
     * would be worth considering removing this migration code and reverting to default for users
     * who had set preferences but have not used Chrome for a year. This change would be subject to
     * privacy review.
     */
    public void migrateNetworkPredictionPreferences() {
        // See if PREF_NETWORK_PREDICTIONS is an old boolean value.
        boolean predictionOptionIsBoolean = false;
        try {
            mSharedPreferences.getString(PREF_NETWORK_PREDICTIONS, "");
        } catch (ClassCastException ex) {
            predictionOptionIsBoolean = true;
        }

        // Nothing to do if the user or this migration code has already set the new
        // preference.
        if (!predictionOptionIsBoolean && obsoleteNetworkPredictionOptionsHasUserSetting()) {
            return;
        }

        // Nothing to do if the old preferences are unset.
        if (!predictionOptionIsBoolean
                && !mSharedPreferences.contains(PREF_BANDWIDTH_OLD)
                && !mSharedPreferences.contains(PREF_BANDWIDTH_NO_CELLULAR_OLD)) {
            return;
        }

        // Migrate if the old preferences are at their default values.
        // (Note that for PREF_BANDWIDTH*, if the setting is default, then there is no way to tell
        // whether the user has set it.)
        final String prefBandwidthDefault =
                BandwidthType.title(BandwidthType.Type.PRERENDER_ON_WIFI);
        final String prefBandwidth =
                mSharedPreferences.getString(PREF_BANDWIDTH_OLD, prefBandwidthDefault);
        boolean prefBandwidthNoCellularDefault = true;
        boolean prefBandwidthNoCellular = mSharedPreferences.getBoolean(
                PREF_BANDWIDTH_NO_CELLULAR_OLD, prefBandwidthNoCellularDefault);

        if (!(prefBandwidthDefault.equals(prefBandwidth))
                || (prefBandwidthNoCellular != prefBandwidthNoCellularDefault)) {
            boolean newValue = true;
            // Observe PREF_BANDWIDTH on mobile network capable devices.
            if (isMobileNetworkCapable()) {
                if (mSharedPreferences.contains(PREF_BANDWIDTH_OLD)) {
                    @BandwidthType.Type
                    int prefetchBandwidthTypePref =
                            BandwidthType.getBandwidthFromTitle(prefBandwidth);
                    if (BandwidthType.Type.NEVER_PRERENDER == prefetchBandwidthTypePref) {
                        newValue = false;
                    } else if (BandwidthType.Type.PRERENDER_ON_WIFI == prefetchBandwidthTypePref
                            || BandwidthType.Type.ALWAYS_PRERENDER == prefetchBandwidthTypePref) {
                        newValue = true;
                    }
                }
            // Observe PREF_BANDWIDTH_NO_CELLULAR on devices without mobile network.
            } else {
                if (mSharedPreferences.contains(PREF_BANDWIDTH_NO_CELLULAR_OLD)) {
                    newValue = prefBandwidthNoCellular;
                }
            }
            // Save new value in Chrome PrefService.
            setNetworkPredictionEnabled(newValue);
        }

        // Delete old sharedPreferences.
        SharedPreferences.Editor sharedPreferencesEditor = mSharedPreferences.edit();
        // Delete PREF_BANDWIDTH and PREF_BANDWIDTH_NO_CELLULAR: just migrated these options.
        if (mSharedPreferences.contains(PREF_BANDWIDTH_OLD)) {
            sharedPreferencesEditor.remove(PREF_BANDWIDTH_OLD);
        }
        if (mSharedPreferences.contains(PREF_BANDWIDTH_NO_CELLULAR_OLD)) {
            sharedPreferencesEditor.remove(PREF_BANDWIDTH_NO_CELLULAR_OLD);
        }
        // Also delete ALLOW_PRERENDER, which was updated based on PREF_BANDWIDTH[_NO_CELLULAR] and
        // network connectivity type, therefore does not carry additional information.
        if (mSharedPreferences.contains(ALLOW_PRERENDER_OLD)) {
            sharedPreferencesEditor.remove(ALLOW_PRERENDER_OLD);
        }
        // Delete bool PREF_NETWORK_PREDICTIONS so that string values can be stored. Note that this
        // SharedPreference carries no information, because it used to be overwritten by
        // kNetworkPredictionEnabled on startup, and now it is overwritten by
        // kNetworkPredictionOptions on startup.
        if (mSharedPreferences.contains(PREF_NETWORK_PREDICTIONS)) {
            sharedPreferencesEditor.remove(PREF_NETWORK_PREDICTIONS);
        }
        sharedPreferencesEditor.apply();
    }

    protected boolean isNetworkAvailable() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();
        return (networkInfo != null && networkInfo.isConnected());
    }

    protected boolean isMobileNetworkCapable() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        // Android telephony team said it is OK to continue using getNetworkInfo() for our purposes.
        // We cannot use ConnectivityManager#getAllNetworks() because that one only reports enabled
        // networks. See crbug.com/532455.
        @SuppressWarnings("deprecation")
        NetworkInfo networkInfo =
                connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
        return networkInfo != null;
    }

    /**
     * Checks whether prerender should be allowed and updates the preference if it is not set yet.
     * @return Whether prerendering should be allowed.
     */
    public boolean shouldPrerender() {
        if (!DeviceClassManager.enablePrerendering()) return false;
        migrateNetworkPredictionPreferences();
        return canPrefetchAndPrerender();
    }

    /**
     * Sets the usage and crash reporting preference ON or OFF.
     *
     * @param enabled A boolean corresponding whether usage and crash reports uploads are allowed.
     */
    public void setUsageAndCrashReporting(boolean enabled) {
        mSharedPreferences.edit().putBoolean(PREF_METRICS_REPORTING, enabled).apply();
        syncUsageAndCrashReportingPrefs();
        if (!enabled) {
            SurveyController.getInstance().clearCache(ContextUtils.getApplicationContext());
        }
    }

    /**
     * Update usage and crash preferences based on Android preferences if possible in case they are
     * out of sync.
     */
    public void syncUsageAndCrashReportingPrefs() {
        // Skip if native browser process is not yet fully initialized.
        if (!BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER).isNativeStarted()) {
            return;
        }

        setMetricsReportingEnabled(isUsageAndCrashReportingPermittedByUser());
    }

    /**
     * Sets whether this client is in-sample for usage metrics and crash reporting. See
     * {@link org.chromium.chrome.browser.metrics.UmaUtils#isClientInMetricsSample} for details.
     */
    public void setClientInMetricsSample(boolean inSample) {
        mSharedPreferences.edit().putBoolean(PREF_METRICS_IN_SAMPLE, inSample).apply();
    }

    /**
     * Checks whether this client is in-sample for usage metrics and crash reporting. See
     * {@link org.chromium.chrome.browser.metrics.UmaUtils#isClientInMetricsSample} for details.
     *
     * @returns boolean Whether client is in-sample.
     */
    @Override
    public boolean isClientInMetricsSample() {
        // The default value is true to avoid sampling out crashes that occur before native code has
        // been initialized on first run. We'd rather have some extra crashes than none from that
        // time.
        return mSharedPreferences.getBoolean(PREF_METRICS_IN_SAMPLE, true);
    }

    /**
     * Checks whether uploading of crash dumps is permitted for the available network(s).
     *
     * @return whether uploading crash dumps is permitted.
     */
    @Override
    public boolean isNetworkAvailableForCrashUploads() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        return NetworkPermissionUtil.isNetworkUnmetered(connectivityManager);
    }

    /**
     * Checks whether uploading of usage metrics and crash dumps is currently permitted, based on
     * user consent only. This doesn't take network condition or experimental state (i.e. disabling
     * upload) into consideration. A crash dump may be retried if this check passes.
     *
     * @return whether the user has consented to reporting usage metrics and crash dumps.
     */
    @Override
    public boolean isUsageAndCrashReportingPermittedByUser() {
        return mSharedPreferences.getBoolean(PREF_METRICS_REPORTING, false);
    }

    /**
     * Check whether the command line switch is used to force uploading if at all possible. Used by
     * test devices to avoid UI manipulation.
     *
     * @return whether uploading should be enabled if at all possible.
     */
    @Override
    public boolean isUploadEnabledForTests() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_CRASH_DUMP_UPLOAD);
    }

    /**
     * @return Whether uploading usage metrics is currently permitted.
     */
    public boolean isMetricsUploadPermitted() {
        return isNetworkAvailable()
                && (isUsageAndCrashReportingPermittedByUser() || isUploadEnabledForTests());
    }

    /**
     * @return Whether there is a user set value for kNetworkPredictionOptions.  This should only be
     * used for preference migration. See http://crbug.com/334602
     */
    private boolean obsoleteNetworkPredictionOptionsHasUserSetting() {
        return PrivacyPreferencesManagerJni.get().obsoleteNetworkPredictionOptionsHasUserSetting();
    }

    /**
     * @return Network predictions preference.
     */
    public boolean getNetworkPredictionEnabled() {
        return PrivacyPreferencesManagerJni.get().getNetworkPredictionEnabled();
    }

    /**
     * Sets network predictions preference.
     */
    public void setNetworkPredictionEnabled(boolean enabled) {
        PrivacyPreferencesManagerJni.get().setNetworkPredictionEnabled(enabled);
    }

    /**
     * @return Whether Network Predictions is configured by policy.
     */
    public boolean isNetworkPredictionManaged() {
        return PrivacyPreferencesManagerJni.get().getNetworkPredictionManaged();
    }

    /**
     * Checks whether network predictions are allowed given preferences and current network
     * connection type.
     * @return Whether network predictions are allowed.
     */
    private boolean canPrefetchAndPrerender() {
        return PrivacyPreferencesManagerJni.get().canPrefetchAndPrerender();
    }

    /**
     * @return Whether usage and crash reporting pref is enabled.
     */
    public boolean isMetricsReportingEnabled() {
        return PrivacyPreferencesManagerJni.get().isMetricsReportingEnabled();
    }

    /**
     * Sets whether the usage and crash reporting pref should be enabled.
     */
    public void setMetricsReportingEnabled(boolean enabled) {
        PrivacyPreferencesManagerJni.get().setMetricsReportingEnabled(enabled);
    }

    /**
     * @return Whether usage and crash report pref is managed.
     */
    public boolean isMetricsReportingManaged() {
        return PrivacyPreferencesManagerJni.get().isMetricsReportingManaged();
    }

    @NativeMethods
    public interface Natives {
        boolean canPrefetchAndPrerender();
        boolean getNetworkPredictionManaged();
        boolean obsoleteNetworkPredictionOptionsHasUserSetting();
        boolean getNetworkPredictionEnabled();
        void setNetworkPredictionEnabled(boolean enabled);
        boolean isMetricsReportingEnabled();
        void setMetricsReportingEnabled(boolean enabled);
        boolean isMetricsReportingManaged();
    }
}
