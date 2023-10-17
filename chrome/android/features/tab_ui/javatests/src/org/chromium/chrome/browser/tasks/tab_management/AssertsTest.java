// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Test that ensures Java asserts are working in tab_management feature module.
 *
 * <p>Not a robolectric test because we want to make sure asserts are enabled after dexing.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AssertsTest {
    @Test
    @SmallTest
    @SuppressWarnings("UseCorrectAssertInTests")
    public void assertInTests() {
        if (BuildConfig.ENABLE_ASSERTS) {
            try {
                assert false;
            } catch (AssertionError e) {
                // When DCHECK is on, asserts should throw AssertionErrors.
                return;
            }
            Assert.fail("Java assert unexpectedly didn't fire.");
        } else {
            // When DCHECK isn't on, asserts should be removed by proguard.
            assert false : "Java assert unexpectedly fired.";
        }
    }

    @Test
    @SmallTest
    @SuppressWarnings("UseCorrectAssertInTests")
    public void assertInModule() {
        if (BuildConfig.ENABLE_ASSERTS) {
            try {
                TabGroupUtils.triggerAssertionForTesting();
            } catch (AssertionError e) {
                // When DCHECK is on, asserts should throw AssertionErrors.
                return;
            }
            Assert.fail("Java assert unexpectedly didn't fire.");
        } else {
            // When DCHECK isn't on, asserts should be removed by proguard.
            TabGroupUtils.triggerAssertionForTesting();
        }
    }
}
