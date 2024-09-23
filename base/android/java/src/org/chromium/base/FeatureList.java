// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.util.ArrayMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.library_loader.LibraryLoader;

import java.util.HashMap;
import java.util.Map;

/** Provides shared capabilities for feature flag support. */
@JNINamespace("base::android")
public class FeatureList {
    /** Test value overrides for tests without native. */
    public static class TestValues {
        private Map<String, Boolean> mFeatureFlags = new HashMap<>();
        private Map<String, Map<String, String>> mFieldTrialParams = new HashMap<>();

        /** Constructor. */
        public TestValues() {}

        /** Set overrides for feature flags. */
        public void setFeatureFlagsOverride(Map<String, Boolean> featureFlags) {
            mFeatureFlags = featureFlags;
        }

        /** Add an override for a feature flag. */
        public void addFeatureFlagOverride(String featureName, boolean testValue) {
            mFeatureFlags.put(featureName, testValue);
        }

        /** Add an override for a field trial parameter. */
        public void addFieldTrialParamOverride(
                String featureName, String paramName, String testValue) {
            Map<String, String> featureParams = mFieldTrialParams.get(featureName);
            if (featureParams == null) {
                featureParams = new ArrayMap<>();
                mFieldTrialParams.put(featureName, featureParams);
            }
            featureParams.put(paramName, testValue);
        }

        /**
         * Add an override for a field trial parameter.
         *
         * @param param The param object that holds feature and param names.
         * @param testValue The string value to override, which will later be parsed/converted into
         *     the actual value.
         */
        public void addFieldTrialParamOverride(FeatureParam param, String testValue) {
            addFieldTrialParamOverride(param.getFeatureName(), param.getName(), testValue);
        }

        Boolean getFeatureFlagOverride(String featureName) {
            return mFeatureFlags.get(featureName);
        }

        String getFieldTrialParamOverride(String featureName, String paramName) {
            Map<String, String> featureParams = mFieldTrialParams.get(featureName);
            if (featureParams == null) return null;
            return featureParams.get(paramName);
        }

        Map<String, String> getAllFieldTrialParamOverridesForFeature(String featureName) {
            return mFieldTrialParams.get(featureName);
        }
    }

    /** Map that stores substitution feature flags for tests. */
    private static @Nullable TestValues sTestFeatures;

    /** Prevent access to default values of the native feature flag. */
    private static boolean sDisableNativeForTesting;

    private FeatureList() {}

    /**
     * @deprecated Check specific flags (via Flag.isEnabled()) instead.
     */
    @Deprecated
    public static boolean isInitialized() {
        return (sDisableNativeForTesting && sTestFeatures != null) || isNativeInitialized();
    }

    /**
     * @return Whether the native FeatureList is initialized or not.
     */
    public static boolean isNativeInitialized() {
        if (!LibraryLoader.getInstance().isInitialized()) return false;
        // Even if the native library is loaded, the C++ FeatureList might not be initialized yet.
        // In that case, accessing it will not immediately fail, but instead cause a crash later
        // when it is initialized. Return whether the native FeatureList has been initialized,
        // so the return value can be tested, or asserted for a more actionable stack trace
        // on failure.
        //
        // The FeatureList is however guaranteed to be initialized by the time
        // AsyncInitializationActivity#finishNativeInitialization is called.
        return FeatureListJni.get().isInitialized();
    }

    /** Whether to block querying feature values from native. */
    public static void setDisableNativeForTesting(boolean value) {
        boolean prev = sDisableNativeForTesting;
        sDisableNativeForTesting = value;
        ResettersForTesting.register(() -> sDisableNativeForTesting = prev);
    }

    /** Whether to pretend native isn't loaded. */
    public static boolean getDisableNativeForTesting() {
        return sDisableNativeForTesting;
    }

    /** Sets the feature flags to use in JUnit tests, since native calls are not available there. */
    @VisibleForTesting
    public static void setTestFeatures(@Nullable Map<String, Boolean> testFeatures) {
        if (testFeatures == null) {
            setTestValues(null);
        } else {
            TestValues testValues = new TestValues();
            testValues.setFeatureFlagsOverride(testFeatures);
            setTestValues(testValues);
        }
    }

    /**
     * Sets the feature flags and field trial parameters to use in JUnit tests, since native calls
     * are not available there.
     */
    @VisibleForTesting
    public static void setTestValues(@Nullable TestValues testValues) {
        TestValues prevValues = sTestFeatures;
        sTestFeatures = testValues;
        ResettersForTesting.register(() -> sTestFeatures = prevValues);
    }

    /** For use by test runners. */
    public static void setTestFeaturesNoResetForTesting(Map<String, Boolean> testFeatures) {
        TestValues testValues = new TestValues();
        testValues.setFeatureFlagsOverride(testFeatures);
        sTestFeatures = testValues;
    }

