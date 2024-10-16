// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.cached_flags;

import android.content.SharedPreferences;

import androidx.annotation.GuardedBy;

import org.chromium.base.supplier.Supplier;

import java.util.HashMap;
import java.util.Map;

/** Keeps track of values returned for cached flags and field trial parameters. */
public abstract class ValuesReturned {
    @GuardedBy("sBoolValues")
    private static final Map<String, Boolean> sBoolValues = new HashMap<>();

    @GuardedBy("sStringValues")
    private static final Map<String, String> sStringValues = new HashMap<>();

    @GuardedBy("sIntValues")
    private static final Map<String, Integer> sIntValues = new HashMap<>();

    @GuardedBy("sDoubleValues")
    private static final Map<String, Double> sDoubleValues = new HashMap<>();

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
     *
     * <p>TODO(crbug.com/40281605): Store these in ValuesOverridden, or better, merge with overrides
     * of FeatureList.
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

    /** Get a previously returned value or uses |valueSupplier| to determine it and store it. */
    public static boolean getReturnedOrNewBoolValue(String key, Supplier<Boolean> valueSupplier) {
        synchronized (sBoolValues) {
            Boolean value = sBoolValues.get(key);
            if (value == null) {
                value = valueSupplier.get();
            }
            sBoolValues.put(key, value);
            return value;
        }
    }

    /** Get a previously returned value or uses |valueSupplier| to determine it and store it. */
    public static String getReturnedOrNewStringValue(String key, Supplier<String> valueSupplier) {
        synchronized (sStringValues) {
            String value = sStringValues.get(key);
            if (value == null) {
                value = valueSupplier.get();
            }
            sStringValues.put(key, value);
            return value;
        }
    }

    /** Get a previously returned value or uses |valueSupplier| to determine it and store it. */
    public static int getReturnedOrNewIntValue(String key, Supplier<Integer> valueSupplier) {
        synchronized (sIntValues) {
            Integer value = sIntValues.get(key);
            if (value == null) {
                value = valueSupplier.get();
            }
            sIntValues.put(key, value);
            return value;
        }
    }

    /** Get a previously returned value or uses |valueSupplier| to determine it and store it. */
    public static double getReturnedOrNewDoubleValue(String key, Supplier<Double> valueSupplier) {
        synchronized (sDoubleValues) {
            Double value = sDoubleValues.get(key);
            if (value == null) {
                value = valueSupplier.get();
            }
            sDoubleValues.put(key, value);
            return value;
        }
    }

    public static void dumpToSharedPreferences(SharedPreferences.Editor editor) {
        synchronized (sBoolValues) {
            for (Map.Entry<String, Boolean> pair : sBoolValues.entrySet()) {
                editor.putBoolean(pair.getKey(), pair.getValue());
            }
        }
        synchronized (sIntValues) {
            for (Map.Entry<String, Integer> pair : sIntValues.entrySet()) {
                editor.putInt(pair.getKey(), pair.getValue());
            }
        }
        synchronized (sDoubleValues) {
            for (Map.Entry<String, Double> pair : sDoubleValues.entrySet()) {
                long ieee754LongValue = Double.doubleToRawLongBits(pair.getValue());
                editor.putLong(pair.getKey(), ieee754LongValue);
            }
        }
        synchronized (sStringValues) {
            for (Map.Entry<String, String> pair : sStringValues.entrySet()) {
                editor.putString(pair.getKey(), pair.getValue());
            }
        }
    }
}
