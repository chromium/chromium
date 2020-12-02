// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.survey.SurveyController;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;
import org.chromium.content_public.browser.BrowserStartupController;

/**
 * Manages preferences related to privacy, metrics reporting, prerendering, and network prediction.
 */
public class PrivacyPreferencesManagerImpl implements PrivacyPreferencesManager {
    @SuppressLint("StaticFieldLeak")
    private static PrivacyPreferencesManagerImpl sInstance;

    private final Context mContext;
    private final SharedPreferencesManager mPrefs;

    @VisibleForTesting
    PrivacyPreferencesManagerImpl(Context context) {
        mContext = context;
        mPrefs = SharedPreferencesManager.getInstance();
    }

    public static PrivacyPreferencesManagerImpl getInstance() {
        if (sInstance == null) {
            sInstance = new PrivacyPreferencesManagerImpl(ContextUtils.getApplicationContext());
        }
        return sInstance;
    }

    @Override
    public void migrateNetworkPredictionPreferences() {
        // See if PREF_NETWORK_PREDICTIONS is an old boolean value.
        boolean predictionOptionIsBoolean = false;
        try {
            mPrefs.readString(ChromePreferenceKeys.PRIVACY_NETWORK_PREDICTIONS, "");
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
                && !mPrefs.contains(ChromePreferenceKeys.PRIVACY_BANDWIDTH_OLD)
                && !mPrefs.contains(ChromePreferenceKeys.PRIVACY_BANDWIDTH_NO_CELLULAR_OLD)) {
            return;
        }

        // Migrate if the old preferences are at their default values.
        // (Note that for PREF_BANDWIDTH*, if the setting is default, then there is no way to tell
        // whether the user has set it.)
        final String prefBandwidthDefault =
                BandwidthType.title(BandwidthType.Type.PRERENDER_ON_WIFI);
        final String prefBandwidth =
                mPrefs.readString(ChromePreferenceKeys.PRIVACY_BANDWIDTH_OLD, prefBandwidthDefault);
        boolean prefBandwidthNoCellularDefault = true;
        boolean prefBandwidthNoCellular =
                mPrefs.readBoolean(ChromePreferenceKeys.PRIVACY_BANDWIDTH_NO_CELLULAR_OLD,
                        prefBandwidthNoCellularDefault);

        if (!(prefBandwidthDefault.equals(prefBandwidth))
                || (prefBandwidthNoCellular != prefBandwidthNoCellularDefault)) {
            boolean newValue = true;
            // Observe PREF_BANDWIDTH on mobile network capable devices.
            if (isMobileNetworkCapable()) {
                if (mPrefs.contains(ChromePreferenceKeys.PRIVACY_BANDWIDTH_OLD)) {
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
            }
            // Observe PREF_BANDWIDTH_NO_CELLULAR on devices without mobile network.
            else {
                if (mPrefs.contains(ChromePreferenceKeys.PRIVACY_BANDWIDTH_NO_CELLULAR_OLD)) {
                    newValue = prefBandwidthNoCellular;
                }
            }
            // Save new value in Chrome PrefService.
            setNetworkPredictionEnabled(newValue);
        }

        // Delete old sharedPreferences.

        // Delete PREF_BANDWIDTH and PREF_BANDWIDTH_NO_CELLULAR: just migrated these options.
        if (mPrefs.contains(ChromePreferenceKeys.PRIVACY_BANDWIDTH_OLD)) {
            mPrefs.removeKey(ChromePreferenceKeys.PRIVACY_BANDWIDTH_OLD);
        }
        if (mPrefs.contains(ChromePreferenceKeys.PRIVACY_BANDWIDTH_NO_CELLULAR_OLD)) {
            mPrefs.removeKey(ChromePreferenceKeys.PRIVACY_BANDWIDTH_NO_CELLULAR_OLD);
        }
        // Also delete ALLOW_PRERENDER, which was updated based on PREF_BANDWIDTH[_NO_CELLULAR] and
        // network connectivity type, therefore does not carry additional information.
        if (mPrefs.contains(ChromePreferenceKeys.PRIVACY_ALLOW_PRERENDER_OLD)) {
            mPrefs.removeKey(ChromePreferenceKeys.PRIVACY_ALLOW_PRERENDER_OLD);
        }
        // Delete bool PREF_NETWORK_PREDICTIONS so that string values can be stored. Note that this
        // SharedPreference carries no information, because it used to be overwritten by
        // kNetworkPredictionEnabled on startup, and now it is overwritten by
        // kNetworkPredictionOptions on startup.
        if (mPrefs.contains(ChromePreferenceKeys.PRIVACY_NETWORK_PREDICTIONS)) {
            mPrefs.removeKey(ChromePreferenceKeys.PRIVACY_NETWORK_PREDICTIONS);
        }
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

    @Override
    public boolean shouldPrerender() {
        if (!DeviceClassManager.enablePrerendering()) return false;
        migrateNetworkPredictionPreferences();
        return canPrefetchAndPrerender();
    }

    @Override
    public void setUsageAndCrashReporting(boolean enabled) {
        mPrefs.writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING, enabled);
        syncUsageAndCrashReportingPrefs();
        if (!enabled) {
            SurveyController.getInstance().clearCache(ContextUtils.getApplicationContext());
        }
    }

    @Override
    public void syncUsageAndCrashReportingPrefs() {
        // Skip if native browser process is not yet fully initialized.
        if (!BrowserStartupController.getInstance().isNativeStarted()) return;

        setMetricsReportingEnabled(isUsageAndCrashReportingPermittedByUser());
    }

    @Override
    public void setClientInMetricsSample(boolean inSample) {
        mPrefs.writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_IN_SAMPLE, inSample);
    }

