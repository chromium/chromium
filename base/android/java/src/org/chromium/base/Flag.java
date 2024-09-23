// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.BuildConfig;

import java.util.HashMap;

import javax.annotation.concurrent.NotThreadSafe;

/**
 * Defines a feature flag for use in Java.
 *
 * <p>Duplicate flag definitions are not permitted, so only a single instance can be created with a
 * given feature name.
 *
 * <p>To instantiate a Flag, use a concrete subclass, i.e. CachedFlag, MutableFlagWithSafeDefault or
 * PostNativeFlag.
 *
 * <p>This class and its subclasses are not thread safe.
 */
@NotThreadSafe
public abstract class Flag {
    // Used to reset all flags between tests.
    private static HashMap<String, Flag> sFlagsCreatedForTesting = new HashMap<>();

    protected final FeatureMap mFeatureMap;
    protected final String mFeatureName;

    protected Flag(FeatureMap featureMap, String featureName) {
        mFeatureMap = featureMap;
        mFeatureName = featureName;

        if (BuildConfig.IS_FOR_TEST) {
            Flag previous = sFlagsCreatedForTesting.put(mFeatureName, this);
            assert previous == null : "Duplicate flag creation for feature: " + featureName;
        }
    }

    /** Returns the unique name of the feature flag. */
    public String getFeatureName() {
        return mFeatureName;
    }

    /**
     * Checks if a feature flag is enabled.
     * @return whether the feature should be considered enabled.
     */
    public abstract boolean isEnabled();

    protected abstract void clearInMemoryCachedValueForTesting();

    /**
     * Resets the in-memory cache of every Flag instance. This shouldn't be used directly by
     * individual tests other than those that exercise Flag subclasses.
     */
    public static void resetAllInMemoryCachedValuesForTesting() {
        for (Flag flag : sFlagsCreatedForTesting.values()) {
            flag.clearInMemoryCachedValueForTesting();
        }
    }

    /**
     * Use an empty sFlagsCreated map for this test instead of carrying over from and to other tests
     * in the same process (batched or Robolectric).
     */
    public static void useTemporaryFlagsCreatedForTesting() {
        HashMap<String, Flag> oldValues = sFlagsCreatedForTesting;
        sFlagsCreatedForTesting = new HashMap<>();
        ResettersForTesting.register(() -> sFlagsCreatedForTesting = oldValues);
    }
}