    /**
     * Adds overrides to feature flags and field trial parameters in addition to existing ones.
     *
     * @param testValuesToMerge the TestValues to merge into existing ones
     * @param replace if true, replaces existing values (e.g. from @EnableFeatures annotations)
     */
    public static void mergeTestValues(@NonNull TestValues testValuesToMerge, boolean replace) {
        TestValues newTestValues;
        if (sTestFeatures == null) {
            newTestValues = new TestValues();
        } else {
            newTestValues = sTestFeatures;
        }

        if (replace) {
            newTestValues.mFeatureFlags.putAll(testValuesToMerge.mFeatureFlags);
        } else {
            for (Map.Entry<String, Boolean> toMerge : testValuesToMerge.mFeatureFlags.entrySet()) {
                newTestValues.mFeatureFlags.putIfAbsent(toMerge.getKey(), toMerge.getValue());
            }
        }

        for (Map.Entry<String, Map<String, String>> e :
                testValuesToMerge.mFieldTrialParams.entrySet()) {
            String featureName = e.getKey();
            var fieldTrialParamsForFeature = newTestValues.mFieldTrialParams.get(featureName);
            if (fieldTrialParamsForFeature == null) {
                fieldTrialParamsForFeature = new ArrayMap<>();
                newTestValues.mFieldTrialParams.put(featureName, fieldTrialParamsForFeature);
            }

            if (replace) {
                fieldTrialParamsForFeature.putAll(e.getValue());
            } else {
                for (Map.Entry<String, String> toMerge : e.getValue().entrySet()) {
                    fieldTrialParamsForFeature.putIfAbsent(toMerge.getKey(), toMerge.getValue());
                }
            }
        }

        setTestValues(newTestValues);
    }

    /**
     * @param featureName The name of the feature to query.
     * @return Whether the feature has a test value configured.
     */
    public static boolean hasTestFeature(String featureName) {
        // TODO(crbug.com/40264751)): Copy into a local reference to avoid race conditions
        // like crbug.com/1494095 unsetting the test features. Locking down flag state will allow
        // this mitigation to be removed.
        TestValues testValues = sTestFeatures;
        return testValues != null && testValues.mFeatureFlags.containsKey(featureName);
    }

    /**
     * @param featureName The name of the feature the param is part of.
     * @param paramName The name of the param to query.
     * @return Whether the param has a test value configured.
     */
    public static boolean hasTestParam(String featureName, String paramName) {
        TestValues testValues = sTestFeatures;
        return testValues != null
                && testValues.mFieldTrialParams.containsKey(featureName)
                && testValues.mFieldTrialParams.get(featureName).containsKey(paramName);
    }

    /**
     * Returns the test value of the feature with the given name.
     *
     * @param featureName The name of the feature to query.
     * @return The test value set for the feature, or null if no test value has been set.
     * @throws IllegalArgumentException if no test value was set and default values aren't allowed.
     */
    public static Boolean getTestValueForFeature(String featureName) {
        // TODO(crbug.com/40264751)): Copy into a local reference to avoid race conditions
        // like crbug.com/1494095 unsetting the test features. Locking down flag state will allow
        // this mitigation to be removed.
        TestValues testValues = sTestFeatures;
        if (testValues != null) {
            Boolean override = testValues.getFeatureFlagOverride(featureName);
            if (override != null) {
                return override;
            }
        }
        if (sDisableNativeForTesting) {
            throw new IllegalArgumentException(
                    "No test value configured for "
                            + featureName
                            + " and native is not available to provide a default value. Use"
                            + " @EnableFeatures or @DisableFeatures to provide test values for"
                            + " the flag.");
        }
        return null;
    }

    /**
     * Returns the test value of the field trial parameter.
     *
     * @param featureName The name of the feature to query.
     * @param paramName The name of the field trial parameter to query.
     * @return The test value set for the parameter, or null if no test value has been set.
     */
    public static String getTestValueForFieldTrialParam(String featureName, String paramName) {
        // TODO(crbug.com/40264751)): Copy into a local reference to avoid race conditions
        // like crbug.com/1494095 unsetting the test features. Locking down flag state will allow
        // this mitigation to be removed.
        TestValues testValues = sTestFeatures;
        if (testValues != null) {
            return testValues.getFieldTrialParamOverride(featureName, paramName);
        }
        return null;
    }

    /**
     * Returns the test value of the all field trial parameters of a given feature.
     *
     * @param featureName The name of the feature to query all parameters.
     * @return The test values set for the parameter, or null if no test values have been set (if
     *     test values were set for other features, an empty Map will be returned, not null).
     */
    public static Map<String, String> getTestValuesForAllFieldTrialParamsForFeature(
            String featureName) {
        // TODO(crbug.com/40264751)): Copy into a local reference to avoid race conditions
        // like crbug.com/1494095 unsetting the test features. Locking down flag state will allow
        // this mitigation to be removed.
        TestValues testValues = sTestFeatures;
        if (testValues != null) {
            return testValues.getAllFieldTrialParamOverridesForFeature(featureName);
        }
        return null;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        boolean isInitialized();
    }
}
