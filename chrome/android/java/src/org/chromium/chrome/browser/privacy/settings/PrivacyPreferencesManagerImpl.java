// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;
import org.chromium.components.policy.PolicyMap;
import org.chromium.components.policy.PolicyService;

/**
 * Manages preferences related to privacy, metrics reporting, prerendering, and network prediction.
 */
public class PrivacyPreferencesManagerImpl implements PrivacyPreferencesManager {
    @SuppressLint("StaticFieldLeak")
    private static PrivacyPreferencesManagerImpl sInstance;

    private final Context mContext;
    private final SharedPreferencesManager mPrefs;
    private PolicyService mPolicyService;
    private PolicyService.Observer mPolicyServiceObserver;

    // Supplier for other class to observe. Null until the supplier is requested.
    private @Nullable ObservableSupplierImpl<Boolean> mCrashUploadPermittedSupplier;

    private boolean mNativeInitialized;

    PrivacyPreferencesManagerImpl(Context context) {
        mContext = context;
        mPrefs = SharedPreferencesManager.getInstance();
        mNativeInitialized = false;
        // TODO(https://crbug.com/1320040). Clean up deprecated preference migration.
        migrateDeprecatedPreferences();
    }

    public static PrivacyPreferencesManagerImpl getInstance() {
        if (sInstance == null) {
            sInstance = new PrivacyPreferencesManagerImpl(ContextUtils.getApplicationContext());
        }
        return sInstance;
    }

    public static void setInstanceForTesting(PrivacyPreferencesManagerImpl instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    public void onNativeInitialized() {
        if (mNativeInitialized) return;

        mNativeInitialized = true;

        createPolicyServiceObserver();
    }

    protected void createPolicyServiceObserver() {
        if (mPolicyService != null) {
            return;
        }

        mPolicyService = PolicyServiceFactory.getGlobalPolicyService();

        mPolicyServiceObserver = new PolicyService.Observer() {
            @Override
            public void onPolicyServiceInitialized() {
                syncUsageAndCrashReportingPermittedByPolicy();
            }

            @Override
            public void onPolicyUpdated(PolicyMap previous, PolicyMap current) {
                syncUsageAndCrashReportingPermittedByPolicy();
            }
        };

        if (mPolicyService.isInitializationComplete()) {
            syncUsageAndCrashReportingPermittedByPolicy();
        }

        mPolicyService.addObserver(mPolicyServiceObserver);
    }

    protected void migrateDeprecatedPreferences() {
        if (mPrefs.contains(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING)) {
            mPrefs.writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER,
                    mPrefs.readBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING, false));
            mPrefs.removeKey(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING);
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

    public void syncUsageAndCrashReportingPermittedByPolicy() {
        // Skip if native browser process is not yet fully initialized.
        if (!mNativeInitialized) return;

        mPrefs.writeBoolean(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY,
                !PrivacyPreferencesManagerImplJni.get().isMetricsReportingDisabledByPolicy());
    }

    @Override
    public void addObserver(Observer observer) {
        getUsageAndCrashReportingPermittedObservableSupplier().addObserver(
                observer::onIsUsageAndCrashReportingPermittedChanged);
    }

    @Override
    public void removeObserver(Observer observer) {
        getUsageAndCrashReportingPermittedObservableSupplier().removeObserver(
                observer::onIsUsageAndCrashReportingPermittedChanged);
    }

    @Override
    public void setUsageAndCrashReporting(boolean enabled) {
        mPrefs.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, enabled);
        syncUsageAndCrashReportingPrefs();
    }

    @Override
    public void syncUsageAndCrashReportingPrefs() {
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
        return mPrefs.readBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY, true);
    }

    @Override
    public boolean isUsageAndCrashReportingPermittedByUser() {
        return mPrefs.readBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);
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
        getUsageAndCrashReportingPermittedObservableSupplier().set(enabled);
    }

    @Override
    public ObservableSupplierImpl<Boolean> getUsageAndCrashReportingPermittedObservableSupplier() {
        ThreadUtils.assertOnUiThread();
        if (mCrashUploadPermittedSupplier == null) {
            mCrashUploadPermittedSupplier = new ObservableSupplierImpl<>();
            mCrashUploadPermittedSupplier.set(isUsageAndCrashReportingPermitted());
        }
        return mCrashUploadPermittedSupplier;
    }

    @NativeMethods
    public interface Natives {
        boolean isMetricsReportingEnabled();
        void setMetricsReportingEnabled(boolean enabled);
        boolean isMetricsReportingDisabledByPolicy();
    }
}
