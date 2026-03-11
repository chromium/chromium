// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.profiles.Profile;
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
}
