// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.BaseFlagTestRule.A_OFF_B_ON;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_A;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_B;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;
import org.chromium.build.annotations.Nullable;

import java.util.Map;

/** Unit Tests for {@link PostNativeFlag}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PostNativeFlagUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test(expected = AssertionError.class)
    public void testDuplicateFeature_throwsException() {
        new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
        new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNoFeatureOverrides_throwIllegalArgumentException() {
        PostNativeFlag featureA = new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
        featureA.isEnabled();
    }

    @Test
    public void testHasFeatureOverrides_getsFromChromeFeatureList() {
        PostNativeFlag featureA = new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
        PostNativeFlag featureB = new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_B);

        // The following feature overrides should be used from now on.
        A_OFF_B_ON.apply();

        // Assert that PostNativeFlags return the values from feature overrides.
        assertFalse(featureA.isEnabled());
        assertTrue(featureB.isEnabled());
    }

    @Test
    public void testHasDefaultValues_getsFromChromeFeatureList() {
        // Create a feature map that returns the following default values for flags.
        FeatureMap testFeatureMap =
                new FeatureMap() {
                    @Override
                    public @Nullable Map<String, Boolean> getFlagsDefaultValuesInTests() {
                        return Map.ofEntries(
                                Map.entry(FEATURE_A, true), Map.entry(FEATURE_B, false));
                    }

                    @Override
                    protected long getNativeMap() {
                        throw new UnsupportedOperationException(
                                "FeatureMap stub for testing does not support getting the flag"
                                        + " value across the native boundary, provide test override"
                                        + " values instead.");
                    }
                };

        PostNativeFlag featureA = new PostNativeFlag(testFeatureMap, FEATURE_A);
        PostNativeFlag featureB = new PostNativeFlag(testFeatureMap, FEATURE_B);

        // Assert that PostNativeFlags return the default values hardcoded in the feature map.
        assertTrue(featureA.isEnabled());
        assertFalse(featureB.isEnabled());
    }
}
