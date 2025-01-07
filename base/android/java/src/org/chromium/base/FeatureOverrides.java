// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.util.ArrayMap;

import androidx.annotation.CheckResult;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.HashMap;
import java.util.Map;

/**
 * Allows overriding feature flags and parameters for tests. Prefer @Features annotations.
 *
 * <p>This is useful when the values set are dynamic, for example in parameterized tests or in tests
 * that needs to change the flag/param values in the middle of the test rather than from the
 * beginning.
 *
 * <p>The @Features.EnableFeatures and @Features.DisableFeatures annotations use FeatureOverrides,
 * but also apply the changes more broadly; they include applying the override in native and they
 * batch according to flag configuration.
 */
@NullMarked
public class FeatureOverrides {
    /** Map that stores substitution feature flags for tests. */
    static @Nullable TestValues sTestFeatures;

    /** Builder of overrides for overriding feature flags and field trial parameters. */
    public static class Builder {
        private final TestValues mTestValues = new TestValues();
        private @Nullable String mLastFeatureName;

        private Builder() {}

        /**
         * Apply overrides to feature flags and field trial parameters in addition to existing ones.
         *
         * <p>On conflict, overwrites the previous override.
         */
        public void apply() {
            mergeTestValues(mTestValues, /* replace= */ true);
        }

        /**
         * Apply overrides to feature flags and field trial parameters in addition to existing ones.
         *
         * <p>On conflict, the previous override is preserved.
         */
        public void applyWithoutOverwrite() {
            mergeTestValues(mTestValues, /* replace= */ false);
        }

        /** For use by test runners. */
        public void applyNoResetForTesting() {
            setTestValuesNoResetForTesting(mTestValues);
        }

        /** Enable a feature flag. */
        @CheckResult
        public Builder enable(String featureName) {
            mTestValues.addFeatureFlagOverride(featureName, true);
            mLastFeatureName = featureName;
            return this;
        }

        /** Disable a feature flag. */
        @CheckResult
        public Builder disable(String featureName) {
            mTestValues.addFeatureFlagOverride(featureName, false);
            mLastFeatureName = null;
            return this;
        }

        /** Enable or disable a feature flag. */
        @CheckResult
        public Builder flag(String featureName, boolean value) {
            mTestValues.addFeatureFlagOverride(featureName, value);
            return this;
        }

        /** Override a boolean param for the last feature flag enabled. */
        @CheckResult
        public Builder param(String paramName, boolean value) {
            return param(getLastFeatureName(), paramName, String.valueOf(value));
        }

        /** Override an int param for the last feature flag enabled. */
        @CheckResult
        public Builder param(String paramName, int value) {
            return param(getLastFeatureName(), paramName, String.valueOf(value));
        }

        /** Override a double param for the last feature flag enabled. */
        @CheckResult
        public Builder param(String paramName, double value) {
            return param(getLastFeatureName(), paramName, String.valueOf(value));
        }

        /** Override a String param for the last feature flag enabled. */
        @CheckResult
        public Builder param(String paramName, String value) {
            return param(getLastFeatureName(), paramName, value);
        }

        private String getLastFeatureName() {
            if (mLastFeatureName == null) {
                throw new IllegalArgumentException(
                        "param(paramName, value) should be used after enable()");
            }
            return mLastFeatureName;
        }

        /** Override a boolean param. */
        @CheckResult
        public Builder param(String featureName, String paramName, boolean value) {
            return param(featureName, paramName, String.valueOf(value));
        }

        /** Override an int param. */
        @CheckResult
        public Builder param(String featureName, String paramName, int value) {
            return param(featureName, paramName, String.valueOf(value));
        }

        /** Override a double param. */
        @CheckResult
        public Builder param(String featureName, String paramName, double value) {
            return param(featureName, paramName, String.valueOf(value));
        }

        /** Override a String param. */
        @CheckResult
        public Builder param(String featureName, String paramName, String value) {
            mTestValues.addFieldTrialParamOverride(featureName, paramName, value);
            return this;
        }

        public boolean isEmpty() {
            return mTestValues.isEmpty();
        }
    }

    /** Maps with the actual test value overrides. */
    private static class TestValues {
        private final Map<String, Boolean> mFeatureFlags = new HashMap<>();
        private final Map<String, Map<String, String>> mFieldTrialParams = new HashMap<>();

        /** Add an override for a feature flag. */
        void addFeatureFlagOverride(String featureName, boolean testValue) {
            mFeatureFlags.put(featureName, testValue);
        }

