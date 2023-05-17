// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Flag;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Flags of this type may be used before native is loaded and return the value read
 * from native and cached to SharedPreferences in a previous run.
 * @see {@link CachedFeatureFlags#isEnabled(String, boolean)}
 */
public class CachedFlag extends Flag {
    private final boolean mDefaultValue;

    public CachedFlag(String featureName, boolean defaultValue) {
        super(featureName);
        mDefaultValue = defaultValue;
    }

    @Override
    public boolean isEnabled() {
        return CachedFeatureFlags.isEnabled(this);
    }

    /**
     * @return the default value to be returned if no value is cached.
     */
    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {}

    @VisibleForTesting
    public void setForTesting(@Nullable Boolean value) {
        CachedFeatureFlags.setForTesting(mFeatureName, value);
    }

    public void cacheFeature() {
        CachedFeatureFlags.cacheFeature(mFeatureName);
    }

    /**
     * Create a Map of feature names -> {@link CachedFlag} from multiple lists of CachedFlags.
     */
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
