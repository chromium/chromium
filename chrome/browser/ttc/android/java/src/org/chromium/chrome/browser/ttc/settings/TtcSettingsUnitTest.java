// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.browser.ttc.R;
import org.chromium.chrome.browser.ttc.TtcKeyedService;
import org.chromium.chrome.browser.ttc.TtcKeyedServiceFactory;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TtcSettings} and {@link TtcSettingsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TTC)
public class TtcSettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfile;
    @Mock private Profile mOffTheRecordProfile;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarJni;
    @Mock private TtcKeyedService mService;

    private TestActivity mActivity;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mOffTheRecordProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    private TtcSettings launchFragment(Profile profile) {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        TtcSettings fragment =
                (TtcSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        TtcSettings.class.getClassLoader(),
                                        TtcSettings.class.getName());
        fragment.setProfile(profile);
        fragmentManager.beginTransaction().replace(android.R.id.content, fragment).commit();
        mActivityScenarioRule.getScenario().moveToState(State.STARTED);
        return fragment;
    }

    private static ChromeSwitchPreference getSwitch(TtcSettings fragment, String key) {
        ChromeSwitchPreference preference = fragment.findPreference(key);
        assertTrue("Missing switch " + key, preference != null);
        return preference;
    }

    @Test
    public void testShowsEverySetting() {
        TtcSettings fragment = launchFragment(mProfile);

        assertEquals(
                mActivity.getString(R.string.ttc_settings_title), fragment.getPageTitle().get());
        for (String key :
                new String[] {
                    TtcSettings.PREF_MICROPHONE_CHIME,
                    TtcSettings.PREF_DESCRIPTION,
                    TtcSettings.PREF_PIN_TO_TOOLBAR,
                    TtcSettings.PREF_CAPTIONS
                }) {
            Preference preference = fragment.findPreference(key);
            assertTrue("Missing " + key, preference != null && preference.isVisible());
        }
    }

    @Test
    public void testSwitchesShowSavedValues() {
        when(mPrefService.getBoolean(Pref.TTC_MICROPHONE_CHIME_ENABLED)).thenReturn(false);
        when(mPrefService.getBoolean(Pref.TTC_CAPTIONS_ENABLED)).thenReturn(true);

        TtcSettings fragment = launchFragment(mProfile);

        assertFalse(getSwitch(fragment, TtcSettings.PREF_MICROPHONE_CHIME).isChecked());
        assertTrue(getSwitch(fragment, TtcSettings.PREF_CAPTIONS).isChecked());
    }

    @Test
    public void testMicrophoneChimeSwitchSavesPref() {
        doTestProfilePrefSwitch(
                TtcSettings.PREF_MICROPHONE_CHIME, Pref.TTC_MICROPHONE_CHIME_ENABLED);
    }

    @Test
    public void testCaptionsSwitchSavesPref() {
        doTestProfilePrefSwitch(TtcSettings.PREF_CAPTIONS, Pref.TTC_CAPTIONS_ENABLED);
    }

    private void doTestProfilePrefSwitch(String key, String pref) {
        ChromeSwitchPreference preference = getSwitch(launchFragment(mProfile), key);

        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, true);
        verify(mPrefService).setBoolean(pref, true);

        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, false);
        verify(mPrefService).setBoolean(pref, false);
    }

    @Test
    public void testPinToToolbarOpensToolbarShortcutSettings() {
        Preference pinToToolbar =
                launchFragment(mProfile).findPreference(TtcSettings.PREF_PIN_TO_TOOLBAR);
        assertEquals(AdaptiveToolbarSettingsFragment.class.getName(), pinToToolbar.getFragment());
    }

    @Test
    public void testPinToToolbarSummaryShowsWhetherTtcIsPinned() {
        TtcSettings fragment = launchFragment(mProfile);
        Preference pinToToolbar = fragment.findPreference(TtcSettings.PREF_PIN_TO_TOOLBAR);
        assertEquals(
                mActivity.getString(R.string.ttc_settings_pin_to_toolbar_summary),
                pinToToolbar.getSummary().toString());

        // TTC is pinned on the toolbar shortcut settings page; coming back updates the summary.
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.TTC);
        fragment.onResume();
        assertEquals(
                mActivity.getString(R.string.ttc_settings_pin_to_toolbar_pinned_summary),
                pinToToolbar.getSummary().toString());

        // Turning toolbar shortcuts off unpins it.
        AdaptiveToolbarPrefs.saveToolbarSettingsToggleState(false);
        fragment.onResume();
        assertEquals(
                mActivity.getString(R.string.ttc_settings_pin_to_toolbar_summary),
                pinToToolbar.getSummary().toString());
    }

    @Test
    public void testOffTheRecordProfileUsesOriginalProfilePrefs() {
        when(mPrefService.getBoolean(Pref.TTC_CAPTIONS_ENABLED)).thenReturn(true);

        ChromeSwitchPreference preference =
                getSwitch(launchFragment(mOffTheRecordProfile), TtcSettings.PREF_CAPTIONS);

        assertTrue(preference.isChecked());
        preference.getOnPreferenceChangeListener().onPreferenceChange(preference, false);
        verify(mPrefService).setBoolean(Pref.TTC_CAPTIONS_ENABLED, false);
    }

    @Test
    public void testSwitchesFollowPrefsUntilDestroyed() {
        when(mPrefChangeRegistrarJni.init(any(), eq(mPrefService))).thenReturn(1L);
        TtcSettings fragment = launchFragment(mProfile);
        verify(mPrefChangeRegistrarJni).add(1L, Pref.TTC_MICROPHONE_CHIME_ENABLED);
        verify(mPrefChangeRegistrarJni).add(1L, Pref.TTC_CAPTIONS_ENABLED);

        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        fragmentManager.beginTransaction().remove(fragment).commitNow();
        verify(mPrefChangeRegistrarJni).destroy(1L);
    }

    @Test
    public void testShouldShowSettingsPageWhenTtcIsAvailable() {
        when(mService.isEnabled()).thenReturn(true);
        TtcKeyedServiceFactory.setForTesting(mService);

        assertTrue(TtcSettingsUtils.shouldShowSettingsPage(mProfile));
        assertTrue(TtcSettingsUtils.shouldShowSettingsPage(mOffTheRecordProfile));
    }

    @Test
    public void testShouldNotShowSettingsPageWhenServiceIsDisabled() {
        when(mService.isEnabled()).thenReturn(false);
        TtcKeyedServiceFactory.setForTesting(mService);

        assertFalse(TtcSettingsUtils.shouldShowSettingsPage(mProfile));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TTC)
    public void testShouldNotShowSettingsPageWhenFeatureIsDisabled() {
        when(mService.isEnabled()).thenReturn(true);
        TtcKeyedServiceFactory.setForTesting(mService);

        assertFalse(TtcSettingsUtils.shouldShowSettingsPage(mProfile));
    }

    @Test
    public void testShouldNotShowSettingsPageWithoutProfile() {
        assertFalse(TtcSettingsUtils.shouldShowSettingsPage(null));
    }
}