        /** Add an override for a field trial parameter. */
        void addFieldTrialParamOverride(String featureName, String paramName, String testValue) {
            Map<String, String> featureParams = mFieldTrialParams.get(featureName);
            if (featureParams == null) {
                featureParams = new ArrayMap<>();
                mFieldTrialParams.put(featureName, featureParams);
            }
            featureParams.put(paramName, testValue);
        }

        public @Nullable Boolean getFeatureFlagOverride(String featureName) {
            return mFeatureFlags.get(featureName);
        }

        public @Nullable String getFieldTrialParamOverride(String featureName, String paramName) {
            Map<String, String> featureParams = mFieldTrialParams.get(featureName);
            if (featureParams == null) return null;
            return featureParams.get(paramName);
        }

        public @Nullable Map<String, String> getAllFieldTrialParamOverridesForFeature(
                String featureName) {
            return mFieldTrialParams.get(featureName);
        }

        void merge(TestValues testValuesToMerge, boolean replace) {
            if (replace) {
                mFeatureFlags.putAll(testValuesToMerge.mFeatureFlags);
            } else {
                for (Map.Entry<String, Boolean> toMerge :
                        testValuesToMerge.mFeatureFlags.entrySet()) {
                    mFeatureFlags.putIfAbsent(toMerge.getKey(), toMerge.getValue());
                }
            }

            for (Map.Entry<String, Map<String, String>> e :
                    testValuesToMerge.mFieldTrialParams.entrySet()) {
                String featureName = e.getKey();
                var fieldTrialParamsForFeature = mFieldTrialParams.get(featureName);
                if (fieldTrialParamsForFeature == null) {
                    fieldTrialParamsForFeature = new ArrayMap<>();
                    mFieldTrialParams.put(featureName, fieldTrialParamsForFeature);
                }

                if (replace) {
                    fieldTrialParamsForFeature.putAll(e.getValue());
                } else {
                    for (Map.Entry<String, String> toMerge : e.getValue().entrySet()) {
                        fieldTrialParamsForFeature.putIfAbsent(
                                toMerge.getKey(), toMerge.getValue());
                    }
                }
            }
        }

        /**
         * Returns a representation of the TestValues.
         *
         * <p>The format returned is:
         *
         * <pre>{FeatureA=true} + {FeatureA.Param1=Value1, FeatureA.ParamB=ValueB}</pre>
         */
        @SuppressWarnings("UnusedMethod")
        public String toDebugString() {
            StringBuilder stringBuilder = new StringBuilder();
            String separator = "";
            stringBuilder.append("{");
            for (var e : mFeatureFlags.entrySet()) {
                String featureName = e.getKey();
                boolean featureValue = e.getValue();
                stringBuilder
                        .append(separator)
                        .append(featureName)
                        .append("=")
                        .append(featureValue);
                separator = ", ";
            }
            stringBuilder.append("}");
            if (!mFieldTrialParams.isEmpty()) {
                stringBuilder.append(" + {");
                for (var e : mFieldTrialParams.entrySet()) {
                    String paramsAndValuesSeparator = "";
                    String featureName = e.getKey();
                    Map<String, String> paramsAndValues = e.getValue();
                    for (var paramAndValue : paramsAndValues.entrySet()) {
                        String paramName = paramAndValue.getKey();
                        String paramValue = paramAndValue.getValue();
                        stringBuilder
                                .append(paramsAndValuesSeparator)
                                .append(featureName)
                                .append(".")
                                .append(paramName)
                                .append("=")
                                .append(paramValue);
                        paramsAndValuesSeparator = ", ";
                    }
                }
                stringBuilder.append("}");
            }
            return stringBuilder.toString();
        }

        public boolean isEmpty() {
            return mFeatureFlags.isEmpty() && mFieldTrialParams.isEmpty();
        }

        boolean hasFlagOverride(String featureName) {
            return mFeatureFlags.containsKey(featureName);
        }

        boolean hasParamOverride(String featureName, String paramName) {
            return mFieldTrialParams.containsKey(featureName)
                    && mFieldTrialParams.get(featureName).containsKey(paramName);
        }
    }

    /** Use {@link #newBuilder()}. */
    private FeatureOverrides() {}

    /** Create a Builder for overriding feature flags and field trial parameters. */
    @CheckResult
    public static FeatureOverrides.Builder newBuilder() {
        return new FeatureOverrides.Builder();
    }

