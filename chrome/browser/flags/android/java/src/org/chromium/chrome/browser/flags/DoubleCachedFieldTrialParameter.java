// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * A double-type {@link CachedFieldTrialParameter}.
 */
public class DoubleCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private double mDefaultValue;

    public DoubleCachedFieldTrialParameter(
            String featureName, String variationName, double defaultValue) {
        super(featureName, variationName, FieldTrialParameterType.DOUBLE, null);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    public double getValue() {
        return CachedFeatureFlags.getConsistentDoubleValue(
                getSharedPreferenceKey(), getDefaultValue());
    }

    public double getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        double value = ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                getFeatureName(), getParameterName(), getDefaultValue());
        SharedPreferencesManager.getInstance().writeDouble(getSharedPreferenceKey(), value);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * Caveat: this does not affect the value returned by native, only by
     * {@link CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    @VisibleForTesting
    public void setForTesting(double overrideValue) {
        CachedFeatureFlags.setOverrideTestValue(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
