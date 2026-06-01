// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.metrics.MetricsReportingLevel;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;
import org.chromium.components.policy.PolicyMap;
import org.chromium.components.policy.PolicyService;

/**
 * Manages preferences related to privacy, metrics reporting, prerendering, and network prediction.
 */
@NullMarked
public class PrivacyPreferencesManagerImpl implements PrivacyPreferencesManager {
    @SuppressLint("StaticFieldLeak")
    private static @Nullable PrivacyPreferencesManagerImpl sInstance;

    private final Context mContext;
    private final SharedPreferencesManager mPrefs;
    private @Nullable PolicyService mPolicyService;
    private PolicyService.@Nullable Observer mPolicyServiceObserver;

    // Supplier for other class to observe. Null until the supplier is requested.
    private @Nullable SettableNonNullObservableSupplier<Boolean> mCrashUploadPermittedSupplier;

    private volatile boolean mNativeInitialized;

    PrivacyPreferencesManagerImpl(Context context) {
        mContext = context;
        mPrefs = ChromeSharedPreferences.getInstance();
        mNativeInitialized = false;
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

        // Cache the metrics restructurization state in SharedPreferences immediately when
        // native is initialized so that it is safe to access pre-native in future sessions.
        mPrefs.writeBoolean(
                ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE,
                PrivacyPreferencesManagerImplJni.get().shouldUseMetricsChoiceRestructure());

        createPolicyServiceObserver();
    }

    protected void createPolicyServiceObserver() {
        if (mPolicyService != null) {
            return;
        }

        mPolicyService = PolicyServiceFactory.getGlobalPolicyService();

        mPolicyServiceObserver =
                new PolicyService.Observer() {
                    @Override
                    public void onPolicyServiceInitialized() {
                        syncUsageAndCrashReportingPermittedByPolicy();
                        syncMetricsReportingDisabledByPolicy();
                    }

                    @Override
                    public void onPolicyUpdated(PolicyMap previous, PolicyMap current) {
                        syncUsageAndCrashReportingPermittedByPolicy();
                        syncMetricsReportingDisabledByPolicy();
                    }
                };

        if (mPolicyService.isInitializationComplete()) {
            syncUsageAndCrashReportingPermittedByPolicy();
            syncMetricsReportingDisabledByPolicy();
        }
        mPolicyService.addObserver(mPolicyServiceObserver);
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
        // networks. See crbug.com/40435982.
        @SuppressWarnings("deprecation")
        NetworkInfo networkInfo =
                connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
        return networkInfo != null;
    }

    /**
     * Get the supplier for isUsageAndCrashReportingPermitted. If the supplier is null, initialize a
     * new one. Ui Thread only.
     */
    protected SettableNonNullObservableSupplier<Boolean> getCrashUploadPermittedSupplier() {
        ThreadUtils.assertOnUiThread();
        if (mCrashUploadPermittedSupplier == null) {
            mCrashUploadPermittedSupplier =
                    ObservableSuppliers.createNonNull(isUsageAndCrashReportingPermitted());
        }
        return mCrashUploadPermittedSupplier;
    }

