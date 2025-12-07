// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.base.test.util.BaseFlagTestRule.FEATURE_A;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;

/** Unit Tests for {@link Flag}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FlagUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    private static final FeatureMap FEATURE_MAP = BaseFlagTestRule.FEATURE_MAP;

    @Test
    public void testDuplicateFeatureFlags_throwsAssertionError_PostNativeAndSafeDefault() {
        new PostNativeFlag(BaseFlagTestRule.FEATURE_MAP, FEATURE_A);
        try {
            new MutableFlagWithSafeDefault(FEATURE_MAP, FEATURE_A, false);
            throw new RuntimeException("Duplicate feature");
        } catch (AssertionError e) {
        }
    }
}
