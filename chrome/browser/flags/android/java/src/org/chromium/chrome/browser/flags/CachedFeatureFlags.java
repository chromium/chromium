// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.AnyThread;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.FieldTrialList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.util.List;
import java.util.Map;

/**
 * A class to cache the state of flags from {@link ChromeFeatureList}.
 *
 * It caches certain feature flags that must take effect on startup before native is initialized.
 * ChromeFeatureList can only be queried through native code. The caching is done in
 * {@link android.content.SharedPreferences}, which is available in Java immediately.
 *
 * To cache a flag from ChromeFeatureList:
 * - Create a static CachedFlag object in {@link ChromeFeatureList} "sMyFlag"
 * - Add it to the list passed to {@code ChromeCachedFlags#cacheNativeFlags()}.
 * - Call {@code ChromeFeatureList.sMyFlag.isEnabled()} to query whether the cached flag is enabled.
 *   Consider this the source of truth for whether the flag is turned on in the current session.
 * - When querying whether a cached feature is enabled from native, call IsJavaDrivenFeatureEnabled
 *   in cached_feature_flags.h.
 *
 * For cached flags that are queried before native is initialized, when a new experiment
 * configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart, when the static Boolean is reset to the newly cached
 * value in shared preferences.
 */
public class CachedFeatureFlags {
    private static ValuesOverridden sValuesOverridden = new ValuesOverridden();

    private static String sReachedCodeProfilerTrialGroup;

    @CalledByNative
    @AnyThread
    static boolean isEnabled(String featureName) {
        CachedFlag cachedFlag = ChromeFeatureList.sAllCachedFlags.get(featureName);
        assert cachedFlag != null;

        return cachedFlag.isEnabled();
    }