    @Override
    public boolean isClientInMetricsSample() {
        // The default value is true to avoid sampling out crashes that occur before native code has
        // been initialized on first run. We'd rather have some extra crashes than none from that
        // time.
        return mPrefs.readBoolean(ChromePreferenceKeys.PRIVACY_METRICS_IN_SAMPLE, true);
    }

    @Override
    public boolean isNetworkAvailableForCrashUploads() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        return NetworkPermissionUtil.isNetworkUnmetered(connectivityManager);
    }

    @Override
    public boolean isUsageAndCrashReportingPermittedByUser() {
        return mPrefs.readBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING, false);
    }

    @Override
    public boolean isUploadEnabledForTests() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_CRASH_DUMP_UPLOAD);
    }

    @Override
    public boolean isMetricsUploadPermitted() {
        return isNetworkAvailable()
                && (isUsageAndCrashReportingPermittedByUser() || isUploadEnabledForTests());
    }

    /**
     * Checks whether network predictions are allowed given preferences and current network
     * connection type.
     * @return Whether network predictions are allowed.
     */
    private boolean canPrefetchAndPrerender() {
        return PrivacyPreferencesManagerImplJni.get().canPrefetchAndPrerender();
    }

    @Override
    public boolean isMetricsReportingEnabled() {
        return PrivacyPreferencesManagerImplJni.get().isMetricsReportingEnabled();
    }

    @Override
    public void setMetricsReportingEnabled(boolean enabled) {
        PrivacyPreferencesManagerImplJni.get().setMetricsReportingEnabled(enabled);
    }

    @Override
    public boolean isMetricsReportingManaged() {
        return PrivacyPreferencesManagerImplJni.get().isMetricsReportingManaged();
    }

    /**
     * @return Whether there is a user set value for kNetworkPredictionOptions.  This should only be
     * used for preference migration. See http://crbug.com/334602
     */
    private boolean obsoleteNetworkPredictionOptionsHasUserSetting() {
        return PrivacyPreferencesManagerImplJni.get()
                .obsoleteNetworkPredictionOptionsHasUserSetting();
    }

    @Override
    public boolean getNetworkPredictionEnabled() {
        return PrivacyPreferencesManagerImplJni.get().getNetworkPredictionEnabled();
    }

    @Override
    public void setNetworkPredictionEnabled(boolean enabled) {
        PrivacyPreferencesManagerImplJni.get().setNetworkPredictionEnabled(enabled);
    }

    @Override
    public boolean isNetworkPredictionManaged() {
        return PrivacyPreferencesManagerImplJni.get().getNetworkPredictionManaged();
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
