// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.chromium.chrome.browser.flags.BaseFlagTestRule.A_OFF_B_ON;
import static org.chromium.chrome.browser.flags.BaseFlagTestRule.FEATURE_A;
import static org.chromium.chrome.browser.flags.BaseFlagTestRule.FEATURE_B;
import static org.chromium.chrome.browser.flags.BaseFlagTestRule.assertIsEnabledMatches;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit Tests for {@link PostNativeFlag}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PostNativeFlagUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test(expected = AssertionError.class)
    public void testDuplicateFeature_throwsException() {
        new PostNativeFlag(FEATURE_A);
        new PostNativeFlag(FEATURE_A);
    }

    @Test(expected = AssertionError.class)
    public void testNativeNotInitialized_throwAssertionError() {
        PostNativeFlag featureA = new PostNativeFlag(FEATURE_A);
        featureA.isEnabled();
    }

    @Test
    public void testNativeInitialized_getsFromChromeFeatureList() {
        PostNativeFlag featureA = new PostNativeFlag(FEATURE_A);
        PostNativeFlag featureB = new PostNativeFlag(FEATURE_B);

        // Values from ChromeFeatureList should be used from now on.
        FeatureList.setTestFeatures(A_OFF_B_ON);

        // Assert {@link MutableFlagWithSafeDefault} uses the values from {@link ChromeFeatureList}.
        assertIsEnabledMatches(A_OFF_B_ON, featureA, featureB);
    }
}
