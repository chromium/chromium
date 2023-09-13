// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.AnyThread;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * An int-type {@link CachedFieldTrialParameter}.
 */
public class IntCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private int mDefaultValue;

    public IntCachedFieldTrialParameter(
            String featureName, String variationName, int defaultValue) {
        super(featureName, variationName, FieldTrialParameterType.INT);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public int getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();
        int defaultValue = getDefaultValue();

        Integer value = ValuesOverridden.getInt(preferenceName);
        if (value != null) {
            return value;
        }

        synchronized (ValuesReturned.sIntValues) {
            value = ValuesReturned.sIntValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = CachedFlagsSafeMode.getInstance().getIntFieldTrialParam(
                    preferenceName, defaultValue);
            if (value == null) {
                value = SharedPreferencesManager.getInstance().readInt(
                        preferenceName, defaultValue);
            }

            ValuesReturned.sIntValues.put(preferenceName, value);
        }
        return value;
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
    public void setForTesting(int overrideValue) {
        ValuesOverridden.setOverrideForTesting(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
