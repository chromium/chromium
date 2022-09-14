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
 * - Set its default value by adding an entry to {@link #sDefaults}.
 * - Add it to the list passed to {@link ChromeCachedFlags#cacheNativeFlags(List)}.
 * - Call {@link #isEnabled(String)} to query whether the cached flag is enabled.
 *   Consider this the source of truth for whether the flag is turned on in the current session.
 * - When querying whether a cached feature is enabled from native, a @CalledByNative method can be
 *   exposed in this file to allow feature_utilities.cc to retrieve the cached value.
 *
 * For cached flags that are queried before native is initialized, when a new experiment
 * configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart, when the static Boolean is reset to the newly cached
 * value in shared preferences.
 */
public class CachedFeatureFlags {
    /**
     * Stores the default values for each feature flag queried, used as a fallback in case native
     * isn't loaded, and no value has been previously cached.
     */
    private static Map<String, Boolean> sDefaults =
            ImmutableMap.<String, Boolean>builder()
                    .put(ChromeFeatureList.ANONYMOUS_UPDATE_CHECKS, true)
                    .put(ChromeFeatureList.APP_MENU_MOBILE_SITE_OPTION, false)
                    .put(ChromeFeatureList.BACK_GESTURE_REFACTOR, false)
                    .put(ChromeFeatureList.CCT_BRAND_TRANSPARENCY, false)
                    .put(ChromeFeatureList.CCT_INCOGNITO, true)
                    .put(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY, false)
                    .put(ChromeFeatureList.CCT_REMOVE_REMOTE_VIEW_IDS, true)
                    .put(ChromeFeatureList.CCT_RESIZABLE_90_MAXIMUM_HEIGHT, false)
                    .put(ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE, false)
                    .put(ChromeFeatureList.CCT_RESIZABLE_FOR_FIRST_PARTIES, true)
                    .put(ChromeFeatureList.COMMERCE_COUPONS, false)
                    .put(ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES, false)
                    .put(ChromeFeatureList.CCT_TOOLBAR_CUSTOMIZATIONS, true)
                    .put(ChromeFeatureList.CCT_RESIZABLE_WINDOW_ABOVE_NAVBAR, true)
                    .put(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS, false)
                    .put(ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED, false)
                    .put(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID, false)
                    .put(ChromeFeatureList.CREATE_SAFEBROWSING_ON_STARTUP, false)
                    .put(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA, false)
                    .put(ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE, true)
                    .put(ChromeFeatureList.EARLY_LIBRARY_LOAD, true)
                    .put(ChromeFeatureList.ELASTIC_OVERSCROLL, true)
                    .put(ChromeFeatureList.ELIDE_PRIORITIZATION_OF_PRE_NATIVE_BOOTSTRAP_TASKS, true)
                    .put(ChromeFeatureList.EXPERIMENTS_FOR_AGSA, true)
                    .put(ChromeFeatureList.FEED_LOADING_PLACEHOLDER, false)
                    .put(ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS, false)
                    .put(ChromeFeatureList.IMMERSIVE_UI_MODE, false)
                    .put(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, false)
                    .put(ChromeFeatureList.INSTANCE_SWITCHER, true)
                    .put(ChromeFeatureList.INSTANT_START, false)
                    .put(ChromeFeatureList.INTEREST_FEED_V2, true)
                    .put(ChromeFeatureList.LENS_CAMERA_ASSISTED_SEARCH, false)
                    .put(ChromeFeatureList.NEW_WINDOW_APP_MENU, true)
                    .put(ChromeFeatureList.OMAHA_MIN_SDK_VERSION_ANDROID, false)
                    .put(ChromeFeatureList.OMNIBOX_ANDROID_AUXILIARY_SEARCH, false)
                    .put(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE, false)
                    .put(ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, false)
                    .put(ChromeFeatureList.OSK_RESIZES_VISUAL_VIEWPORT, false)
                    .put(ChromeFeatureList.PAINT_PREVIEW_DEMO, false)
                    .put(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP, false)
                    .put(ChromeFeatureList.QUERY_TILES, false)
                    .put(ChromeFeatureList.QUERY_TILES_ON_START, false)
                    .put(ChromeFeatureList.PREFETCH_NOTIFICATION_SCHEDULING_INTEGRATION, false)
                    .put(ChromeFeatureList.READ_LATER, false)
                    .put(ChromeFeatureList.START_SURFACE_ANDROID, true)
                    .put(ChromeFeatureList.START_SURFACE_RETURN_TIME, false)
                    .put(ChromeFeatureList.START_SURFACE_REFACTOR, false)
                    .put(ChromeFeatureList.STORE_HOURS, false)
                    .put(ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT, true)
                    .put(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, true)
                    .put(ChromeFeatureList.TAB_GROUPS_ANDROID, true)
                    .put(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID, false)
                    .put(ChromeFeatureList.TAB_GROUPS_FOR_TABLETS, false)
                    .put(ChromeFeatureList.TAB_STRIP_IMPROVEMENTS, false)
                    .put(ChromeFeatureList.TAB_TO_GTS_ANIMATION, true)
                    .put(ChromeFeatureList.TEST_DEFAULT_DISABLED, false)
                    .put(ChromeFeatureList.TEST_DEFAULT_ENABLED, true)
                    .put(ChromeFeatureList.TOOLBAR_USE_HARDWARE_BITMAP_DRAW, false)
                    .put(ChromeFeatureList.USE_CHIME_ANDROID_SDK, false)
                    .put(ChromeFeatureList.WEB_APK_TRAMPOLINE_ON_INITIAL_INTENT, true)
                    .build();
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
                    .put(ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE,
                            ChromePreferenceKeys.FLAGS_CACHED_DOWNLOAD_AUTO_RESUMPTION_IN_NATIVE)
                    .put(ChromeFeatureList.IMMERSIVE_UI_MODE,
                            ChromePreferenceKeys.FLAGS_CACHED_IMMERSIVE_UI_MODE_ENABLED)
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
     * Checks if a cached feature flag is enabled.
     *
     * Requires that the feature be registered in {@link #sDefaults}.
     *
     * @param featureName the feature name from ChromeFeatureList.
     * @return whether the cached feature should be considered enabled.
     */
    public static boolean isEnabled(String featureName) {
        // All cached feature flags should have a default value.
        if (!sDefaults.containsKey(featureName)) {
            throw new IllegalArgumentException(
                    "Feature " + featureName + " has no default in CachedFeatureFlags.");
        }
        return isEnabled(featureName, sDefaults.get(featureName));
    }

    /**
     * Rules from highest to lowest priority:
     * 1. If the flag has been forced by {@link #setForTesting}, the forced value is returned.
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
     * @param featureName the feature name from ChromeFeatureList.
     * @param value the value that {@link #isEnabled(String)} will be forced to return. If null,
     *     remove any values previously forced.
     */
    public static void setForTesting(String featureName, @Nullable Boolean value) {
        String preferenceName = getPrefForFeatureFlag(featureName);
        synchronized (sValuesReturned.boolValues) {
            sValuesReturned.boolValues.put(preferenceName, value);
        }
    }

    /**
     * Sets the feature flags to use in JUnit and instrumentation tests.
     */
    @VisibleForTesting
    public static void setFeaturesForTesting(Map<String, Boolean> features) {
        assert features != null;

        sValuesOverridden.enableOverrides();

        for (Map.Entry<String, Boolean> entry : features.entrySet()) {
            String key = entry.getKey();

            if (!sDefaults.containsKey(key)) {
                continue;
            }

            setForTesting(key, entry.getValue());
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
        for (Map.Entry<String, Boolean> e : sDefaults.entrySet()) {
            String prefKey = ChromePreferenceKeys.FLAGS_CACHED.createKey(e.getKey());
            SharedPreferencesManager.getInstance().removeKey(prefKey);
        }
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
    public static Map<String, Boolean> swapDefaultsForTesting(Map<String, Boolean> testDefaults) {
        Map<String, Boolean> swapped = sDefaults;
        sDefaults = testDefaults;
        return swapped;
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
