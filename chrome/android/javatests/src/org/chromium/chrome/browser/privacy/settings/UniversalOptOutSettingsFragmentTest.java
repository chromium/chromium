// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Tests for {@link UniversalOptOutSettings}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.UNIVERSAL_OPT_OUT_SETTINGS)
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public class UniversalOptOutSettingsFragmentTest {
    @Rule
    public final SettingsActivityTestRule<UniversalOptOutSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(UniversalOptOutSettings.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsIndexData mSearchIndexDataMock;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        // Assume default is false, but clear just in case.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().clearPref(Pref.UNIVERSAL_OPT_OUT_ENABLED);
                });
    }

    private PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    @Test
    @LargeTest
    public void testSwitchState_MatchesPrefWhenDisabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED, false);
                });

        mSettingsActivityTestRule.startSettingsActivity();
        UniversalOptOutSettings fragment = mSettingsActivityTestRule.getFragment();
        ChromeSwitchPreference switchPref =
                fragment.findPreference(UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_SWITCH);
        assertNotNull(switchPref);

        assertFalse(switchPref.isChecked());
    }

    @Test
    @LargeTest
    public void testSwitchState_MatchesPrefWhenEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED, true);
                });

        mSettingsActivityTestRule.startSettingsActivity();
        UniversalOptOutSettings fragment = mSettingsActivityTestRule.getFragment();
        ChromeSwitchPreference switchPref =
                fragment.findPreference(UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_SWITCH);
        assertNotNull(switchPref);

        assertTrue(switchPref.isChecked());
    }

    @Test
    @LargeTest
    public void testSwitch_TogglesPref() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED, false);
                });

        mSettingsActivityTestRule.startSettingsActivity();
        UniversalOptOutSettings fragment = mSettingsActivityTestRule.getFragment();
        ChromeSwitchPreference switchPref =
                fragment.findPreference(UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_SWITCH);
        assertNotNull(switchPref);

        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getPrefService().getBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED)));

        // Click the switch preference
        ThreadUtils.runOnUiThreadBlocking(() -> switchPref.performClick());

        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getPrefService().getBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED)));
    }

    @Test
    @LargeTest
    public void testInfoTextIsDisplayed() {
        mSettingsActivityTestRule.startSettingsActivity();
        UniversalOptOutSettings fragment = mSettingsActivityTestRule.getFragment();
        Preference infoTextPref =
                fragment.findPreference(UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_INFO_TEXT);
        assertNotNull(infoTextPref);

        String expectedSummary =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.universal_opt_out_info_text);
        assertEquals(expectedSummary, infoTextPref.getSummary().toString());
    }

    @Test
    @LargeTest
    public void testPageTitle() {
        mSettingsActivityTestRule.startSettingsActivity();
        UniversalOptOutSettings fragment = mSettingsActivityTestRule.getFragment();
        String expectedTitle =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.universal_opt_out_sub_page_title);
        assertEquals(expectedTitle, fragment.getPageTitle().get());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.UNIVERSAL_OPT_OUT_SETTINGS)
    public void testSearchableIndex_RemovedWhenNonEligible() {
        var indexProvider = UniversalOptOutSettings.SEARCH_INDEX_DATA_PROVIDER;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED, false);
                    getPrefService().setBoolean(Pref.UNIVERSAL_OPT_OUT_ELIGIBLE, false);
                    indexProvider.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            ProfileManager.getLastUsedRegularProfile());
                });

        verify(mSearchIndexDataMock)
                .removeEntry(
                        indexProvider.getUniqueId(
                                UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_SWITCH));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        indexProvider.getUniqueId(
                                UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_INFO_TEXT));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.UNIVERSAL_OPT_OUT_SETTINGS)
    public void testSearchableIndex_RemovedWhenFeatureDisabled() {
        var indexProvider = UniversalOptOutSettings.SEARCH_INDEX_DATA_PROVIDER;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    indexProvider.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            ProfileManager.getLastUsedRegularProfile());
                });

        verify(mSearchIndexDataMock)
                .removeEntry(
                        indexProvider.getUniqueId(
                                UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_SWITCH));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        indexProvider.getUniqueId(
                                UniversalOptOutSettings.PREF_UNIVERSAL_OPT_OUT_INFO_TEXT));
    }
}
