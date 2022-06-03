// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * A String-type {@link CachedFieldTrialParameter}.
 */
public class StringCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private final String mDefaultValue;

    public StringCachedFieldTrialParameter(
            String featureName, String variationName, String defaultValue) {
        this(featureName, variationName, defaultValue, null);
    }

    public StringCachedFieldTrialParameter(String featureName, String variationName,
            String defaultValue, String preferenceKeyOverride) {
        super(featureName, variationName, FieldTrialParameterType.STRING, preferenceKeyOverride);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    public String getValue() {
        return CachedFeatureFlags.getConsistentStringValue(
                getSharedPreferenceKey(), getDefaultValue());
    }

    public String getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        String value =
                ChromeFeatureList.getFieldTrialParamByFeature(getFeatureName(), getParameterName());
        SharedPreferencesManager.getInstance().writeString(
                getSharedPreferenceKey(), value.isEmpty() ? getDefaultValue() : value);
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
    public void setForTesting(String overrideValue) {
        CachedFeatureFlags.setOverrideTestValue(getSharedPreferenceKey(), overrideValue);
    }
}
