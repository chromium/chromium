// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.AnyThread;

import org.chromium.base.FeatureMap;

import java.util.Map;

/** AllCachedFieldTrialParameters caches all the parameters for a feature. */
public class AllCachedFieldTrialParameters extends CachedFieldTrialParameter {

    public AllCachedFieldTrialParameters(FeatureMap featureMap, String featureName) {
        // As this includes all parameters, the parameterName is empty.
        super(featureMap, featureName, /* parameterName= */ "", FieldTrialParameterType.ALL);
    }

    /** Returns a map of field trial parameter to value. */
    @AnyThread
    public Map<String, String> getParams() {
        return CachedFlagsSharedPreferences.decodeJsonEncodedMap(getConsistentStringValue());
    }

    @AnyThread
    private String getConsistentStringValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();

        String value = ValuesOverridden.getString(preferenceName);
        if (value != null) {
            return value;
        }

        synchronized (ValuesReturned.sStringValues) {
            value = ValuesReturned.sStringValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = CachedFlagsSafeMode.getInstance().getStringFieldTrialParam(preferenceName, "");
            if (value == null) {
                value = CachedFlagsSharedPreferences.getInstance().readString(preferenceName, "");
            }

            ValuesReturned.sStringValues.put(preferenceName, value);
        }
        return value;
    }

    @Override
    void writeCacheValueToEditor(final SharedPreferences.Editor editor) {
        final Map<String, String> params =
                mFeatureMap.getFieldTrialParamsForFeature(getFeatureName());
        editor.putString(
                getSharedPreferenceKey(), CachedFlagsSharedPreferences.encodeParams(params));
    }

    /** Sets the parameters for the specified feature when used in tests. */
    public static void setForTesting(String featureName, Map<String, String> params) {
        String preferenceKey =
                CachedFlagsSharedPreferences.generateParamSharedPreferenceKey(featureName, "");
        String overrideValue = CachedFlagsSharedPreferences.encodeParams(params);
        ValuesOverridden.setOverrideForTesting(preferenceKey, overrideValue);
    }
}