    /** Enable a feature flag for testing. */
    public static void enable(String featureName) {
        newBuilder().enable(featureName).apply();
    }

    /** Disable a feature flag for testing. */
    public static void disable(String featureName) {
        newBuilder().disable(featureName).apply();
    }

    /** Override a feature flag for testing. */
    public static void overrideFlag(String featureName, boolean testValue) {
        newBuilder().flag(featureName, testValue).apply();
    }

    /** Override a boolean feature param for testing. */
    public static void overrideParam(String featureName, String paramName, boolean testValue) {
        newBuilder().param(featureName, paramName, testValue).apply();
    }

    /** Override an int feature param for testing. */
    public static void overrideParam(String featureName, String paramName, int testValue) {
        newBuilder().param(featureName, paramName, testValue).apply();
    }

    /** Override a double feature param for testing. */
    public static void overrideParam(String featureName, String paramName, double testValue) {
        newBuilder().param(featureName, paramName, testValue).apply();
    }

    /** Override a feature param for testing. */
    public static void overrideParam(String featureName, String paramName, String testValue) {
        newBuilder().param(featureName, paramName, testValue).apply();
    }

    /**
     * Rarely necessary. Remove all Java overrides to feature flags and field trial parameters.
     *
     * <p>You don't need to call this on tearDown() or at the end of a test. ResettersForTesting
     * already resets test values.
     *
     * <p>@Features annotations and @CommandLineFlags --enable/disable-features are affected by
     * this.
     */
    public static void removeAllIncludingAnnotations() {
        overwriteTestValues(null);
    }

    private static void overwriteTestValues(@Nullable TestValues testValues) {
        TestValues prevValues = sTestFeatures;
        sTestFeatures = testValues;
        ResettersForTesting.register(() -> sTestFeatures = prevValues);
    }

    private static void setTestValuesNoResetForTesting(TestValues testValues) {
        sTestFeatures = testValues;
    }

    /**
     * Adds overrides to feature flags and field trial parameters in addition to existing ones.
     *
     * @param testValuesToMerge the TestValues to merge into existing ones
     * @param replace if true, replaces existing overrides; otherwise preserve them
     */
    private static void mergeTestValues(TestValues testValuesToMerge, boolean replace) {
        TestValues newTestValues = new TestValues();
        if (sTestFeatures != null) {
            newTestValues.merge(sTestFeatures, /* replace= */ true);
        }
        newTestValues.merge(testValuesToMerge, replace);
        overwriteTestValues(newTestValues);
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
        return testValues != null && testValues.hasFlagOverride(featureName);
    }

    /**
     * @param featureName The name of the feature the param is part of.
     * @param paramName The name of the param to query.
     * @return Whether the param has a test value configured.
     */
    public static boolean hasTestParam(String featureName, String paramName) {
        TestValues testValues = sTestFeatures;
        return testValues != null && testValues.hasParamOverride(featureName, paramName);
    }

    /**
     * Returns the test value of the feature with the given name.
     *
     * @param featureName The name of the feature to query.
     * @return The test value set for the feature, or null if no test value has been set.
     * @throws IllegalArgumentException if no test value was set and default values aren't allowed.
     */
    public static @Nullable Boolean getTestValueForFeatureStrict(String featureName) {
        Boolean testValue = getTestValueForFeature(featureName);
        if (testValue == null && FeatureList.getDisableNativeForTesting()) {
            throw new IllegalArgumentException(
                    "No test value configured for "
                            + featureName
                            + " and native is not available to provide a default value. Use"
                            + " @EnableFeatures or @DisableFeatures to provide test values for"
                            + " the flag.");
        }
        return testValue;
    }

    /**
     * Returns the test value of the feature with the given name.
     *
     * @param featureName The name of the feature to query.
     * @return The test value set for the feature, or null if no test value has been set.
     */
    public static @Nullable Boolean getTestValueForFeature(String featureName) {
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
        return null;
    }

    /**
     * Returns the test value of the field trial parameter.
     *
     * @param featureName The name of the feature to query.
     * @param paramName The name of the field trial parameter to query.
     * @return The test value set for the parameter, or null if no test value has been set.
     */
    public static @Nullable String getTestValueForFieldTrialParam(
            String featureName, String paramName) {
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
    public static @Nullable Map<String, String> getTestValuesForAllFieldTrialParamsForFeature(
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
}
