// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.AnyThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.common.collect.ImmutableMap;

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
    /**
     * Non-dynamic preference keys used historically for specific features.
     *
     * Do not add new values to this list. To add a new cached feature flag, just follow the
     * instructions in the class javadoc.
     */
    private static final Map<String, String> sNonDynamicPrefKeys =
            ImmutableMap.<String, String>builder()
                    .put(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED,
                            ChromePreferenceKeys.FLAGS_CACHED_COMMAND_LINE_ON_NON_ROOTED_ENABLED)
                    .put(ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                            ChromePreferenceKeys
                                    .FLAGS_CACHED_SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT)
                    .put(ChromeFeatureList.START_SURFACE_ANDROID,
                            ChromePreferenceKeys.FLAGS_CACHED_START_SURFACE_ENABLED)
                    .put(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                            ChromePreferenceKeys.FLAGS_CACHED_GRID_TAB_SWITCHER_ENABLED)
                    .put(ChromeFeatureList.TAB_GROUPS_ANDROID,
                            ChromePreferenceKeys.FLAGS_CACHED_TAB_GROUPS_ANDROID_ENABLED)
                    .build();

    private static ValuesReturned sValuesReturned = new ValuesReturned();
    private static ValuesOverridden sValuesOverridden = new ValuesOverridden();
    private static CachedFlagsSafeMode sSafeMode = new CachedFlagsSafeMode();

    private static String sReachedCodeProfilerTrialGroup;

    /**
     * Rules from highest to lowest priority:
     * 1. If the flag has been forced by @EnableFeatures/@DisableFeatures or
     *    {@link CachedFlag#setForTesting}, the forced value is returned.
     * 2. If a value was previously returned in the same run, the same value is returned for
     *    consistency.
     * 3. If native is loaded, the value from {@link ChromeFeatureList} is returned.
     * 4. If in a previous run, the value from {@link ChromeFeatureList} was cached to SharedPrefs,
     *    it is returned.
     * 5. The default value passed as a parameter is returned.
     */
    @CalledByNative
    @AnyThread
    static boolean isEnabled(String featureName, boolean defaultValue) {
        sSafeMode.onFlagChecked();

        String preferenceName = getPrefForFeatureFlag(featureName);

        Boolean flag;
        synchronized (sValuesReturned.boolValues) {
            flag = sValuesReturned.boolValues.get(preferenceName);
            if (flag != null) {
                return flag;
            }

            flag = sSafeMode.isEnabled(featureName, preferenceName, defaultValue);
            if (flag == null) {
                SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
                if (prefs.contains(preferenceName)) {
                    flag = prefs.readBoolean(preferenceName, false);
                } else {
                    flag = defaultValue;
                }
            }

            sValuesReturned.boolValues.put(preferenceName, flag);
        }
        return flag;
    }

    /**
     * Caches the value of a feature from {@link ChromeFeatureList} to SharedPrefs.
     *
     * @param featureName the feature name from ChromeFeatureList.
     */
    static void cacheFeature(String featureName) {
        String preferenceName = getPrefForFeatureFlag(featureName);
        boolean isEnabledInNative = ChromeFeatureList.isEnabled(featureName);
        SharedPreferencesManager.getInstance().writeBoolean(preferenceName, isEnabledInNative);
    }

    /**
     * Forces a feature to be enabled or disabled for testing.
     *
     * Do not call this from tests; use @EnableFeatures/@DisableFeatures annotations or
     *      {@link CachedFlag#setForTesting(Boolean)} instead.
     *
     * @param featureName the feature name from ChromeFeatureList.
     * @param value the value that {@link CachedFlag#isEnabled()} will be forced to return. If null,
     *     remove any values previously forced.
     */
    static void setForTesting(String featureName, @Nullable Boolean value) {
        String preferenceName = getPrefForFeatureFlag(featureName);
        synchronized (sValuesReturned.boolValues) {
            sValuesReturned.boolValues.put(preferenceName, value);
        }
    }

    /**
     * Sets the feature flags to use in JUnit and instrumentation tests.
     *
     * Do not call this from tests; use @EnableFeatures/@DisableFeatures annotations or
     *      {@link CachedFlag#setForTesting(Boolean)} instead.
     */
    @VisibleForTesting
    public static void setFeaturesForTesting(Map<String, Boolean> features) {
        assert features != null;

        sValuesOverridden.enableOverrides();

        for (Map.Entry<String, Boolean> entry : features.entrySet()) {
            setForTesting(entry.getKey(), entry.getValue());
        }
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
        sSafeMode.cacheSafeModeForCachedFlagsEnabled();
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
        sSafeMode.onStartOrResumeCheckpoint();
    }

    /**
     * Call when aborting an initialization flow that would have resulted in caching flags.
     */
    public static void onPauseCheckpoint() {
        sSafeMode.onPauseCheckpoint();
    }

    /**
     * Call when finishing an initialization flow with flags having been cached successfully.
     */
    public static void onEndCheckpoint() {
        sSafeMode.onEndCheckpoint(sValuesReturned);
    }

    public static @CachedFlagsSafeMode.Behavior int getSafeModeBehaviorForTesting() {
        return sSafeMode.getBehaviorForTesting();
    }

    @AnyThread
    static boolean getConsistentBooleanValue(String preferenceName, boolean defaultValue) {
        sSafeMode.onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getBool(preferenceName, defaultValue);
        }

        Boolean value;
        synchronized (sValuesReturned.boolValues) {
            value = sValuesReturned.boolValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = sSafeMode.getBooleanFieldTrialParam(preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readBoolean(
                        preferenceName, defaultValue);
            }

            sValuesReturned.boolValues.put(preferenceName, value);
        }
        return value;
    }

    @AnyThread
    static String getConsistentStringValue(String preferenceName, String defaultValue) {
        sSafeMode.onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getString(preferenceName, defaultValue);
        }

        String value;
        synchronized (sValuesReturned.stringValues) {
            value = sValuesReturned.stringValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = sSafeMode.getStringFieldTrialParam(preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readString(
                        preferenceName, defaultValue);
            }

            sValuesReturned.stringValues.put(preferenceName, value);
        }
        return value;
    }

    @AnyThread
    static int getConsistentIntValue(String preferenceName, int defaultValue) {
        sSafeMode.onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getInt(preferenceName, defaultValue);
        }

        Integer value;
        synchronized (sValuesReturned.intValues) {
            value = sValuesReturned.intValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = sSafeMode.getIntFieldTrialParam(preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readInt(
                        preferenceName, defaultValue);
            }

            sValuesReturned.intValues.put(preferenceName, value);
        }
        return value;
    }

    @AnyThread
    static double getConsistentDoubleValue(String preferenceName, double defaultValue) {
        sSafeMode.onFlagChecked();

        if (sValuesOverridden.isEnabled()) {
            return sValuesOverridden.getDouble(preferenceName, defaultValue);
        }

        Double value;
        synchronized (sValuesReturned.doubleValues) {
            value = sValuesReturned.doubleValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = sSafeMode.getDoubleFieldTrialParam(preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readDouble(
                        preferenceName, defaultValue);
            }

            sValuesReturned.doubleValues.put(preferenceName, value);
        }
        return value;
    }

    @AnyThread
    private static String getPrefForFeatureFlag(String featureName) {
        String legacyPrefKey = sNonDynamicPrefKeys.get(featureName);
        if (legacyPrefKey == null) {
            return ChromePreferenceKeys.FLAGS_CACHED.createKey(featureName);
        } else {
            return legacyPrefKey;
        }
    }

    @VisibleForTesting
    public static void resetFlagsForTesting() {
        sValuesReturned.clearForTesting();
        sValuesOverridden.removeOverrides();
        sSafeMode.clearMemoryForTesting();
    }

    @VisibleForTesting
    public static void resetDiskForTesting() {
        SharedPreferencesManager.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.FLAGS_CACHED);
        for (Map.Entry<String, String> e : sNonDynamicPrefKeys.entrySet()) {
            String prefKey = e.getValue();
            SharedPreferencesManager.getInstance().removeKey(prefKey);
        }
    }

    @VisibleForTesting
    static void setOverrideTestValue(String preferenceKey, String overrideValue) {
        sValuesOverridden.setOverrideTestValue(preferenceKey, overrideValue);
    }

    @VisibleForTesting
    static void setSafeModeExperimentEnabledForTesting(Boolean value) {
        sSafeMode.setExperimentEnabledForTesting(value);
    }

    @NativeMethods
    interface Natives {
        boolean isNetworkServiceWarmUpEnabled();
    }
}
