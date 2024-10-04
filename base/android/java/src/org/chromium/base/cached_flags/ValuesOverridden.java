// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import org.chromium.build.annotations.CheckDiscard;

import java.util.HashMap;
import java.util.Map;

/** Keeps track of values overridden for testing for cached flags and field trial parameters. */
public abstract class ValuesOverridden {
    @CheckDiscard(
            "Should only exist in tests and in debug builds, should be optimized out in "
                    + "Release.")
    private static Map<String, String> sOverridesTestFeatures;

    /**
     * Set an |overrideValue| to be used in place of the disk value of a |preferenceKey| in tests.
     *
     * <p>Don't use directly from tests.
     */
    public static void setOverrideForTesting(String preferenceKey, String overrideValue) {
        if (sOverridesTestFeatures == null) {
            sOverridesTestFeatures = new HashMap<>();
        }
        sOverridesTestFeatures.put(preferenceKey, overrideValue);
    }

    /** Get the Map of boolean preferences with overridden values. */
    public static Boolean getBool(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        String stringValue = sOverridesTestFeatures.get(preferenceName);
        return stringValue != null ? Boolean.valueOf(stringValue) : null;
    }

    /** Get the Map of String preferences with overridden values. */
    public static String getString(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        return sOverridesTestFeatures.get(preferenceName);
    }

    /** Get the Map of int preferences with overridden values. */
    public static Integer getInt(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        String stringValue = sOverridesTestFeatures.get(preferenceName);
        return stringValue != null ? Integer.valueOf(stringValue) : null;
    }

    /** Get the Map of double preferences with overridden values. */
    public static Double getDouble(String preferenceName) {
        if (sOverridesTestFeatures == null) return null;
        String stringValue = sOverridesTestFeatures.get(preferenceName);
        return stringValue != null ? Double.valueOf(stringValue) : null;
    }

    /**
     * Remove all override values set.
     *
     * <p>Don't use directly from tests.
     */
    public static void removeOverrides() {
        sOverridesTestFeatures = null;
    }
}
