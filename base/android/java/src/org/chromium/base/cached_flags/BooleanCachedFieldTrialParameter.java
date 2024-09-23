// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureMap;

/** A boolean-type {@link CachedFieldTrialParameter}. */
public class BooleanCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private boolean mDefaultValue;

    public BooleanCachedFieldTrialParameter(
            FeatureMap featureMap, String featureName, String variationName, boolean defaultValue) {
        super(featureMap, featureName, variationName, FieldTrialParameterType.BOOLEAN);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public boolean getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();
        boolean defaultValue = getDefaultValue();

        Boolean value = ValuesOverridden.getBool(preferenceName);
        if (value != null) {
            return value;
        }

        synchronized (ValuesReturned.sBoolValues) {
            value = ValuesReturned.sBoolValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value =
                    CachedFlagsSafeMode.getInstance()
                            .getBooleanFieldTrialParam(preferenceName, defaultValue);
            if (value == null) {
                value =
                        CachedFlagsSharedPreferences.getInstance()
                                .readBoolean(preferenceName, defaultValue);
            }

            ValuesReturned.sBoolValues.put(preferenceName, value);
        }
        return value;
    }

    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final boolean value =
                mFeatureMap.getFieldTrialParamByFeatureAsBoolean(
                        getFeatureName(), getParameterName(), getDefaultValue());
        editor.putBoolean(getSharedPreferenceKey(), value);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * <p>Caveat: this does not affect the value returned by native, only by {@link
     * CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(boolean overrideValue) {
        ValuesOverridden.setOverrideForTesting(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
