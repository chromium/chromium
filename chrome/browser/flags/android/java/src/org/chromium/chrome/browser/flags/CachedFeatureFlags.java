// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.AnyThread;

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
 *
 * For cached flags that are queried before native is initialized, when a new experiment
 * configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart, when the static Boolean is reset to the newly cached
 * value in shared preferences.
 */
public class CachedFeatureFlags {
    /**
     * Sets the feature flags to use in JUnit and instrumentation tests.
     *
     * @deprecated Do not call this from tests; use @EnableFeatures/@DisableFeatures annotations
     * instead.
     */
    @Deprecated
    public static void setFeaturesForTesting(Map<String, Boolean> features) {
        assert features != null;

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
     * Caches flags that must take effect on startup but are set via native code.
     */
    public static void cacheFieldTrialParameters(List<CachedFieldTrialParameter> parameters) {
        for (CachedFieldTrialParameter parameter : parameters) {
            parameter.cacheToDisk();
        }
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

        Boolean value = ValuesOverridden.getBool(preferenceName);
        if (value != null) {
            return value;
        }

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

        String value = ValuesOverridden.getString(preferenceName);
        if (value != null) {
            return value;
        }

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

        Integer value = ValuesOverridden.getInt(preferenceName);
        if (value != null) {
            return value;
        }

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

        Double value = ValuesOverridden.getDouble(preferenceName);
        if (value != null) {
            return value;
        }

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
        ValuesOverridden.removeOverrides();
        CachedFlagsSafeMode.getInstance().clearMemoryForTesting();
    }

    static void setOverrideForTesting(String preferenceKey, String overrideValue) {
        ValuesOverridden.setOverrideForTesting(preferenceKey, overrideValue);
    }
}
