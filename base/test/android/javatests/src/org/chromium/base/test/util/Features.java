// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.util.ArraySet;

import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureParam;
import org.chromium.base.Flag;
import org.chromium.base.cached_flags.ValuesReturned;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Provides annotations for enabling / disabling features during tests.
 *
 * <p>Sample code:
 *
 * <pre>
 * @EnableFeatures(BaseFeatures.FOO)
 * public class Test {
 *
 *    @EnableFeatures(BaseFeatures.BAR)
 *    public void testBarEnabled() { ... }
 *
 *    @DisableFeatures(ContentFeatureList.BAZ)
 *    public void testBazDisabled() { ... }
 * }
 * </pre>
 */
public class Features {
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableFeatures {
        String[] value();
    }

    @Retention(RetentionPolicy.RUNTIME)
    public @interface DisableFeatures {
        String[] value();
    }

    private Features() {}

    static void resetCachedFlags() {
        // TODO(agrieve): Allow cached flags & field trials to be set in @BeforeClass.
        ValuesReturned.clearForTesting();
        Flag.resetAllInMemoryCachedValuesForTesting();
        FeatureParam.resetAllInMemoryCachedValuesForTesting();
    }

    public static void reset(Map<String, Boolean> flagStates) {
        // TODO(agrieve): Use ScopedFeatureList to update native feature states even after
        //     native feature list has been initialized.
        FlagsAndFieldTrials flagsAndFieldTrials = separateFlagsAndFieldTrials(flagStates);
        FeatureList.setTestFeaturesNoResetForTesting(flagsAndFieldTrials.mFeatureToValue);
        // Apply "--force-fieldtrials" and "--force-fieldtrial-params" passed by @CommandLineFlags.
        FieldTrials.applyFieldTrialsParams(
                CommandLine.getInstance(), flagsAndFieldTrials.mFieldTrialToFeatures);
    }

    /**
     * Converts {"A": true, "B<Trial1": true, "C": false}.
     *
     * @return [mFeatureToValue = {"A": true, "B": true, "C": false}, mFieldTrialToFeatures =
     *     {"Trial1": {"A"}}]
     */
    private static FlagsAndFieldTrials separateFlagsAndFieldTrials(
            Map<String, Boolean> featureAndFieldTrialToValue) {
        FlagsAndFieldTrials flagsAndFieldTrials = new FlagsAndFieldTrials();
        for (Map.Entry<String, Boolean> entry : featureAndFieldTrialToValue.entrySet()) {
            String rawFeatureName = entry.getKey();
            Boolean featureFlagValue = entry.getValue();
            if (rawFeatureName.contains("<")) {
                if (!featureFlagValue) {
                    throw new IllegalArgumentException(
                            String.format(
                                    "--disable-features=%s should not have a field trial",
                                    rawFeatureName));
                }
                String[] parts = rawFeatureName.split("<");
                if (parts.length > 2) {
                    throw new IllegalArgumentException(
                            String.format(
                                    "--enable-features=%s has multiple field trials",
                                    rawFeatureName));
                }
                String feature = parts[0];
                String fieldTrial = parts[1];
                flagsAndFieldTrials.mFeatureToValue.put(feature, featureFlagValue);
                flagsAndFieldTrials
                        .mFieldTrialToFeatures
                        .computeIfAbsent(fieldTrial, key -> new ArraySet<>())
                        .add(feature);
            } else {
                flagsAndFieldTrials.mFeatureToValue.put(rawFeatureName, featureFlagValue);
            }
        }
        return flagsAndFieldTrials;
    }

    private static class FlagsAndFieldTrials {
        private Map<String, Boolean> mFeatureToValue = new HashMap<>();
        private Map<String, Set<String>> mFieldTrialToFeatures = new HashMap<>();
    }
}
