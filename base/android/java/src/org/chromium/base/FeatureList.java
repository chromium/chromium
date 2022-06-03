// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;

import java.util.HashMap;
import java.util.Map;

/**
 * Provides shared capabilities for feature flag support.
 */
@JNINamespace("base::android")
@MainDex
public class FeatureList {
    /**
     * Test value overrides for tests without native.
     */
    public static class TestValues {
        private Map<String, Boolean> mFeatureFlags = new HashMap<>();
        private Map<String, String> mFieldTrialParams = new HashMap<>();

        /**
         * Constructor.
         */
        public TestValues() {}

        /**
         * Set overrides for feature flags.
         */
        public void setFeatureFlagsOverride(Map<String, Boolean> featureFlags) {
            mFeatureFlags = featureFlags;
        }

        /**
         * Add an override for a field trial parameter.
         */
        public void addFeatureFlagOverride(String featureName, boolean testValue) {
            mFeatureFlags.put(featureName, testValue);
        }

        /**
         * Add an override for a field trial parameter.
         */
        public void addFieldTrialParamOverride(
                String featureName, String paramName, String testValue) {
            mFieldTrialParams.put(makeKey(featureName, paramName), testValue);
        }

        Boolean getFeatureFlagOverride(String featureName) {
            return mFeatureFlags.get(featureName);
        }

        String getFieldTrialParamOverride(String featureName, String paramName) {
            return mFieldTrialParams.get(makeKey(featureName, paramName));
        }

        private static String makeKey(String featureName, String paramName) {
            return featureName + ":" + paramName;
        }
    }

    /** Map that stores substitution feature flags for tests. */
    private static @Nullable TestValues sTestFeatures;

    /** Access to default values of the native feature flag. */
    private static boolean sTestCanUseDefaults;

    private FeatureList() {}

    /**
     * @return Whether the native FeatureList has been initialized. If this method returns false,
     *         none of the methods in this class that require native access should be called (except
     *         in tests if test features have been set).
     */
    public static boolean isInitialized() {
        return hasTestFeatures() || isNativeInitialized();
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

    /**
     * This is called explicitly for instrumentation tests via Features#applyForInstrumentation().
     * Unit tests and Robolectric tests must not invoke this and should rely on the {@link Features}
     * annotations to enable or disable any feature flags.
     */
    @VisibleForTesting
    public static void setTestCanUseDefaultsForTesting() {
        sTestCanUseDefaults = true;
    }

    /**
     * We reset the value to false after the instrumentation test to avoid any unwanted
     * persistence of the state. This is invoked by Features#reset().
     */
    @VisibleForTesting
    public static void resetTestCanUseDefaultsForTesting() {
        sTestCanUseDefaults = false;
    }

    /**
     * Sets the feature flags to use in JUnit tests, since native calls are not available there.
     */
    @VisibleForTesting
    public static void setTestFeatures(Map<String, Boolean> testFeatures) {
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
    public static void setTestValues(TestValues testFeatures) {
        sTestFeatures = testFeatures;
    }

    /**
     * @return Whether test feature values have been configured.
     */
    public static boolean hasTestFeatures() {
        return sTestFeatures != null;
    }

    /**
     * Returns the test value of the feature with the given name.
     *
     * @param featureName The name of the feature to query.
     * @return The test value set for the feature, or null if no test value has been set.
     * @throws IllegalArgumentException if no test value was set and default values aren't allowed.
     */
    public static Boolean getTestValueForFeature(String featureName) {
        if (hasTestFeatures()) {
            Boolean override = sTestFeatures.getFeatureFlagOverride(featureName);
            if (override != null) {
                return override;
            }
            if (!sTestCanUseDefaults) {
                throw new IllegalArgumentException("No test value configured for " + featureName);
            }
        }
        return null;
    }

    /**
     * Returns the test value of the field trial parameter.
     *
     * @param featureName The name of the feature to query.
     * @param paramName The name of the field trial parameter to query.
     * @return The test value set for the parameter, or null if no test value has been set.
     * @throws IllegalArgumentException if no test value was set and default values aren't allowed.
     */
    public static String getTestValueForFieldTrialParam(String featureName, String paramName) {
        if (hasTestFeatures()) {
            String override = sTestFeatures.getFieldTrialParamOverride(featureName, paramName);
            if (override != null) {
                return override;
            }
        }
        return null;
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized();
    }
}
