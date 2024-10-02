// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import androidx.annotation.GuardedBy;

import java.util.HashMap;
import java.util.Map;

/** Keeps track of values returned for cached flags and field trial parameters. */
public abstract class ValuesReturned {
    @GuardedBy("sBoolValues")
    static final Map<String, Boolean> sBoolValues = new HashMap<>();

    @GuardedBy("sStringValues")
    static final Map<String, String> sStringValues = new HashMap<>();

    @GuardedBy("sIntValues")
    static final Map<String, Integer> sIntValues = new HashMap<>();

    @GuardedBy("sDoubleValues")
    static final Map<String, Double> sDoubleValues = new HashMap<>();

    /**
     * Forget the values returned this run for tests. New values will be calculated when needed.
     *
     * <p>Do not call this directly from tests.
     */
    public static void clearForTesting() {
        synchronized (sBoolValues) {
            sBoolValues.clear();
        }
        synchronized (sStringValues) {
            sStringValues.clear();
        }
        synchronized (sIntValues) {
            sIntValues.clear();
        }
        synchronized (sDoubleValues) {
            sDoubleValues.clear();
        }
    }

    /**
     * Sets the feature flags to use in JUnit and instrumentation tests.
     *
     * <p>Do not call this from tests; use @EnableFeatures/@DisableFeatures annotations instead.
     */
    public static void setFeaturesForTesting(Map<String, Boolean> features) {
        synchronized (sBoolValues) {
            for (Map.Entry<String, Boolean> entry : features.entrySet()) {
                String featureName = entry.getKey();
                Boolean flagValue = entry.getValue();
                String sharedPreferencesKey =
                        CachedFlagsSharedPreferences.FLAGS_CACHED.createKey(featureName);
                sBoolValues.put(sharedPreferencesKey, flagValue);
            }
        }
    }
}
