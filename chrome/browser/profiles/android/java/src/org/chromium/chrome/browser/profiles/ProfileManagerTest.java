// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link ProfileManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ProfileManagerTest {
    @Test
    public void testLastUsedProfileBaseName() {
        assertNull(ProfileManager.getLastUsedProfileBaseName());

        String baseName = "Profile 1";
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.PROFILES_LAST_USED_PROFILE_BASE_NAME, baseName);

        assertEquals(baseName, ProfileManager.getLastUsedProfileBaseName());
    }
}
