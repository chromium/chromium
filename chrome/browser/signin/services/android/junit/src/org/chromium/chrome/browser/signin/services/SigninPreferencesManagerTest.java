// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link SigninPreferencesManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninPreferencesManagerTest {
    @Test
    public void testAccountsChangedPref() {
        SigninPreferencesManager prefsManager = SigninPreferencesManager.getInstance();
        assertFalse("Should never return true before the pref has ever been set.",
                prefsManager.checkAndClearAccountsChangedPref());
        assertFalse("Should never return true before the pref has ever been set.",
                prefsManager.checkAndClearAccountsChangedPref());

        // Mark the pref as set.
        prefsManager.markAccountsChangedPref();

        assertTrue("Should return true first time after marking accounts changed",
                prefsManager.checkAndClearAccountsChangedPref());
        assertFalse("Should only return true first time after marking accounts changed",
                prefsManager.checkAndClearAccountsChangedPref());
        assertFalse("Should only return true first time after marking accounts changed",
                prefsManager.checkAndClearAccountsChangedPref());

        // Mark the pref as set again.
        prefsManager.markAccountsChangedPref();

        assertTrue("Should return true first time after marking accounts changed",
                prefsManager.checkAndClearAccountsChangedPref());
        assertFalse("Should only return true first time after marking accounts changed",
                prefsManager.checkAndClearAccountsChangedPref());
        assertFalse("Should only return true first time after marking accounts changed",
                prefsManager.checkAndClearAccountsChangedPref());
    }
}