    public void syncUsageAndCrashReportingPermittedByPolicy() {
        assert mNativeInitialized;

        mPrefs.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY,
                !PrivacyPreferencesManagerImplJni.get().isMetricsReportingDisabledByPolicy());
    }

    public void syncMetricsReportingDisabledByPolicy() {
        assert mNativeInitialized;

        mPrefs.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY,
                PrivacyPreferencesManagerImplJni.get().isMetricsReportingDisabledByPolicy());

        getCrashUploadPermittedSupplier().set(isMetricsReportingEnabled());
    }

    @Override
    public void setUsageAndCrashReporting(boolean enabled) {
        mPrefs.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, enabled);
        syncUsageAndCrashReportingPrefs();
    }

    @Override
    public void setMetricsReportingLevel(@MetricsReportingLevel int level) {
        mPrefs.writeInt(ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL, level);
        PrivacyPreferencesManagerImplJni.get().setMetricsReportingLevelInLocalState(level);
        getCrashUploadPermittedSupplier().set(isMetricsReportingEnabled());
    }

    @Override
    public void syncUsageAndCrashReportingPrefs() {
        setMetricsReportingEnabled(isUsageAndCrashReportingPermitted());
    }

    @Override
    public void setClientInSampleForMetrics(boolean inSample) {
        mPrefs.writeBoolean(ChromePreferenceKeys.PRIVACY_IN_SAMPLE_FOR_METRICS, inSample);
    }

    @Override
    public boolean isClientInSampleForMetrics() {
        // The default value is true to avoid sampling out metrics that occur before native code has
        // been initialized on first run. I.e., clients are presumed to be in-sample until we know
        // otherwise. Note that metrics reporting is also gated on the user's pref, not just being
        // in-sample.
        return mPrefs.readBoolean(ChromePreferenceKeys.PRIVACY_IN_SAMPLE_FOR_METRICS, true);
    }

    @Override
    public void setClientInSampleForCrashes(boolean inSampleForCrash) {
        mPrefs.writeBoolean(ChromePreferenceKeys.PRIVACY_IN_SAMPLE_FOR_CRASHES, inSampleForCrash);
    }

    @Override
    public boolean isClientInSampleForCrashes() {
        // The default value is true to avoid sampling out crashes that occur before native code has
        // been initialized on first run.  I.e., clients are presumed to be in-sample until we know
        // otherwise. Note that crash reporting is also gated on the user's pref, not just being
        // in-sample.
        return mPrefs.readBoolean(ChromePreferenceKeys.PRIVACY_IN_SAMPLE_FOR_CRASHES, true);
    }

    @Override
    public boolean isNetworkAvailableForCrashUploads() {
        ConnectivityManager connectivityManager =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        return NetworkPermissionUtil.isNetworkUnmetered(connectivityManager);
    }

    @Override
    public boolean isUsageAndCrashReportingPermittedByPolicy() {
        if (shouldUseMetricsChoiceRestructure()) {
            return !mPrefs.readBoolean(
                    ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_DISABLED_BY_POLICY, false);
        }
        return mPrefs.readBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_POLICY, true);
    }

    @Override
    public boolean isUsageAndCrashReportingPermittedByUser() {
        if (shouldUseMetricsChoiceRestructure()) {
            @MetricsReportingLevel
            int level =
                    mPrefs.readInt(
                            ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_LEVEL,
                            MetricsReportingLevel.NONE);
            switch (level) {
                case MetricsReportingLevel.NONE:
                    return false;
                case MetricsReportingLevel.BASIC:
                case MetricsReportingLevel.ADVANCED:
                    return true;
                default:
                    return false;
            }
        }
        return mPrefs.readBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);
    }

    @Override
    public boolean isUploadEnabledForTests() {
        CommandLine commandLine = CommandLine.getInstance();
        return commandLine != null && commandLine.hasSwitch(ChromeSwitches.FORCE_CRASH_DUMP_UPLOAD);
    }

    @Override
    public boolean isMetricsUploadPermitted() {
        return isNetworkAvailable()
                && (isUsageAndCrashReportingPermitted() || isUploadEnabledForTests());
    }

    @Override
    public boolean isMetricsReportingEnabled() {
        return PrivacyPreferencesManagerImplJni.get().isBasicMetricsReportingEnabled();
    }

    @Override
    public void setMetricsReportingEnabled(boolean enabled) {
        PrivacyPreferencesManagerImplJni.get().setMetricsReportingEnabled(enabled);
        getCrashUploadPermittedSupplier().set(enabled);
    }

    @Override
    public NonNullObservableSupplier<Boolean>
            getUsageAndCrashReportingPermittedObservableSupplier() {
        return getCrashUploadPermittedSupplier();
    }

    public boolean shouldUseMetricsChoiceRestructure() {
        // This method can be called before the native library is loaded/initialized (e.g.,
        // in background services or binder threads checking crash uploading permission).
        // To avoid UnsatisfiedLinkError, we read the value cached in SharedPreferences.
        // The cache is populated in onNativeInitialized() using the latest C++ state.
        return mPrefs.readBoolean(
                ChromePreferenceKeys.PRIVACY_SHOULD_USE_METRICS_CHOICE_RESTRUCTURE, false);
    }

    @NativeMethods
    public interface Natives {
        boolean isBasicMetricsReportingEnabled();

        void setMetricsReportingEnabled(boolean enabled);

        boolean isMetricsReportingDisabledByPolicy();

        void setMetricsReportingLevelInLocalState(
                @JniType("metrics::MetricsReportingLevel") int level);

        boolean shouldUseMetricsChoiceRestructure();
    }
}
