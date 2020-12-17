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
        boolean getNetworkPredictionEnabled();
        void setNetworkPredictionEnabled(boolean enabled);
        boolean isMetricsReportingEnabled();
        void setMetricsReportingEnabled(boolean enabled);
        boolean isMetricsReportingManaged();
    }
}
