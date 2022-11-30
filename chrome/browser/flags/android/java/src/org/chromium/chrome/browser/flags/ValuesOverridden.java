// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.CheckDiscard;

import java.util.HashMap;
import java.util.Map;

/**
 * Keeps track of values overridden for testing for cached flags and field trial parameters.
 */
class ValuesOverridden {
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    private Map<String, String> mOverridesTestFeatures;

    @VisibleForTesting
    void setOverrideTestValue(String preferenceKey, String overrideValue) {
        enableOverrides();
        mOverridesTestFeatures.put(preferenceKey, overrideValue);
    }

    void enableOverrides() {
        // Do not overwrite if there are already existing overridden features in
        // sOverridesTestFeatures.
        if (mOverridesTestFeatures == null) {
            mOverridesTestFeatures = new HashMap<>();
        }
    }

    boolean isEnabled() {
        return mOverridesTestFeatures != null;
    }

    boolean getBool(String preferenceName, boolean defaultValue) {
        String value = mOverridesTestFeatures.get(preferenceName);
        if (value != null) {
            return Boolean.valueOf(value);
        }
        return defaultValue;
    }

    String getString(String preferenceName, String defaultValue) {
        String stringValue = mOverridesTestFeatures.get(preferenceName);
        if (stringValue != null) {
            return stringValue;
        }
        return defaultValue;
    }

    int getInt(String preferenceName, int defaultValue) {
        String stringValue = mOverridesTestFeatures.get(preferenceName);
        if (stringValue != null) {
            return Integer.valueOf(stringValue);
        }
        return defaultValue;
    }

    double getDouble(String preferenceName, double defaultValue) {
        String stringValue = mOverridesTestFeatures.get(preferenceName);
        if (stringValue != null) {
            return Double.valueOf(stringValue);
        }
        return defaultValue;
    }

    void removeOverrides() {
        mOverridesTestFeatures = null;
    }
}
