// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureMap;

/** An int-type {@link CachedFieldTrialParameter}. */
public class IntCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private int mDefaultValue;

    public IntCachedFieldTrialParameter(
            FeatureMap featureMap, String featureName, String variationName, int defaultValue) {
        super(featureMap, featureName, variationName, FieldTrialParameterType.INT);
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

            value =
                    CachedFlagsSafeMode.getInstance()
                            .getIntFieldTrialParam(preferenceName, defaultValue);
            if (value == null) {
                value =
                        CachedFlagsSharedPreferences.getInstance()
                                .readInt(preferenceName, defaultValue);
            }

            ValuesReturned.sIntValues.put(preferenceName, value);
        }
        return value;
    }

    public int getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final int value =
                mFeatureMap.getFieldTrialParamByFeatureAsInt(
                        getFeatureName(), getParameterName(), getDefaultValue());
        editor.putInt(getSharedPreferenceKey(), value);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * <p>Caveat: this does not affect the value returned by native, only by {@link
     * CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(int overrideValue) {
        ValuesOverridden.setOverrideForTesting(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
