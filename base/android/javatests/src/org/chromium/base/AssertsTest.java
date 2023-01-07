// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.build.BuildConfig;

/**
 * Test that ensures Java asserts are working.
 *
 * Not a robolectric test because we want to make sure asserts are enabled after dexing.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AssertsTest {
    private static final String TAG = "AssertsTest";

    @Test
    @SmallTest
    @SuppressWarnings("UseCorrectAssertInTests")
    public void testAssertsWorkAsExpected() {
        Log.i(TAG, "BuildConfig.ENABLE_ASSERTS=%b", BuildConfig.ENABLE_ASSERTS);
        if (BuildConfig.ENABLE_ASSERTS) {
            try {
                assert false;
            } catch (AssertionError e) {
                // When asserts are enabled, asserts should throw AssertionErrors.
                return;
            }
            Assert.fail("Java assert unexpectedly didn't fire.");
        } else {
            // When asserts are disabled, asserts should be removed by proguard.
            assert false : "Java assert unexpectedly fired.";
        }
    }
}
