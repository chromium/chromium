// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.AnyThread;

import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** A String-type {@link CachedFieldTrialParameter}. */
public class StringCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private final String mDefaultValue;

    public StringCachedFieldTrialParameter(
            String featureName, String variationName, String defaultValue) {
        super(featureName, variationName, FieldTrialParameterType.STRING);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public String getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();
        String defaultValue = getDefaultValue();

        String value = ValuesOverridden.getString(preferenceName);
        if (value != null) {
            return value;
        }

        synchronized (ValuesReturned.sStringValues) {
            value = ValuesReturned.sStringValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value =
                    CachedFlagsSafeMode.getInstance()
                            .getStringFieldTrialParam(preferenceName, defaultValue);
            if (value == null) {
                value =
                        ChromeSharedPreferences.getInstance()
                                .readString(preferenceName, defaultValue);
            }

            ValuesReturned.sStringValues.put(preferenceName, value);
        }
        return value;
    }

    public String getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        String value =
                ChromeFeatureList.getFieldTrialParamByFeature(getFeatureName(), getParameterName());
        ChromeSharedPreferences.getInstance()
                .writeString(getSharedPreferenceKey(), value.isEmpty() ? getDefaultValue() : value);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * Caveat: this does not affect the value returned by native, only by
     * {@link CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(String overrideValue) {
        ValuesOverridden.setOverrideForTesting(getSharedPreferenceKey(), overrideValue);
    }
}
