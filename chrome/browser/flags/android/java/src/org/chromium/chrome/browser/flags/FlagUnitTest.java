// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.chromium.chrome.browser.flags.BaseFlagTestRule.FEATURE_A;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Flag;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit Tests for {@link Flag}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FlagUnitTest {
    @Rule public final BaseFlagTestRule mBaseFlagTestRule = new BaseFlagTestRule();

    @Test
    public void testDuplicateFeatureFlags_throwsAssertionError() {
        new PostNativeFlag(FEATURE_A);
        try {
            new MutableFlagWithSafeDefault(FEATURE_A, false);
            throw new RuntimeException("Duplicate feature");
        } catch (AssertionError e) {
        }
        try {
            new CachedFlag(FEATURE_A, false);
            throw new RuntimeException("Duplicate feature");
        } catch (AssertionError e) {
        }

        Flag.resetFlagsForTesting();

        new MutableFlagWithSafeDefault(FEATURE_A, false);
        try {
            new PostNativeFlag(FEATURE_A);
            throw new RuntimeException("Duplicate feature");
        } catch (AssertionError e) {
        }
        try {
            new CachedFlag(FEATURE_A, false);
            throw new RuntimeException("Duplicate feature");
        } catch (AssertionError e) {
        }

        Flag.resetFlagsForTesting();

        new CachedFlag(FEATURE_A, false);
        try {
            new MutableFlagWithSafeDefault(FEATURE_A, false);
            throw new RuntimeException("Duplicate feature");
        } catch (AssertionError e) {
        }
        try {
            new PostNativeFlag(FEATURE_A);
            throw new RuntimeException("Duplicate feature");
        } catch (AssertionError e) {
        }
    }
}
