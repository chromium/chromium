// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.base.test.util.BaseFlagTestRule.A_OFF_B_ON;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_A;
import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_B;
import static org.chromium.base.test.util.BaseFlagTestRule.assertIsEnabledMatches;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;

/** Unit Tests for {@link PostNativeFlag}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PostNativeFlagUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test(expected = AssertionError.class)
    public void testDuplicateFeature_throwsException() {
        new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
        new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
    }

    @Test(expected = AssertionError.class)
    public void testNativeNotInitialized_throwAssertionError() {
        PostNativeFlag featureA = new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);

        // Disable test feature short circuit so the test goes through the same code
        // path as prod chrome.
        FeatureList.setDisableNativeForTesting(false);

        featureA.isEnabled();
    }

    @Test
    public void testNativeInitialized_getsFromChromeFeatureList() {
        PostNativeFlag featureA = new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
        PostNativeFlag featureB = new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_B);

        // Values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_OFF_B_ON);

        // Assert {@link MutableFlagWithSafeDefault} uses the values from {@link ChromeFeatureList}.
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);
    }
}
