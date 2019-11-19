// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.support.v7.app.AppCompatDelegate;

import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences;
import org.chromium.chrome.test.ui.DummyUiActivity;

import java.util.Arrays;
import java.util.List;

/**
 * Helper methods to be used in tests to specify night mode state.
 */
public class NightModeTestUtils {
    /**
     * {@link ParameterProvider} used for parameterized test that provides the night mode state.
     */
    public static class NightModeParams implements ParameterProvider {
        private static List<ParameterSet> sNightModeParams =
                Arrays.asList(new ParameterSet().value(false).name("NightModeDisabled"),
                        new ParameterSet().value(true).name("NightModeEnabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sNightModeParams;
        }
    }

    /**
     * Sets up the night mode state for {@link DummyUiActivity}.
     * @param nightModeEnabled Whether night mode should be enabled.
     */
    public static void setUpNightModeForDummyUiActivity(boolean nightModeEnabled) {
        AppCompatDelegate.setDefaultNightMode(nightModeEnabled ? AppCompatDelegate.MODE_NIGHT_YES
                                                               : AppCompatDelegate.MODE_NIGHT_NO);
    }

    /**
     * Resets the night mode state for {@link DummyUiActivity}.
     */
    public static void tearDownNightModeForDummyUiActivity() {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM);
    }

    /**
     * Sets up initial states for night mode before
     * {@link org.chromium.chrome.browser.ChromeActivity} is launched.
     */
    public static void setUpNightModeBeforeChromeActivityLaunched() {
        FeatureUtilities.setNightModeAvailableForTesting(true);
        NightModeUtils.setNightModeSupportedForTesting(true);
    }

    /**
     * Sets up the night mode state for {@link org.chromium.chrome.browser.ChromeActivity}.
     * @param nightModeEnabled Whether night mode should be enabled.
     */
    public static void setUpNightModeForChromeActivity(boolean nightModeEnabled) {
        SharedPreferencesManager.getInstance().writeInt(ChromePreferenceKeys.UI_THEME_SETTING_KEY,
                nightModeEnabled ? ThemePreferences.ThemeSetting.DARK
                                 : ThemePreferences.ThemeSetting.LIGHT);
    }

    /**
     * Resets the night mode state after {@link org.chromium.chrome.browser.ChromeActivity} is
     * destroyed.
     */
    public static void tearDownNightModeAfterChromeActivityDestroyed() {
        FeatureUtilities.setNightModeAvailableForTesting(null);
        NightModeUtils.setNightModeSupportedForTesting(null);
        GlobalNightModeStateProviderHolder.resetInstanceForTesting();
        SharedPreferencesManager.getInstance().removeKey(ChromePreferenceKeys.UI_THEME_SETTING_KEY);
    }
}
