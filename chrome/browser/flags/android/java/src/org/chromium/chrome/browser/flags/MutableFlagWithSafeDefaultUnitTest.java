// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.chromium.chrome.browser.flags.BaseFlagTestRule.A_OFF_B_ON;
import static org.chromium.chrome.browser.flags.BaseFlagTestRule.A_ON_B_OFF;
import static org.chromium.chrome.browser.flags.BaseFlagTestRule.FEATURE_A;
import static org.chromium.chrome.browser.flags.BaseFlagTestRule.FEATURE_B;
import static org.chromium.chrome.browser.flags.BaseFlagTestRule.assertIsEnabledMatches;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit Tests for {@link MutableFlagWithSafeDefault}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MutableFlagWithSafeDefaultUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test(expected = AssertionError.class)
    public void testDuplicateFeature_throwsException() {
        new MutableFlagWithSafeDefault(FEATURE_A, true);
        new MutableFlagWithSafeDefault(FEATURE_A, true);
    }

    @Test
    public void testNativeInitialized_getsFromChromeFeatureList() {
        MutableFlagWithSafeDefault featureA = new MutableFlagWithSafeDefault(FEATURE_A, false);
        MutableFlagWithSafeDefault featureB = new MutableFlagWithSafeDefault(FEATURE_B, true);

        // Values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_ON_B_OFF);

        // Verify that {@link MutableFlagWithSafeDefault} returns native values.
        assertIsEnabledMatches(A_ON_B_OFF, featureA, featureB);
    }

    @Test
    public void testNativeNotInitialized_useDefault() {
        MutableFlagWithSafeDefault featureA = new MutableFlagWithSafeDefault(FEATURE_A, false);
        MutableFlagWithSafeDefault featureB = new MutableFlagWithSafeDefault(FEATURE_B, true);

        // Query the flags to make sure the default values are returned.
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);
    }

    @Test
    public void testNativeInitializedUsedDefault_getsFromChromeFeatureList() {
        MutableFlagWithSafeDefault featureA = new MutableFlagWithSafeDefault(FEATURE_A, false);
        MutableFlagWithSafeDefault featureB = new MutableFlagWithSafeDefault(FEATURE_B, true);

        // Query the flags to make sure the default values are returned.
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);

        // Values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_ON_B_OFF);

        // Verify that {@link MutableFlagWithSafeDefault} returns native values.
        assertIsEnabledMatches(A_ON_B_OFF, featureA, featureB);
    }
}
