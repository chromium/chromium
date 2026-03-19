// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.GLIC_AUTO_BROWSE_SETTING_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.GLIC_BUTTON_PINNED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.GLIC_PRECISE_LOCATION_SETTING_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED;

import android.content.Context;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.preference.Preference;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link GlicSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicSettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;

    @Mock private Profile mProfileMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        doNothing().when(mCustomTabLauncher).openUrlInCct(any(Context.class), anyString());

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    private GlicSettings launchFragment() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        GlicSettings glicSettings =
                (GlicSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        GlicSettings.class.getClassLoader(),
                                        GlicSettings.class.getName());
        glicSettings.setProfile(mProfileMock);
        glicSettings.setCustomTabLauncher(mCustomTabLauncher);
        fragmentManager.beginTransaction().replace(android.R.id.content, glicSettings).commit();
        mActivityScenarioRule.getScenario().moveToState(State.STARTED);

        assertEquals(
                mActivity.getString(R.string.settings_glic_button_toggle),
                glicSettings.getPageTitle().get());

        return glicSettings;
    }

    @Test
    public void testLaunchGlicSettings() {
        // Verifies that the fragment can be launched without crashing.
        launchFragment();
    }

    @Test
    public void testClickPermissionsActivity() {
        GlicSettings fragment = launchFragment();
        Preference preference = fragment.findPreference("glic_permissions_activity");
        preference.getOnPreferenceClickListener().onPreferenceClick(preference);
        verify(mCustomTabLauncher)
                .openUrlInCct(
                        any(),
                        eq(
                                mActivity.getString(
                                        R.string.settings_glic_permissions_activity_button_url)));
    }

    @Test
    public void testClickExtensions() {
        GlicSettings fragment = launchFragment();
        Preference preference = fragment.findPreference("glic_extensions");
        preference.getOnPreferenceClickListener().onPreferenceClick(preference);
        verify(mCustomTabLauncher)
                .openUrlInCct(
                        any(),
                        eq(mActivity.getString(R.string.settings_glic_extensions_button_url)));
    }

    @Test
    public void testGlicButtonPinnedInitialState_Enabled() {
        doTestInitialState(GLIC_BUTTON_PINNED, "glic_button", true);
    }

    @Test
    public void testGlicButtonPinnedInitialState_Disabled() {
        doTestInitialState(GLIC_BUTTON_PINNED, "glic_button", false);
    }

    @Test
    public void testGlicButtonPinnedToggle() {
        doTestToggle(GLIC_BUTTON_PINNED, "glic_button");
    }

    @Test
    public void testLocationPermissionInitialState_Enabled() {
        doTestInitialState(GLIC_PRECISE_LOCATION_SETTING_ENABLED, "permissions_location", true);
    }

    @Test
    public void testLocationPermissionInitialState_Disabled() {
        doTestInitialState(GLIC_PRECISE_LOCATION_SETTING_ENABLED, "permissions_location", false);
    }

    @Test
    public void testLocationPermissionToggle() {
        doTestToggle(GLIC_PRECISE_LOCATION_SETTING_ENABLED, "permissions_location");
    }

    @Test
    public void testTabAccessPermissionInitialState_Enabled() {
        doTestInitialState(
                GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED,
                "glic_permissions_default_tab_access",
                true);
    }

    @Test
    public void testTabAccessPermissionInitialState_Disabled() {
        doTestInitialState(
                GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED,
                "glic_permissions_default_tab_access",
                false);
    }

    @Test
    public void testTabAccessPermissionToggle() {
        doTestToggle(
                GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED,
                "glic_permissions_default_tab_access");
    }

    @Test
    public void testAutoBrowsePermissionInitialState_Enabled() {
        doTestInitialState(GLIC_AUTO_BROWSE_SETTING_ENABLED, "glic_permissions_auto_browse", true);
    }

    @Test
    public void testAutoBrowsePermissionInitialState_Disabled() {
        doTestInitialState(GLIC_AUTO_BROWSE_SETTING_ENABLED, "glic_permissions_auto_browse", false);
    }

    @Test
    public void testAutoBrowsePermissionToggle() {
        doTestToggle(GLIC_AUTO_BROWSE_SETTING_ENABLED, "glic_permissions_auto_browse");
    }

    private void doTestToggle(String sharedPreferenceKey, String preferenceKey) {
        ChromeSharedPreferences.getInstance().writeBoolean(sharedPreferenceKey, false);
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference preference = fragment.findPreference(preferenceKey);

        // Test toggling on
        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, true);
        assertTrue(
                "After toggling on, " + sharedPreferenceKey + " should be true.",
                ChromeSharedPreferences.getInstance().readBoolean(sharedPreferenceKey, false));

        // Test toggling off
        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, false);
        assertFalse(
                "After toggling off, " + sharedPreferenceKey + " should be false.",
                ChromeSharedPreferences.getInstance().readBoolean(sharedPreferenceKey, true));
    }

    private void doTestInitialState(String prefKey, String prefName, boolean initialState) {
        ChromeSharedPreferences.getInstance().writeBoolean(prefKey, initialState);
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference preference = fragment.findPreference(prefName);
        assertEquals(initialState, preference.isChecked());
    }
}
