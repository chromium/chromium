// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.SessionStartupPref;
import org.chromium.chrome.browser.multiwindow.TabbedStartupCoordinator;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Instrumentation tests for {@link OnStartupSettingsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ON_STARTUP_SETTINGS_UI)
public class OnStartupSettingsFragmentTest {
    @Rule
    public final SettingsTestRule<OnStartupSettingsFragment> mSettingsTestRule =
            new SettingsTestRule<>(OnStartupSettingsFragment.class);

    private Profile mProfile;

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(/* isDesktop= */ true);
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                });
    }

    @After
    public void tearDown() {
        setRestoreOnStartupPref(SessionStartupPref.LAST);
    }

    @Test
    @SmallTest
    public void testInitialSelection_matchesStoredStartupPref() {
        // Setup stored startup preference as URLS.
        setRestoreOnStartupPref(SessionStartupPref.URLS);

        // Act.
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();

        // Verify initial selection matches URLS and the other radio buttons are unchecked.
        assertFalse(pref.getNewTabPrefForTesting().isChecked());
        assertFalse(pref.getLastPrefForTesting().isChecked());
        assertTrue(pref.getUrlsPrefForTesting().isChecked());
    }

    @Test
    @SmallTest
    public void testSelectingRadioButtons_updatesPrefService() {
        // Setup stored startup preference as NEW_TAB.
        setRestoreOnStartupPref(SessionStartupPref.NEW_TAB);
        RadioButtonGroupOnStartupPreference pref = launchAndGetRadioGroupPreference();
        assertTrue(pref.getNewTabPrefForTesting().isChecked());

        // Act & Verify selecting LAST updates PrefService.
        ThreadUtils.runOnUiThreadBlocking(() -> pref.getLastPrefForTesting().performClick());
        assertEquals(SessionStartupPref.LAST, getRestoreOnStartupPref());

        // Act & Verify selecting URLS updates PrefService.
        ThreadUtils.runOnUiThreadBlocking(() -> pref.getUrlsPrefForTesting().performClick());
        assertEquals(SessionStartupPref.URLS, getRestoreOnStartupPref());
    }

    private RadioButtonGroupOnStartupPreference launchAndGetRadioGroupPreference() {
        mSettingsTestRule.startSettingsActivity();
        OnStartupSettingsFragment fragment = mSettingsTestRule.getFragment();
        return fragment.findPreference(OnStartupSettingsFragment.PREF_RADIO_BUTTON_GROUP);
    }

    private void setRestoreOnStartupPref(@SessionStartupPref int startupType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> TabbedStartupCoordinator.setSessionStartupType(mProfile, startupType));
    }

    private @SessionStartupPref int getRestoreOnStartupPref() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> UserPrefs.get(mProfile).getInteger(Pref.RESTORE_ON_STARTUP));
    }
}
