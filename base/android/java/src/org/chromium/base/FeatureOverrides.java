// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;

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
        @Nullable private String mLastFeatureName;

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
        public Builder enable(String featureName) {
            addFeatureFlagOverride(featureName, true);
            mLastFeatureName = featureName;
            return this;
        }

        /** Disable a feature flag. */
        public Builder disable(String featureName) {
            addFeatureFlagOverride(featureName, false);
            mLastFeatureName = null;
            return this;
        }

        /** Override a boolean param for the last feature flag enabled. */
        public Builder param(String paramName, boolean value) {
            return param(getLastFeatureName(), paramName, String.valueOf(value));
        }

        /** Override an int param for the last feature flag enabled. */
        public Builder param(String paramName, int value) {
            return param(getLastFeatureName(), paramName, String.valueOf(value));
        }

        /** Override a double param for the last feature flag enabled. */
        public Builder param(String paramName, double value) {
            return param(getLastFeatureName(), paramName, String.valueOf(value));
        }

        /** Override a String param for the last feature flag enabled. */
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
        public Builder param(String featureName, String paramName, boolean value) {
            return param(featureName, paramName, String.valueOf(value));
        }

        /** Override an int param. */
        public Builder param(String featureName, String paramName, int value) {
            return param(featureName, paramName, String.valueOf(value));
        }

        /** Override a double param. */
        public Builder param(String featureName, String paramName, double value) {
            return param(featureName, paramName, String.valueOf(value));
        }

        /** Override a String param. */
        public Builder param(String featureName, String paramName, String value) {
            addFieldTrialParamOverride(featureName, paramName, value);
            return this;
        }
    }

    /** Use {@link #newBuilder()}. */
    private FeatureOverrides() {}

    /** Create a Builder for overriding feature flags and field trial parameters. */
    public static FeatureOverrides.Builder newBuilder() {
        return new FeatureOverrides.Builder();
    }

    /** Enable a feature flag for testing. */
    public static void enable(String featureName) {
        FeatureList.setTestFeature(featureName, true);
    }

    /** Disable a feature flag for testing. */
    public static void disable(String featureName) {
        FeatureList.setTestFeature(featureName, false);
    }

    /** Override a feature flag for testing. */
    public static void overrideFlag(String featureName, boolean testValue) {
        FeatureList.setTestFeature(featureName, testValue);
    }

    /** Override a feature param for testing. */
    public static void overrideParam(String featureName, String paramName, String testValue) {
        FeatureList.setTestFeatureParam(featureName, paramName, testValue);
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