    /**
     * Sets the feature flags to use in JUnit and instrumentation tests.
     *
     * Do not call this from tests; use @EnableFeatures/@DisableFeatures annotations instead.
     */
    public static void setFeaturesForTesting(Map<String, Boolean> features) {
        assert features != null;

        sValuesOverridden.enableOverrides();

        CachedFlag.setFeaturesForTesting(features);
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheNativeFlags(List<CachedFlag> featuresToCache) {
        for (CachedFlag feature : featuresToCache) {
            feature.cacheFeature();
        }
    }

    /**
     * Caches a predetermined list of flags that must take effect on startup but are set via native
     * code.
     *
     * Do not add new simple boolean flags here, use {@link #cacheNativeFlags} instead.
     */
    public static void cacheAdditionalNativeFlags() {
        cacheNetworkServiceWarmUpEnabled();
        CachedFlagsSafeMode.getInstance().cacheSafeModeForCachedFlagsEnabled();
        cacheReachedCodeProfilerTrialGroup();

        // Propagate REACHED_CODE_PROFILER feature value to LibraryLoader. This can't be done in
        // LibraryLoader itself because it lives in //base and can't depend on ChromeFeatureList.
        LibraryLoader.setReachedCodeProfilerEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.REACHED_CODE_PROFILER),
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.REACHED_CODE_PROFILER, "sampling_interval_us", 0));

        // Similarly, propagate the BACKGROUND_THREAD_POOL feature value to LibraryLoader.
        LibraryLoader.setBackgroundThreadPoolEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.BACKGROUND_THREAD_POOL));

        // Propagate the CACHE_ACTIVITY_TASKID feature value to ApplicationStatus.
        ApplicationStatus.setCachingEnabled(
                ChromeFeatureList.isEnabled(ChromeFeatureList.CACHE_ACTIVITY_TASKID));
    }

    /**
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheFieldTrialParameters(List<CachedFieldTrialParameter> parameters) {
        for (CachedFieldTrialParameter parameter : parameters) {
            parameter.cacheToDisk();
        }
    }

    public static void cacheMinimalBrowserFlagsTimeFromNativeTime() {
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                System.currentTimeMillis());
    }

    /**
     * Returns the time (in millis) the minimal browser flags were cached.
     */
    public static long getLastCachedMinimalBrowserFlagsTimeMillis() {
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS, 0);
    }

    /**
     * Cache whether warming up network service process is enabled, so that the value
     * can be made available immediately on next start up.
     */
    private static void cacheNetworkServiceWarmUpEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FLAGS_CACHED_NETWORK_SERVICE_WARM_UP_ENABLED,
                CachedFeatureFlagsJni.get().isNetworkServiceWarmUpEnabled());
    }

    /**
     * @return whether warming up network service is enabled.
     */
    public static boolean isNetworkServiceWarmUpEnabled() {
        return getConsistentBooleanValue(
                ChromePreferenceKeys.FLAGS_CACHED_NETWORK_SERVICE_WARM_UP_ENABLED, false);
    }

    /**
     * Caches the trial group of the reached code profiler feature to be using on next startup.
     */
    private static void cacheReachedCodeProfilerTrialGroup() {
        // Make sure that the existing value is saved in a static variable before overwriting it.
        if (sReachedCodeProfilerTrialGroup == null) {
            getReachedCodeProfilerTrialGroup();
        }

        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP,
                FieldTrialList.findFullName(ChromeFeatureList.REACHED_CODE_PROFILER));
    }

    /**
     * @return The trial group of the reached code profiler.
     */
    @CalledByNative
    public static String getReachedCodeProfilerTrialGroup() {
        if (sReachedCodeProfilerTrialGroup == null) {
            sReachedCodeProfilerTrialGroup = SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.REACHED_CODE_PROFILER_GROUP, "");
        }

        return sReachedCodeProfilerTrialGroup;
    }

    /**
     * Call when entering an initialization flow that should result in caching flags.
     */
    public static void onStartOrResumeCheckpoint() {
        CachedFlagsSafeMode.getInstance().onStartOrResumeCheckpoint();
    }

    /**
     * Call when aborting an initialization flow that would have resulted in caching flags.
     */
    public static void onPauseCheckpoint() {
        CachedFlagsSafeMode.getInstance().onPauseCheckpoint();
    }

    /**
     * Call when finishing an initialization flow with flags having been cached successfully.
     */
    public static void onEndCheckpoint() {
        CachedFlagsSafeMode.getInstance().onEndCheckpoint();
    }

    public static @CachedFlagsSafeMode.Behavior int getSafeModeBehaviorForTesting() {
        return CachedFlagsSafeMode.getInstance().getBehaviorForTesting();
    }

    @AnyThread
    static boolean getConsistentBooleanValue(String preferenceName, boolean defaultValue) {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getBool(preferenceName, defaultValue);
        }

        Boolean value;
        synchronized (ValuesReturned.sBoolValues) {
            value = ValuesReturned.sBoolValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = CachedFlagsSafeMode.getInstance().getBooleanFieldTrialParam(
                    preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readBoolean(
                        preferenceName, defaultValue);
            }

            ValuesReturned.sBoolValues.put(preferenceName, value);
        }
        return value;
    }

    @AnyThread
    static String getConsistentStringValue(String preferenceName, String defaultValue) {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getString(preferenceName, defaultValue);
        }

        String value;
        synchronized (ValuesReturned.sStringValues) {
            value = ValuesReturned.sStringValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = CachedFlagsSafeMode.getInstance().getStringFieldTrialParam(
                    preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readString(
                        preferenceName, defaultValue);
            }

            ValuesReturned.sStringValues.put(preferenceName, value);
        }
        return value;
    }

    @AnyThread
    static int getConsistentIntValue(String preferenceName, int defaultValue) {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getInt(preferenceName, defaultValue);
        }

        Integer value;
        synchronized (ValuesReturned.sIntValues) {
            value = ValuesReturned.sIntValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = CachedFlagsSafeMode.getInstance().getIntFieldTrialParam(
                    preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readInt(
                        preferenceName, defaultValue);
            }

            ValuesReturned.sIntValues.put(preferenceName, value);
        }
        return value;
    }

    @AnyThread
    static double getConsistentDoubleValue(String preferenceName, double defaultValue) {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getDouble(preferenceName, defaultValue);
        }

        Double value;
        synchronized (ValuesReturned.sDoubleValues) {
            value = ValuesReturned.sDoubleValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = CachedFlagsSafeMode.getInstance().getDoubleFieldTrialParam(
                    preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readDouble(
                        preferenceName, defaultValue);
            }

            ValuesReturned.sDoubleValues.put(preferenceName, value);
        }
        return value;
    }

    public static void resetFlagsForTesting() {
        ValuesReturned.clearForTesting();
        sValuesOverridden.removeOverrides();
        CachedFlagsSafeMode.getInstance().clearMemoryForTesting();
    }

    static void setOverrideForTesting(String preferenceKey, String overrideValue) {
        sValuesOverridden.setOverrideForTesting(preferenceKey, overrideValue);
    }

    static void setSafeModeExperimentEnabledForTesting(Boolean value) {
        CachedFlagsSafeMode.getInstance().setExperimentEnabledForTesting(value);
    }

    @NativeMethods
    interface Natives {
        boolean isNetworkServiceWarmUpEnabled();
    }
}
