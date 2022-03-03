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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
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

    @VisibleForTesting
    public static void setInstanceForTesting(PrivacyPreferencesManagerImpl instance) {
        sInstance = instance;
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
    public void setUsageAndCrashReporting(boolean enabled) {
        mPrefs.writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING, enabled);
        syncUsageAndCrashReportingPrefs();
    }

    @Override
    public void syncUsageAndCrashReportingPrefs() {
        // Skip if native browser process is not yet fully initialized.
        if (!BrowserStartupController.getInstance().isNativeStarted()) return;

        setMetricsReportingEnabled(isUsageAndCrashReportingPermitted());
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
    public boolean isUsageAndCrashReportingPermittedByPolicy() {
        // TODO(https://crbug.com/1301701) This function is being called from an invalid thread.
        // This constant return value is set while figuring out the problem.
        return true;
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
                && (isUsageAndCrashReportingPermitted() || isUploadEnabledForTests());
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
    public boolean isMetricsReportingDisabledByPolicy() {
        return PrivacyPreferencesManagerImplJni.get().isMetricsReportingDisabledByPolicy();
    }

    @NativeMethods
    public interface Natives {
        boolean isMetricsReportingEnabled();
        void setMetricsReportingEnabled(boolean enabled);
        boolean isMetricsReportingDisabledByPolicy();
    }
}
