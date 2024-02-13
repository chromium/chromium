// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureMap;
import org.chromium.base.Flag;
import org.chromium.base.shared_preferences.SharedPreferencesManager;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * CachedFlags are Flags that may be used before native is loaded and the FeatureList is
 * initialized.
 *
 * <p>They return a flag value read from native in a previous run, using SharedPreferences as
 * persistence.
 *
 * <p>@see {@link #isEnabled()} for more details about the logic.
 *
 * <p>To cache a flag from a {@link FeatureMap}, e.g. FooFeatureMap:
 *
 * <ul>
 *   <li>Create a static CachedFlag object in FooFeatureMap "sMyFlag"
 *   <li>Add it to the list FooFeatureMap#sFlagsCachedFullBrowser
 *   <li>Call {@code FooFeatureMap.sMyFlag.isEnabled()} to query whether the cached flag is enabled.
 *       Consider this the source of truth for whether the flag is turned on in the current session.
 * </ul>
 *
 * <p>Metrics caveat: For cached flags that are queried before native is initialized, when a new
 * experiment configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart.
 */
public class CachedFlag extends Flag {
    private final boolean mDefaultValue;
    private String mPreferenceKey;

    public CachedFlag(FeatureMap featureMap, String featureName, boolean defaultValue) {
        super(featureMap, featureName);
        mDefaultValue = defaultValue;
    }

    /**
     * Rules from highest to lowest priority:
     *
     * <ul>
     *   <li>1. If the flag has been forced by @EnableFeatures/@DisableFeatures or {@link
     *       CachedFlag#setForTesting}, the forced value is returned.
     *   <li>2. If a value was previously returned in the same run, the same value is returned for
     *       consistency.
     *   <li>3. If in a previous run, the value from {@link FeatureMap} was cached to SharedPrefs,
     *       it is returned.
     *   <li>4. The |defaultValue| passed as a constructor parameter is returned.
     * </ul>
     */
    @Override
    public boolean isEnabled() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();

        Boolean flag;
        synchronized (ValuesReturned.sBoolValues) {
            Map<String, Boolean> boolValuesReturned = ValuesReturned.sBoolValues;
            flag = boolValuesReturned.get(preferenceName);
            if (flag != null) {
                return flag;
            }

            flag =
                    CachedFlagsSafeMode.getInstance()
                            .isEnabled(mFeatureName, preferenceName, mDefaultValue);
            if (flag == null) {
                SharedPreferencesManager prefs = CachedFlagsSharedPreferences.getInstance();
                if (prefs.contains(preferenceName)) {
                    flag = prefs.readBoolean(preferenceName, false);
                } else {
                    flag = mDefaultValue;
                }
            }

            boolValuesReturned.put(preferenceName, flag);
        }
        return flag;
    }

    /**
     * @return the default value to be returned if no value is cached.
     */
    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {
        // ValuesReturned is cleared by CachedFlagUtils#resetFlagsForTesting().
    }

    /**
     * Forces a feature to be enabled or disabled for testing.
     *
     * @deprecated do not call this from tests; use @EnableFeatures/@DisableFeatures instead,
     * since batched tests need to be split by feature flag configuration.
     */
    @VisibleForTesting
    @Deprecated
    public void setForTesting(@Nullable Boolean value) {
        synchronized (ValuesReturned.sBoolValues) {
            ValuesReturned.sBoolValues.put(getSharedPreferenceKey(), value);
        }
    }

    /** Caches the value of the feature from {@link FeatureMap} to SharedPrefs. */
    void cacheFeature() {
        boolean isEnabledInNative = mFeatureMap.isEnabledInNative(mFeatureName);

        CachedFlagsSharedPreferences.getInstance()
                .writeBoolean(getSharedPreferenceKey(), isEnabledInNative);
    }

    String getSharedPreferenceKey() {
        // Create the key only once to avoid String concatenation every flag check.
        if (mPreferenceKey == null) {
            mPreferenceKey = CachedFlagsSharedPreferences.FLAGS_CACHED.createKey(mFeatureName);
        }
        return mPreferenceKey;
    }

    /**
     * Sets the feature flags to use in JUnit and instrumentation tests.
     *
     * @deprecated Do not call this from tests; use @EnableFeatures/@DisableFeatures annotations
     * instead.
     */
    @Deprecated
    public static void setFeaturesForTesting(Map<String, Boolean> features) {
        for (Map.Entry<String, Boolean> entry : features.entrySet()) {
            String featureName = entry.getKey();
            Boolean flagValue = entry.getValue();
            String sharedPreferencesKey =
                    CachedFlagsSharedPreferences.FLAGS_CACHED.createKey(featureName);
            synchronized (ValuesReturned.sBoolValues) {
                ValuesReturned.sBoolValues.put(sharedPreferencesKey, flagValue);
            }
        }
    }

    public static void resetDiskForTesting() {
        CachedFlagsSharedPreferences.getInstance()
                .removeKeysWithPrefix(CachedFlagsSharedPreferences.FLAGS_CACHED);
    }

    /** Create a Map of feature names -> {@link CachedFlag} from multiple lists of CachedFlags. */
    public static Map<String, CachedFlag> createCachedFlagMap(
            List<List<CachedFlag>> allCachedFlagsLists) {
        HashMap<String, CachedFlag> cachedFlagMap = new HashMap<>();
        for (List<CachedFlag> cachedFlagsList : allCachedFlagsLists) {
            for (CachedFlag cachedFlag : cachedFlagsList) {
                cachedFlagMap.put(cachedFlag.getFeatureName(), cachedFlag);
            }
        }
        return cachedFlagMap;
    }
}
