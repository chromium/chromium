// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;
import static org.chromium.chrome.browser.settings.MainSettings.PREF_TOOLBAR_SHORTCUT;
import static org.chromium.chrome.browser.settings.MainSettings.PREF_UI_THEME;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.NEW_TAB;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.NONE;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;

/** Tests for {@link AppearanceSettingsFragment}. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class AppearanceSettingsFragmentTest {

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    private AppearanceSettingsFragment mSettings;

    @Test
    @SmallTest
    public void testToolbarShortcutPreferenceIsAbsentWhenDisabled() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(NONE);
        launchSettings();
        Assert.assertNull(mSettings.findPreference(PREF_TOOLBAR_SHORTCUT));
    }

    @Test
    @SmallTest
    public void testToolbarShortcutPreferenceIsPresentWhenEnabled() throws ClassNotFoundException {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(NEW_TAB);
        launchSettings();
        assertSettingsExists(PREF_TOOLBAR_SHORTCUT, AdaptiveToolbarSettingsFragment.class);
    }

    @Test
    @SmallTest
    public void testUiThemePreference() throws ClassNotFoundException {
        launchSettings();

        final var uiThemePref = assertSettingsExists(PREF_UI_THEME, ThemeSettingsFragment.class);
        Assert.assertEquals(
                ThemeSettingsEntry.SETTINGS,
                uiThemePref.getExtras().getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));

        final var context = mSettings.getContext();
        Assert.assertEquals(
                NightModeUtils.getThemeSettingTitle(context, NightModeUtils.getThemeSetting()),
                uiThemePref.getSummary());

        final var prefs = ChromeSharedPreferences.getInstance();
        for (int theme = 0; theme < ThemeType.NUM_ENTRIES; theme++) {
            ThreadUtils.runOnUiThreadBlocking(mSettings::onPause);
            prefs.writeInt(UI_THEME_SETTING, theme);
            ThreadUtils.runOnUiThreadBlocking(mSettings::onResume);
            Assert.assertEquals(
                    NightModeUtils.getThemeSettingTitle(context, theme), uiThemePref.getSummary());
        }
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
        mSettingsTestRule.launchPreference(
                AppearanceSettingsFragment.class,
                /* fragmentArgs= */ null,
                (fragment) -> ((AppearanceSettingsFragment) fragment).setProfile(mProfile));
        mSettings = (AppearanceSettingsFragment) mSettingsTestRule.getPreferenceFragment();
    }
}
