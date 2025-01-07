// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.CheckResult;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
    public static class Builder extends FeatureList.TestValues {
        private @Nullable String mLastFeatureName;

        private Builder() {}

        /**
         * Apply overrides to feature flags and field trial parameters in addition to existing ones.
         *
         * <p>On conflict, overwrites the previous override.
         */
        public void apply() {
            FeatureList.mergeTestValues(this, /* replace= */ true);
        }

        /**
         * Apply overrides to feature flags and field trial parameters in addition to existing ones.
         *
         * <p>On conflict, the previous override is preserved.
         */
        public void applyWithoutOverwrite() {
            FeatureList.mergeTestValues(this, /* replace= */ false);
        }

        /** Enable a feature flag. */
        @CheckResult
        public Builder enable(String featureName) {
            addFeatureFlagOverride(featureName, true);
            mLastFeatureName = featureName;
            return this;
        }

        /** Disable a feature flag. */
        @CheckResult
        public Builder disable(String featureName) {
            addFeatureFlagOverride(featureName, false);
            mLastFeatureName = null;
            return this;
        }

        /** Enable or disable a feature flag. */
        @CheckResult
        public Builder flag(String featureName, boolean value) {
            addFeatureFlagOverride(featureName, value);
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
            addFieldTrialParamOverride(featureName, paramName, value);
            return this;
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
        FeatureList.removeAllTestOverrides();
    }
}
