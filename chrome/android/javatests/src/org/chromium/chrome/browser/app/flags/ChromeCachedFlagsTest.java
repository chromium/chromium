// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import static org.junit.Assert.fail;

import androidx.test.filters.MediumTest;

import org.junit.Assume;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.cached_flags.CachedFlag;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Checks for app-level issues with flags. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests state of flags at specific app startup points")
public class ChromeCachedFlagsTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    // Baseline so that the test can be enabled and catch new violations.
    //
    // These flags are vulnerable to crbug.com/380111667: CachedFlags have different values in
    // first test of batch vs rest of batch.
    //
    // To fix, change the defaultValueInTests argument in the CachedFlag constructor to be the
    // same as the value set in fieldtrial_testing_config.json, then remove it from here. You
    // may need to update tests that break due to them using a different flag value after this
    // change.
    //
    // DO NOT ADD FLAGS TO THIS LIST.
    private static final Set<CachedFlag> BASELINE = Set.of();

    /**
     * Tests that the |defaultValueForTests| in the CachedFlag declaration matches
     * fieldtrial_testing_config.json.
     *
     * <p>Also breaks when the baseline contains unnecessary exceptions.
     *
     * <p>TODO(crbug.com/445490091): Write a test to ensure the |defaultValue| in the CachedFlag
     * declaration matches the default value in native.
     */
    @Test
    @MediumTest
    public void testValueIsConsistentWithDefault() {
        // In Chrome-branded builds, the fieldtrial_testing_config.json isn't applied, so
        // flag values may differ.
        Assume.assumeTrue(!BuildConfig.IS_CHROME_BRANDED);

        // If the switch --disable-field-trial-config is set, the fieldtrial_testing_config.json
        // isn't applied either.
        Assume.assumeTrue(!CommandLine.getInstance().hasSwitch("disable-field-trial-config"));

        mCtaTestRule.startOnBlankPage();

        List<List<CachedFlag>> allListsOfCachedFlags =
                new ArrayList<>(ChromeCachedFlags.LISTS_OF_CACHED_FLAGS_FULL_BROWSER);
        allListsOfCachedFlags.addAll(ChromeCachedFlags.LISTS_OF_CACHED_FLAGS_MINIMAL_BROWSER);

        List<String> notMatching = new ArrayList<>();
        List<String> notNeededInBaseline = new ArrayList<>();
        for (List<CachedFlag> listOfCachedFlags : allListsOfCachedFlags) {
            for (CachedFlag cachedFlag : listOfCachedFlags) {
                // Get the fieldtrial_testing_config.json value by calling the FeatureMap bypassing
                // ValuesReturned, SharedPreferences, going straight to native. This value might
                // not match CachedFlag.isEnabled() if, for example, the CachedFlag has been
                // checked before native.
                boolean jsonValue =
                        cachedFlag
                                .getFeatureMapForTesting()
                                .isEnabledInNative(cachedFlag.getFeatureName());
                boolean matches = cachedFlag.getDefaultValue() == jsonValue;
                if (BASELINE.contains(cachedFlag)) {
                    if (matches) {
                        notNeededInBaseline.add(cachedFlag.getFeatureName());
                    }
                } else {
                    if (!matches) {
                        notMatching.add(cachedFlag.getFeatureName());
                    }
                }
            }
        }

        if (!notMatching.isEmpty()) {
            fail(
                    "|defaultValueInTests| and fieldtrial_testing_config.json values do not match"
                            + " for flags: "
                            + String.join(", ", notMatching));
        }
        if (!notNeededInBaseline.isEmpty()) {
            fail("Flags not needed in baseline: " + String.join(", ", notNeededInBaseline));
        }
    }
}
