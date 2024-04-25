// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureMap;

/** A String-type {@link CachedFieldTrialParameter}. */
public class StringCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private final String mDefaultValue;

    public StringCachedFieldTrialParameter(
            FeatureMap featureMap, String featureName, String variationName, String defaultValue) {
        super(featureMap, featureName, variationName, FieldTrialParameterType.STRING);
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
                        CachedFlagsSharedPreferences.getInstance()
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
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final String value =
                mFeatureMap.getFieldTrialParamByFeature(getFeatureName(), getParameterName());
        editor.putString(getSharedPreferenceKey(), value.isEmpty() ? getDefaultValue() : value);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * <p>Caveat: this does not affect the value returned by native, only by {@link
     * CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(String overrideValue) {
        ValuesOverridden.setOverrideForTesting(getSharedPreferenceKey(), overrideValue);
    }
}
