// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/** Boolean {@link FeatureParam} that will return a default value before native is loaded. */
public class MutableBooleanParamWithSafeDefault extends MutableParamWithSafeDefault<Boolean> {
    public MutableBooleanParamWithSafeDefault(
            FeatureMap featureMap, String featureName, String paramName, boolean defaultValue) {
        super(featureMap, featureName, paramName, defaultValue);
    }

    /** Returns the value of this param as a primitive boolean. */
    public boolean getValue() {
        return getValueBoxed();
    }

    @Override
    protected Boolean readValueFromFeatureMap() {
        return mFeatureMap.getFieldTrialParamByFeatureAsBoolean(
                mFeatureName, mParamName, mDefaultValue);
    }
}
