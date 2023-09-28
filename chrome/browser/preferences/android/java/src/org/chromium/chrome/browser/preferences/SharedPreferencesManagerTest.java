// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertFalse;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link SharedPreferencesManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedPreferencesManagerTest {
    @Test
    @SmallTest
    public void testPrefsAreWipedBetweenTests_1() {
        doTestPrefsAreWipedBetweenTests();
    }

    @Test
    @SmallTest
    public void testPrefsAreWipedBetweenTests_2() {
        doTestPrefsAreWipedBetweenTests();
    }

    /**
     * {@link #testPrefsAreWipedBetweenTests_1()} and {@link #testPrefsAreWipedBetweenTests_2()}
     * each set the same preference and fail if it has been set previously. Whichever order these
     * tests are run, either will fail if the prefs are not wiped between tests.
     */
    private void doTestPrefsAreWipedBetweenTests() {
        // Disable key checking for this test because "dirty_pref" isn't registered in the "in use"
        // list.
        SharedPreferencesManager.getInstance().disableKeyCheckerForTesting();

        // If the other test has set this flag and it was not wiped out, fail.
        assertFalse(SharedPreferencesManager.getInstance().readBoolean("dirty_pref", false));

        // Set the flag so the other test ensures it was wiped out.
        SharedPreferencesManager.getInstance().writeBoolean("dirty_pref", true);
    }
}
