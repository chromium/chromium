// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/** Boolean {@link FeatureParam} that will return a default value before native is loaded. */
public class MutableIntParamWithSafeDefault extends MutableParamWithSafeDefault<Integer> {
    public MutableIntParamWithSafeDefault(
            FeatureMap featureMap, String featureName, String paramName, int defaultValue) {
        super(featureMap, featureName, paramName, defaultValue);
    }

    /** Returns the value of this param as a primitive int. */
    public int getValue() {
        return getValueBoxed();
    }

    @Override
    protected Integer readValueFromFeatureMap() {
        return mFeatureMap.getFieldTrialParamByFeatureAsInt(
                mFeatureName, mParamName, mDefaultValue);
    }
}
