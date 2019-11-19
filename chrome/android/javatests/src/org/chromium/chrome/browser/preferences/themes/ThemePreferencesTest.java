// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.themes;

import static org.chromium.chrome.browser.ChromeFeatureList.ANDROID_NIGHT_MODE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING_KEY;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences.ThemeSetting;
import org.chromium.chrome.browser.ui.widget.RadioButtonWithDescription;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for ThemePreferences.
 */
// clang-format off
@Features.EnableFeatures(ANDROID_NIGHT_MODE)
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class ThemePreferencesTest extends DummyUiActivityTestCase {
    // clang-format on
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("DefaultLightDisabled"),
                    new ParameterSet().value(true).name("DefaultLightEnabled"));

    private boolean mDefaultToLight;
    private ThemePreferences mFragment;
    private RadioButtonGroupThemePreference mPreference;

    public ThemePreferencesTest(boolean defaultToLight) {
        mDefaultToLight = defaultToLight;
        FeatureUtilities.setNightModeDefaultToLightForTesting(defaultToLight);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        SharedPreferencesManager.getInstance().removeKey(UI_THEME_SETTING_KEY);
        Preferences preferences = PreferencesTest.startPreferences(
                InstrumentationRegistry.getInstrumentation(), ThemePreferences.class.getName());
        mFragment = (ThemePreferences) preferences.getMainFragment();
        mPreference = (RadioButtonGroupThemePreference) mFragment.findPreference(
                ThemePreferences.PREF_UI_THEME_PREF);
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> SharedPreferencesManager.getInstance().removeKey(UI_THEME_SETTING_KEY));

        FeatureUtilities.setNightModeDefaultToLightForTesting(null);
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @Feature({"Themes"})
    public void testSelectThemes() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Default to light parameter is only applicable pre-Q.
            if (mDefaultToLight && BuildInfo.isAtLeastQ()) {
                Assert.assertFalse("Q should not default to light.",
                        FeatureUtilities.isNightModeDefaultToLight());
                return;
            }

            int expectedDefaultTheme =
                    mDefaultToLight ? ThemeSetting.LIGHT : ThemeSetting.SYSTEM_DEFAULT;
            Assert.assertEquals("Incorrect default theme setting.", expectedDefaultTheme,
                    NightModeUtils.getThemeSetting());
            assertButtonCheckedCorrectly(
                    mDefaultToLight ? "Light" : "System default", expectedDefaultTheme);

            // Select System default
            Assert.assertEquals(R.id.system_default, getButton(0).getId());
            selectButton(0);
            assertButtonCheckedCorrectly("System default", 0);
            Assert.assertEquals(ThemeSetting.SYSTEM_DEFAULT, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    SharedPreferencesManager.getInstance().readInt(UI_THEME_SETTING_KEY));

            // Select Light
            Assert.assertEquals(R.id.light, getButton(1).getId());
            selectButton(1);
            assertButtonCheckedCorrectly("Light", 1);
            Assert.assertEquals(ThemeSetting.LIGHT, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    SharedPreferencesManager.getInstance().readInt(UI_THEME_SETTING_KEY));

            // Select Dark
            Assert.assertEquals(R.id.dark, getButton(2).getId());
            selectButton(2);
            assertButtonCheckedCorrectly("Dark", 2);
            Assert.assertEquals(ThemeSetting.DARK, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    SharedPreferencesManager.getInstance().readInt(UI_THEME_SETTING_KEY));
        });
    }

    private RadioButtonWithDescription getButton(int index) {
        return (RadioButtonWithDescription) mPreference.getButtonsForTesting().get(index);
    }

    private void selectButton(int index) {
        getButton(index).onClick(null);
    }

    private boolean isRestUnchecked(int selectedIndex) {
        for (int i = 0; i < ThemeSetting.NUM_ENTRIES; i++) {
            if (i != selectedIndex && getButton(i).isChecked()) {
                return false;
            }
        }
        return true;
    }

    private void assertButtonCheckedCorrectly(String buttonTitle, int index) {
        Assert.assertTrue(buttonTitle + " button should be checked.", getButton(index).isChecked());
        Assert.assertTrue(
                "Buttons except " + buttonTitle + " should be unchecked.", isRestUnchecked(index));
    }
}
