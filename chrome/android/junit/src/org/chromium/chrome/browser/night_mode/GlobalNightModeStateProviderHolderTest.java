// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING_KEY;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences;

/**
 * Unit tests for {@link GlobalNightModeStateProviderHolder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GlobalNightModeStateProviderHolderTest {
    @After
    public void tearDown() {
        FeatureUtilities.setNightModeAvailableForTesting(null);
        GlobalNightModeStateProviderHolder.resetInstanceForTesting();
        SharedPreferencesManager.getInstance().removeKey(UI_THEME_SETTING_KEY);
    }

    @Test
    public void testNightModeNotAvailable() {
        FeatureUtilities.setNightModeAvailableForTesting(false);

        // Verify that night mode is disabled.
        assertFalse(GlobalNightModeStateProviderHolder.getInstance().isInNightMode());

        // Verify that night mode cannot be enabled.
        SharedPreferencesManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.DARK);
        assertFalse(GlobalNightModeStateProviderHolder.getInstance().isInNightMode());
    }

    @Test
    public void testNightModeAvailable() {
        // Verify that the instance is a GlobalNightModeStateController. Other tests are covered
        // in GlobalNightModeStateControllerTest.java.
        FeatureUtilities.setNightModeAvailableForTesting(true);
        assertTrue(GlobalNightModeStateProviderHolder.getInstance()
                           instanceof GlobalNightModeStateController);
    }
}
