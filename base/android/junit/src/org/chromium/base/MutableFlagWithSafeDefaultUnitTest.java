// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.BaseFlagTestRule.A_ON_B_OFF;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_A;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_B;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_MAP;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;

/** Unit Tests for {@link MutableFlagWithSafeDefault}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MutableFlagWithSafeDefaultUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test(expected = AssertionError.class)
    public void testDuplicateFeatureInMap_throwsException() {
        FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_A, false);
        FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_A, false);
    }

    @Test(expected = AssertionError.class)
    public void testDuplicateFeatureOutsideOfMap_throwsException() {
        FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_A, false);
        new MutableFlagWithSafeDefault(FEATURE_MAP, FEATURE_A, false);
    }

    @Test
    public void testNativeInitialized_getsFromFeatureMap() {
        MutableFlagWithSafeDefault featureA =
                FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_A, false);
        MutableFlagWithSafeDefault featureB =
                FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_B, true);

        // Values from FeatureMap should be used from now on.
        A_ON_B_OFF.apply();

        // Verify that {@link MutableFlagWithSafeDefault} returns values from FeatureMap.
        assertTrue(featureA.isEnabled());
        assertFalse(featureB.isEnabled());
    }

    @Test
    public void testNativeNotInitialized_useDefault() {
        MutableFlagWithSafeDefault featureA =
                FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_A, false);
        MutableFlagWithSafeDefault featureB =
                FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_B, true);

        // Query the flags to make sure the default values are returned.
        assertFalse(featureA.isEnabled());
        assertTrue(featureB.isEnabled());
    }

    @Test
    public void testNativeInitializedUsedDefault_getsFromFeatureMap() {
        MutableFlagWithSafeDefault featureA =
                FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_A, false);
        MutableFlagWithSafeDefault featureB =
                FEATURE_MAP.mutableFlagWithSafeDefault(FEATURE_B, true);

        // Query the flags to make sure the default values are returned.
        assertFalse(featureA.isEnabled());
        assertTrue(featureB.isEnabled());

        // Values from FeatureMap should be used from now on.
        A_ON_B_OFF.apply();

        // Verify that {@link MutableFlagWithSafeDefault} returns values from FeatureMap.
        assertTrue(featureA.isEnabled());
        assertFalse(featureB.isEnabled());
    }
}
