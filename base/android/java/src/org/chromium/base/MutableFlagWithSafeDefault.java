// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/**
 * Flags of this type are un-cached flags that may be called before native,
 * but not primarily. They have good default values to use before native is loaded,
 * and will switch to using the native value once native is loaded.
 * These flags replace code like:
 * if (FeatureList.isInitialized() && SomeFeatureMap.isEnabled(featureName))
 * or
 * if (!FeatureList.isInitialized() || SomeFeatureMap.isEnabled(featureName)).
 */
public class MutableFlagWithSafeDefault extends Flag {
    private final boolean mDefaultValue;
    private Boolean mInMemoryCachedValue;

    public MutableFlagWithSafeDefault(
            FeatureMap featureMap, String featureName, boolean defaultValue) {
        super(featureMap, featureName);
        mDefaultValue = defaultValue;
    }

    /** Returns a new mutable boolean param with the given values. */
    public MutableBooleanParamWithSafeDefault newBooleanParam(
            String paramName, boolean defaultValue) {
        return new MutableBooleanParamWithSafeDefault(
                mFeatureMap, mFeatureName, paramName, defaultValue);
    }

    /** Returns a new mutable int param with the given values. */
    public MutableIntParamWithSafeDefault newIntParam(String paramName, int defaultValue) {
        return new MutableIntParamWithSafeDefault(
                mFeatureMap, mFeatureName, paramName, defaultValue);
    }

    @Override
    public boolean isEnabled() {
        if (mInMemoryCachedValue != null) return mInMemoryCachedValue;
        if (FeatureList.hasTestFeature(mFeatureName)) {
            return mFeatureMap.isEnabledInNative(mFeatureName);
        }

        if (FeatureList.isNativeInitialized()) {
            mInMemoryCachedValue = mFeatureMap.isEnabledInNative(mFeatureName);
            return mInMemoryCachedValue;
        }

        return mDefaultValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {
        mInMemoryCachedValue = null;
    }
}
