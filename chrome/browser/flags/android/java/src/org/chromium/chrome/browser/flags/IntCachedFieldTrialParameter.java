// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * An int-type {@link CachedFieldTrialParameter}.
 */
public class IntCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private int mDefaultValue;

    public IntCachedFieldTrialParameter(
            String featureName, String variationName, int defaultValue) {
        super(featureName, variationName, FieldTrialParameterType.INT, null);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    public int getValue() {
        return CachedFeatureFlags.getConsistentIntValue(
                getSharedPreferenceKey(), getDefaultValue());
    }

    public int getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        int value = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                getFeatureName(), getParameterName(), getDefaultValue());
        SharedPreferencesManager.getInstance().writeInt(getSharedPreferenceKey(), value);
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
    public void setForTesting(int overrideValue) {
        CachedFeatureFlags.setOverrideTestValue(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
