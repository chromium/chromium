// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/**
 * Tests for {@link TabsSettings}, specifically for the Always open Custom Tabs in Chrome setting.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
@DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class TabsSettingsTest {
    @Rule
    public final SettingsActivityTestRule<TabsSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(TabsSettings.class);

    @Before
    @After
    public void cleanUpSharedPreferences() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER);
    }

    @After
    public void tearDown() {
        if (mSettingsActivityTestRule.getActivity() != null) {
            mSettingsActivityTestRule.getActivity().finish();
        }
    }

    @Test
    @SmallTest
    @Feature({"Settings"})
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testCctAlwaysOpenInBrowserPreference_VisibleWhenFeatureEnabled() {
        mSettingsActivityTestRule.startSettingsActivity();
        TabsSettings tabsSettings = mSettingsActivityTestRule.getFragment();

        Preference preference =
                tabsSettings.findPreference(TabsSettings.PREF_CCT_ALWAYS_OPEN_IN_BROWSER);
        Assert.assertNotNull(
                "Always open custom tabs preference should be present when flag is enabled.",
                preference);
        Assert.assertTrue(
                "Preference should be a ChromeSwitchPreference.",
                preference instanceof ChromeSwitchPreference);
        Assert.assertTrue("Preference should be visible.", preference.isVisible());
    }

    @Test
    @SmallTest
    @Feature({"Settings"})
    @DisableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testCctAlwaysOpenInBrowserPreference_HiddenWhenFeatureDisabled() {
        mSettingsActivityTestRule.startSettingsActivity();
        TabsSettings tabsSettings = mSettingsActivityTestRule.getFragment();

        Preference preference =
                tabsSettings.findPreference(TabsSettings.PREF_CCT_ALWAYS_OPEN_IN_BROWSER);
        Assert.assertNotNull(
                "Always open custom tabs preference object should be in hierarchy.", preference);
        Assert.assertFalse(
                "Always open custom tabs preference should be hidden when flag is disabled.",
                preference.isVisible());
    }

    @Test
    @SmallTest
    @Feature({"Settings"})
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testCctAlwaysOpenInBrowserPreference_TogglingUpdatesSharedPreferences() {
        mSettingsActivityTestRule.startSettingsActivity();
        TabsSettings tabsSettings = mSettingsActivityTestRule.getFragment();

        ChromeSwitchPreference pref =
                (ChromeSwitchPreference)
                        tabsSettings.findPreference(TabsSettings.PREF_CCT_ALWAYS_OPEN_IN_BROWSER);
        Assert.assertNotNull(
                "Always open custom tabs preference should be present when flag is enabled.", pref);

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        Assert.assertFalse("Switch should be unchecked by default.", pref.isChecked());
        Assert.assertFalse(
                "Preference should be false by default in SharedPreferences.",
                prefs.readBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, false));

        ThreadUtils.runOnUiThreadBlocking(pref::performClick);
        Assert.assertTrue("Switch should be checked after click.", pref.isChecked());
        Assert.assertTrue(
                "Preference should be true in SharedPreferences after toggling on.",
                prefs.readBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, false));

        ThreadUtils.runOnUiThreadBlocking(pref::performClick);
        Assert.assertFalse("Switch should be unchecked after second click.", pref.isChecked());
        Assert.assertFalse(
                "Preference should be false in SharedPreferences after toggling off.",
                prefs.readBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, false));
    }
}
