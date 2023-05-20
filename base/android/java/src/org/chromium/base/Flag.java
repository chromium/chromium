// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.util.HashMap;

import javax.annotation.concurrent.NotThreadSafe;

/**
 * Defines a feature flag for use in Java.
 *
 * Duplicate flag definitions are not permitted, so only a single
 * instance can be created with a given feature name.
 *
 * To create a flag, instantiate a concrete subclass, i.e. CachedFlag, MutableFlagWithSafeDefault or
 * PostNativeFlag.
 *
 * This class and its subclasses are not thread safe.
 */
@NotThreadSafe
public abstract class Flag {
    private static HashMap<String, Flag> sFlagsCreated = new HashMap<>();
    protected final String mFeatureName;
    protected Boolean mValue;

    protected Flag(String featureName) {
        assert !sFlagsCreated.containsKey(featureName);
        mFeatureName = featureName;
        sFlagsCreated.put(mFeatureName, this);
    }

    /**
     * @return the unique name of the feature flag.
     */
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
     * Resets the list of active flag instances. This shouldn't be used directly by individual
     * tests other than those that exercise Flag subclasses.
     */
    public static void resetFlagsForTesting() {
        resetAllInMemoryCachedValuesForTesting();
        sFlagsCreated.clear();
    }

    /**
     * Resets the in-memory cache of every Flag instance. This shouldn't be used directly by
     * individual tests other than those that exercise Flag subclasses.
     */
    public static void resetAllInMemoryCachedValuesForTesting() {
        for (Flag flag : sFlagsCreated.values()) {
            flag.clearInMemoryCachedValueForTesting();
        }
    }
}
