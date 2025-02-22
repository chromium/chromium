// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import static org.chromium.chrome.browser.settings.MainSettings.PREF_UI_THEME;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;

/** Tests for {@link AppearanceSettingsFragment}. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class AppearanceSettingsFragmentTest {

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    private AppearanceSettingsFragment mSettings;

    @Test
    @SmallTest
    public void testUiThemePreference() throws ClassNotFoundException {
        launchSettings();
        final var uiThemePref = assertSettingsExists(PREF_UI_THEME, ThemeSettingsFragment.class);
        Assert.assertEquals(
                ThemeSettingsEntry.SETTINGS,
                uiThemePref.getExtras().getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));
    }

    private @NonNull Preference assertSettingsExists(
            @NonNull String prefKey, @NonNull Class settingsFragmentClass)
            throws ClassNotFoundException {
        final Preference pref = mSettings.findPreference(prefKey);
        Assert.assertNotNull(pref);
        Assert.assertNotNull(pref.getFragment());
        Assert.assertEquals(settingsFragmentClass, Class.forName(pref.getFragment()));
        return pref;
    }

    private void launchSettings() {
        mSettingsTestRule.launchPreference(AppearanceSettingsFragment.class);
        mSettings = (AppearanceSettingsFragment) mSettingsTestRule.getPreferenceFragment();
    }
}
