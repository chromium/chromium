// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.build.annotations.CheckDiscard;

import java.util.HashMap;
import java.util.Map;

/**
 * Keeps track of values overridden for testing for cached flags and field trial parameters.
 */
class ValuesOverridden {
    @CheckDiscard("Should only exist in tests and in debug builds, should be optimized out in "
            + "Release.")
    private static Map<String, String> sOverridesTestFeatures;

    static void setOverrideForTesting(String preferenceKey, String overrideValue) {
        if (sOverridesTestFeatures == null) {
            sOverridesTestFeatures = new HashMap<>();
        }
        sOverridesTestFeatures.put(preferenceKey, overrideValue);
    }

    static Boolean getBool(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        String stringValue = sOverridesTestFeatures.get(preferenceName);
        return stringValue != null ? Boolean.valueOf(stringValue) : null;
    }

    static String getString(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        return sOverridesTestFeatures.get(preferenceName);
    }

    static Integer getInt(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        String stringValue = sOverridesTestFeatures.get(preferenceName);
        return stringValue != null ? Integer.valueOf(stringValue) : null;
    }

    static Double getDouble(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        String stringValue = sOverridesTestFeatures.get(preferenceName);
        return stringValue != null ? Double.valueOf(stringValue) : null;
    }

    static void removeOverrides() {
        sOverridesTestFeatures = null;
    }
}
