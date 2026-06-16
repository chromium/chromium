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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.GLIC_AUTO_BROWSE_SETTING_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.GLIC_PRECISE_LOCATION_SETTING_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED;

import android.Manifest;
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
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link GlicSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.ANDROID_BOTTOM_BAR,
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL
})
@EnableFeatures(ChromeFeatureList.ACTOR_LOGIN_PERMISSIONS_UI)
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
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJniMock;
    @Mock private GlicKeyedServiceFactory.Natives mGlicKeyedServiceFactoryJniMock;
    @Mock private GlicKeyedService mGlicKeyedServiceMock;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJniMock);
        GlicKeyedServiceFactoryJni.setInstanceForTesting(mGlicKeyedServiceFactoryJniMock);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);

        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        when(mGlicKeyedServiceFactoryJniMock.getForProfile(mProfileMock))
                .thenReturn(mGlicKeyedServiceMock);
        when(mGlicEnablingJniMock.shouldShowWebActuationToggle(any())).thenReturn(true);
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
                mActivity.getString(R.string.glic_setting_label),
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
                        eq("https://myactivity.google.com/product/gemini?utm_source=gemini"));
    }

    @Test
    public void testClickExtensions() {
        GlicSettings fragment = launchFragment();
        Preference preference = fragment.findPreference("glic_extensions");
        preference.getOnPreferenceClickListener().onPreferenceClick(preference);
        verify(mCustomTabLauncher).openUrlInCct(any(), eq("https://gemini.google.com/apps"));
    }

    @Test
    public void testGlicButtonPreferenceExists() {
        GlicSettings fragment = launchFragment();
        Preference preference = fragment.findPreference("glic_button");
        assertTrue("Preference glic_button should exist", preference != null);
    }

    @Test
    public void testGlicButtonPreference_Glic() {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS,
                        AdaptiveToolbarButtonVariant.GLIC);
        GlicSettings fragment = launchFragment();
        Preference preference = fragment.findPreference("glic_button");
        assertEquals(
                mActivity.getString(R.string.glic_button_entrypoint_pinned_label),
                preference.getTitle());
        assertEquals(
                mActivity.getString(R.string.glic_button_entrypoint_label),
                preference.getSummary());
    }

    @Test
    public void testGlicButtonPreference_NotGlic() {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SETTINGS,
                        AdaptiveToolbarButtonVariant.AUTO);
        GlicSettings fragment = launchFragment();
        Preference preference = fragment.findPreference("glic_button");
        assertEquals(mActivity.getString(R.string.glic_pin), preference.getTitle());
        assertEquals(
                mActivity.getString(R.string.settings_glic_button_toggle_sublabel),
                preference.getSummary());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testGlicButtonPreference_SidePanel_Pinned() {
        when(mPrefServiceMock.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);
        GlicSettings fragment = launchFragment();

        Preference preference = fragment.findPreference("glic_button");
        assertFalse(
                "Preference glic_button should be invisible with side panel FF enabled",
                preference.isVisible());

        ChromeSwitchPreference togglePreference = fragment.findPreference("glic_button_toggle");
        assertTrue(
                "Preference glic_button_toggle should be visible with side panel FF enabled",
                togglePreference.isVisible());
        assertTrue("Toggle should be checked when Glic is pinned", togglePreference.isChecked());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testGlicButtonPreference_SidePanel_Toggle() {
        when(mPrefServiceMock.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(false);
        GlicSettings fragment = launchFragment();

        ChromeSwitchPreference togglePreference = fragment.findPreference("glic_button_toggle");
        assertFalse(
                "Toggle should not be checked when Glic is not pinned",
                togglePreference.isChecked());

        // Test toggling on
        togglePreference.getOnPreferenceChangeListener().onPreferenceChange(togglePreference, true);
        verify(mPrefServiceMock).setBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, true);

        // Test toggling off
        togglePreference
                .getOnPreferenceChangeListener()
                .onPreferenceChange(togglePreference, false);
        verify(mPrefServiceMock).setBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, false);
    }

    @Test
    public void testLocationPermissionInitialState_Enabled() {
        doTestInitialState(GlicPrefNames.GLIC_GEOLOCATION_ENABLED, "permissions_location", true);
    }

    @Test
    public void testLocationPermissionInitialState_Disabled() {
        doTestInitialState(GlicPrefNames.GLIC_GEOLOCATION_ENABLED, "permissions_location", false);
    }

    @Test
    public void testLocationPermissionToggle() {
        doTestToggle(
                GLIC_PRECISE_LOCATION_SETTING_ENABLED,
                GlicPrefNames.GLIC_GEOLOCATION_ENABLED,
                "permissions_location");
    }

    @Test
    public void testTabAccessPermissionInitialState_Enabled() {
        doTestInitialState(
                GlicPrefNames.GLIC_DEFAULT_TAB_CONTEXT_ENABLED,
                "glic_permissions_default_tab_access",
                true);
    }

    @Test
    public void testTabAccessPermissionInitialState_Disabled() {
        doTestInitialState(
                GlicPrefNames.GLIC_DEFAULT_TAB_CONTEXT_ENABLED,
                "glic_permissions_default_tab_access",
                false);
    }

    @Test
    public void testTabAccessPermissionToggle() {
        doTestToggle(
                GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED,
                GlicPrefNames.GLIC_DEFAULT_TAB_CONTEXT_ENABLED,
                "glic_permissions_default_tab_access");
    }

    @Test
    public void testAutoBrowsePermissionInitialState_Enabled() {
        when(mGlicKeyedServiceMock.getUserEnabledActuationOnWeb()).thenReturn(true);
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference preference = fragment.findPreference("glic_permissions_auto_browse");
        assertTrue(preference.isChecked());
    }

    @Test
    public void testAutoBrowsePermissionInitialState_Disabled() {
        when(mGlicKeyedServiceMock.getUserEnabledActuationOnWeb()).thenReturn(false);
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference preference = fragment.findPreference("glic_permissions_auto_browse");
        assertFalse(preference.isChecked());
    }

    @Test
    public void testAutoBrowsePermissionToggle() {
        when(mGlicKeyedServiceMock.getUserEnabledActuationOnWeb()).thenReturn(false);
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference preference = fragment.findPreference("glic_permissions_auto_browse");

        // Test toggling on
        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, true);
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(GLIC_AUTO_BROWSE_SETTING_ENABLED, false));
        verify(mGlicKeyedServiceMock).setUserEnabledActuationOnWeb(true);

        // Test toggling off
        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, false);
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(GLIC_AUTO_BROWSE_SETTING_ENABLED, true));
        verify(mGlicKeyedServiceMock).setUserEnabledActuationOnWeb(false);
    }

    private void doTestToggle(String sharedPrefKey, String profilePrefKey, String viewId) {
        when(mPrefServiceMock.getBoolean(profilePrefKey)).thenReturn(false);
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference preference = fragment.findPreference(viewId);

        // Test toggling on
        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, true);
        assertTrue(ChromeSharedPreferences.getInstance().readBoolean(sharedPrefKey, false));
        verify(mPrefServiceMock).setBoolean(profilePrefKey, true);

        // Test toggling off
        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, false);
        assertFalse(ChromeSharedPreferences.getInstance().readBoolean(sharedPrefKey, true));
        verify(mPrefServiceMock).setBoolean(profilePrefKey, false);
    }

    private void doTestInitialState(String profilePrefKey, String prefName, boolean initialState) {
        when(mPrefServiceMock.getBoolean(profilePrefKey)).thenReturn(initialState);
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference preference = fragment.findPreference(prefName);
        assertEquals(initialState, preference.isChecked());
    }

    @Test
    public void testStartupSync_PermissionDenied() {
        when(mPrefServiceMock.getBoolean("glic.geolocation_enabled")).thenReturn(true);
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .denyPermissions(Manifest.permission.ACCESS_FINE_LOCATION);

        GlicSettings fragment = launchFragment();

        // Verifies startup validation does NOT turn it off
        verify(mPrefServiceMock, never()).setBoolean("glic.geolocation_enabled", false);
        ChromeSwitchPreference locationPref = fragment.findPreference("permissions_location");
        assertEquals(true, locationPref.isChecked());
    }

    @Test
    public void testOnPreferenceChange_FinePermission() {
        GlicSettings fragment = launchFragment();
        ChromeSwitchPreference locationPref = fragment.findPreference("permissions_location");

        // Simulate toggle On
        locationPref.getOnPreferenceChangeListener().onPreferenceChange(locationPref, true);
        verify(mPrefServiceMock).setBoolean("glic.geolocation_enabled", true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testGlicButtonPreference_AndroidBottomBarEnabled() {
        GlicSettings fragment = launchFragment();
        Preference preference = fragment.findPreference("glic_button");
        assertFalse("Preference glic_button should not be visible", preference.isVisible());
        Preference preferenceCategory = fragment.findPreference("glic_preference_section");
        assertFalse(
                "Preference glic_preference_section should not be visible",
                preferenceCategory.isVisible());
    }

    @Test
    public void testEnterpriseMode_MovesToBottom() {
        when(mGlicEnablingJniMock.isProfileManaged(mProfileMock)).thenReturn(true);
        when(mGlicEnablingJniMock.isDisabledByPolicy(mProfileMock)).thenReturn(false);

        GlicSettings fragment = launchFragment();

        GlicExtraInfoPreference aiInfoPref = fragment.findPreference("glic_custom_box_preference");
        assertTrue("Preference glic_custom_box_preference should exist", aiInfoPref != null);

        assertEquals("Order should be 999 for enterprise", 999, aiInfoPref.getOrder());
    }
}
