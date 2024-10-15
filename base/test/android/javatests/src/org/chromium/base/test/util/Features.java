// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureParam;
import org.chromium.base.Flag;
import org.chromium.base.cached_flags.ValuesOverridden;
import org.chromium.base.cached_flags.ValuesReturned;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

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
        ValuesOverridden.removeOverrides();
        FieldTrials.getInstance().reset();
        Flag.resetAllInMemoryCachedValuesForTesting();
        FeatureParam.resetAllInMemoryCachedValuesForTesting();
    }

    public static void reset(Map<String, Boolean> flagStates) {
        // TODO(agrieve): Use ScopedFeatureList to update native feature states even after
        //     native feature list has been initialized.
        Map<String, Boolean> cleanFlagStates = cleanUpFlagStates(flagStates);
        FeatureList.setTestFeaturesNoResetForTesting(cleanFlagStates);
        // Apply "--force-fieldtrials" passed by @CommandLineFlags.
        FieldTrials.getInstance().applyFieldTrials(CommandLine.getInstance());
    }

    /**
     * Removes field trials from the keys of |flagsStates|.
     *
     * <p>E.g.: {"FeatureA<Trial1": true} becomes {"FeatureA": true}.
     */
    private static Map<String, Boolean> cleanUpFlagStates(Map<String, Boolean> flagStates) {
        Map<String, Boolean> cleanFlagStates = new HashMap<>();
        for (Map.Entry<String, Boolean> entry : flagStates.entrySet()) {
            String rawFeatureName = entry.getKey();
            Boolean featureFlagValue = entry.getValue();
            if (rawFeatureName.contains("<")) {
                assert featureFlagValue
                        : String.format(
                                "--disable-features=%s should not have a field trial",
                                rawFeatureName);
                cleanFlagStates.put(rawFeatureName.split("<")[0], featureFlagValue);
            } else {
                cleanFlagStates.put(rawFeatureName, featureFlagValue);
            }
        }
        return cleanFlagStates;
    }
}
