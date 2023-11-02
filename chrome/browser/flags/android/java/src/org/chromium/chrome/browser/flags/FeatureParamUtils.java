// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import android.text.TextUtils;

/** Utility class for convenience functions for working feature params. */
public final class FeatureParamUtils {
    /** Private constructor to avoid instantiation. */
    private FeatureParamUtils() {}

    /**
     * Used to check a three state (true, false, not set) feature param and check if the current
     * value at runtime is different from the specified param value.
     * @param featureName Name the feature to fetch the param from.
     * @param paramName The name of the param to fetch.
     * @param currentValue The current value to match against the param value.
     * @return Whether the param exists and does not matches the current value.
     */
    public static boolean paramExistsAndDoesNotMatch(
            String featureName, String paramName, boolean currentValue) {
        Boolean paramValue = getFieldTrialParamByFeatureAsBooleanOrNull(featureName, paramName);
        return paramValue != null && paramValue != currentValue;
    }

    /**
     * Gets the given feature param as a nullable Boolean. If the feature/param is not set or does
     * not exist, then null will be returned. This gives the caller 3 effect states.
     * @param featureName Name the feature to fetch the param from.
     * @param paramName The name of the param to fetch.
     * @return Null if the param is not set, otherwise the value of the param parsed as a bool.
     *         Non-empty param values that do not look like a boolean will be treated as false.
     */
    public static Boolean getFieldTrialParamByFeatureAsBooleanOrNull(
            String featureName, String paramName) {
        String paramValue = ChromeFeatureList.getFieldTrialParamByFeature(featureName, paramName);
        return TextUtils.isEmpty(paramValue) ? null : Boolean.parseBoolean(paramValue);
    }
}
