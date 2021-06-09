// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for the AppLocalUtils class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AppLocaleUtilsTest {
    // Test getAppLanguagePref.
    @Test
    @SmallTest
    public void testGetAppLanguagePref() {
        String lang = AppLocaleUtils.getAppLanguagePref();
        Assert.assertEquals(null, lang);

        AppLocaleUtils.setAppLanguagePref("en-US");
        lang = AppLocaleUtils.getAppLanguagePref();
        Assert.assertEquals("en-US", lang);
    }

    // Test setAppLanguagePref.
    @Test
    @SmallTest
    public void testSetAppLanguagePref() {
        assertLanguagePrefEquals(null);

        AppLocaleUtils.setAppLanguagePref("en-US");
        assertLanguagePrefEquals("en-US");

        AppLocaleUtils.setAppLanguagePref("fr");
        assertLanguagePrefEquals("fr");
    }

    // Test isAppLanguagePref.
    @Test
    @SmallTest
    public void testIsAppLanguagePref() {
        Assert.assertFalse(AppLocaleUtils.isAppLanguagePref("en"));

        AppLocaleUtils.setAppLanguagePref("en-US");
        Assert.assertTrue(AppLocaleUtils.isAppLanguagePref("en-US"));

        Assert.assertFalse(AppLocaleUtils.isAppLanguagePref("en"));
    }

    // Helper function to manually get and check AppLanguagePref.
    private void assertLanguagePrefEquals(String language) {
        Assert.assertEquals(language,
                SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, null));
    }
}
