// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.ContextUtils;
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
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
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
    @Mock private SettingsIndexData mSettingsIndexData;
    @Mock private TtcShortcutUtils.Natives mShortcutUtilsJni;

    private TestActivity mActivity;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarJni);
        TtcShortcutUtilsJni.setInstanceForTesting(mShortcutUtilsJni);
        when(mShortcutUtilsJni.getKeyLabels(anyString())).thenReturn(new String[] {"Key"});
        when(mPrefService.getString(anyString())).thenReturn("");
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

    /** Binds {@code preference} to a new view, as the settings list does, and returns the view. */
    private static View bindView(Preference preference) {
        LayoutInflater inflater = LayoutInflater.from(preference.getContext());
        View view = inflater.inflate(preference.getLayoutResource(), null);
        inflater.inflate(
                preference.getWidgetLayoutResource(), view.findViewById(android.R.id.widget_frame));
        preference.onBindViewHolder(PreferenceViewHolder.createInstanceForTests(view));
        return view;
    }

    private void setHardKeyboardConnected(boolean connected) {
        int keyboard = connected ? Configuration.KEYBOARD_QWERTY : Configuration.KEYBOARD_NOKEYS;
        mActivity.getResources().getConfiguration().keyboard = keyboard;
        ContextUtils.getApplicationContext().getResources().getConfiguration().keyboard = keyboard;
    }

    @Test
    public void testShortcutsHiddenWithoutKeyboard() {
        setHardKeyboardConnected(false);
        TtcSettings fragment = launchFragment(mProfile);

        assertFalse(fragment.findPreference(TtcSettings.PREF_KEYBOARD_SHORTCUT).isVisible());
        assertFalse(fragment.findPreference(TtcSettings.PREF_HOLD_TO_SPEAK_SHORTCUT).isVisible());
    }

    @Test
    public void testShortcutsShowSavedShortcuts() {
        setHardKeyboardConnected(true);
        when(mPrefService.getString(Pref.TTC_KEYBOARD_SHORTCUT)).thenReturn("Alt+Shift+K");
        when(mPrefService.getString(Pref.TTC_HOLD_TO_SPEAK_SHORTCUT)).thenReturn("");
        TtcSettings fragment = launchFragment(mProfile);

        TtcShortcutPreference keyboardShortcut =
                fragment.findPreference(TtcSettings.PREF_KEYBOARD_SHORTCUT);
        assertTrue(keyboardShortcut.isVisible());
        assertEquals("Alt+Shift+K", keyboardShortcut.getShortcut());

        // A cleared shortcut isn't set.
        TtcShortcutPreference holdToSpeakShortcut =
                fragment.findPreference(TtcSettings.PREF_HOLD_TO_SPEAK_SHORTCUT);
        assertTrue(holdToSpeakShortcut.isVisible());
        assertEquals("", holdToSpeakShortcut.getShortcut());
    }

    @Test
    public void testClearShortcutSavesEmptyValue() {
        setHardKeyboardConnected(true);
        when(mPrefService.getString(Pref.TTC_KEYBOARD_SHORTCUT)).thenReturn("Alt+Shift+K");
        TtcShortcutPreference keyboardShortcut =
                launchFragment(mProfile).findPreference(TtcSettings.PREF_KEYBOARD_SHORTCUT);

        when(mPrefService.getString(Pref.TTC_KEYBOARD_SHORTCUT)).thenReturn("");
        keyboardShortcut.clear();

        verify(mPrefService).setString(Pref.TTC_KEYBOARD_SHORTCUT, "");
        assertEquals("", keyboardShortcut.getShortcut());
    }

    @Test
    public void testShortcutRowShowsKeycaps() {
        setHardKeyboardConnected(true);
        when(mPrefService.getString(Pref.TTC_KEYBOARD_SHORTCUT)).thenReturn("Alt+Shift+K");
        when(mShortcutUtilsJni.getKeyLabels("Alt+Shift+K"))
                .thenReturn(new String[] {"Alt", "Shift", "K"});
        TtcShortcutPreference keyboardShortcut =
                launchFragment(mProfile).findPreference(TtcSettings.PREF_KEYBOARD_SHORTCUT);
        assertFalse(keyboardShortcut.isSelectable());

        View view = bindView(keyboardShortcut);
        LinearLayout keycaps = view.findViewById(R.id.ttc_shortcut_keycaps);
        assertEquals(View.VISIBLE, keycaps.getVisibility());
        assertEquals(3, keycaps.getChildCount());
        assertEquals("Alt", ((TextView) keycaps.getChildAt(0)).getText().toString());
        assertEquals("Shift", ((TextView) keycaps.getChildAt(1)).getText().toString());
        assertEquals("K", ((TextView) keycaps.getChildAt(2)).getText().toString());
        assertEquals(View.GONE, view.findViewById(R.id.ttc_shortcut_not_set).getVisibility());

        // The keycaps are spaced apart, with no space before the first or after the last.
        MarginLayoutParams first = (MarginLayoutParams) keycaps.getChildAt(0).getLayoutParams();
        MarginLayoutParams second = (MarginLayoutParams) keycaps.getChildAt(1).getLayoutParams();
        MarginLayoutParams last = (MarginLayoutParams) keycaps.getChildAt(2).getLayoutParams();
        assertEquals(0, first.getMarginStart());
        assertTrue(second.getMarginStart() > 0);
        assertEquals(0, last.getMarginEnd());

        // The clear button names its row, and clears the shortcut.
        View clear = view.findViewById(R.id.ttc_shortcut_clear);
        assertEquals(View.VISIBLE, clear.getVisibility());
        assertEquals(
                mActivity.getString(
                        R.string.ttc_settings_shortcut_clear,
                        mActivity.getString(R.string.ttc_settings_keyboard_shortcut)),
                clear.getContentDescription().toString());

        when(mPrefService.getString(Pref.TTC_KEYBOARD_SHORTCUT)).thenReturn("");
        clear.performClick();
        verify(mPrefService).setString(Pref.TTC_KEYBOARD_SHORTCUT, "");
        view = bindView(keyboardShortcut);
        assertEquals(View.GONE, view.findViewById(R.id.ttc_shortcut_keycaps).getVisibility());
        assertEquals(View.VISIBLE, view.findViewById(R.id.ttc_shortcut_not_set).getVisibility());
        assertEquals(View.GONE, view.findViewById(R.id.ttc_shortcut_clear).getVisibility());
    }

    @Test
    public void testShortcutRowWithoutKeysShowsNotSet() {
        setHardKeyboardConnected(true);
        // A value saved on another platform names no keys on Android.
        when(mPrefService.getString(Pref.TTC_KEYBOARD_SHORTCUT)).thenReturn("Command+K");
        when(mShortcutUtilsJni.getKeyLabels("Command+K")).thenReturn(new String[0]);
        when(mPrefService.getString(Pref.TTC_HOLD_TO_SPEAK_SHORTCUT)).thenReturn("");
        TtcSettings fragment = launchFragment(mProfile);

        for (String key :
                new String[] {
                    TtcSettings.PREF_KEYBOARD_SHORTCUT, TtcSettings.PREF_HOLD_TO_SPEAK_SHORTCUT
                }) {
            View view = bindView(fragment.findPreference(key));
            LinearLayout keycaps = view.findViewById(R.id.ttc_shortcut_keycaps);
            assertEquals(key, View.GONE, keycaps.getVisibility());
            assertEquals(key, 0, keycaps.getChildCount());
            assertEquals(
                    key,
                    View.VISIBLE,
                    view.findViewById(R.id.ttc_shortcut_not_set).getVisibility());
            assertEquals(
                    key, View.GONE, view.findViewById(R.id.ttc_shortcut_clear).getVisibility());
        }
    }

    @Test
    public void testShortcutsFollowKeyboardConnection() {
        setHardKeyboardConnected(false);
        when(mPrefService.getString(Pref.TTC_KEYBOARD_SHORTCUT)).thenReturn("Alt+Shift+K");
        TtcSettings fragment = launchFragment(mProfile);
        TtcShortcutPreference keyboardShortcut =
                fragment.findPreference(TtcSettings.PREF_KEYBOARD_SHORTCUT);
        TtcShortcutPreference holdToSpeakShortcut =
                fragment.findPreference(TtcSettings.PREF_HOLD_TO_SPEAK_SHORTCUT);
        assertFalse(keyboardShortcut.isVisible());
        assertFalse(holdToSpeakShortcut.isVisible());

        // Connecting a keyboard doesn't recreate the page.
        setHardKeyboardConnected(true);
        fragment.onConfigurationChanged(mActivity.getResources().getConfiguration());
        assertTrue(keyboardShortcut.isVisible());
        assertTrue(holdToSpeakShortcut.isVisible());
        assertEquals("Alt+Shift+K", keyboardShortcut.getShortcut());

        setHardKeyboardConnected(false);
        fragment.onConfigurationChanged(mActivity.getResources().getConfiguration());
        assertFalse(keyboardShortcut.isVisible());
        assertFalse(holdToSpeakShortcut.isVisible());
    }

    @Test
    public void testShortcutsFollowPrefsWithoutKeyboard() {
        // The rows follow their prefs even while hidden, for when a keyboard is connected.
        setHardKeyboardConnected(false);
        when(mPrefChangeRegistrarJni.init(any(), eq(mPrefService))).thenReturn(1L);
        launchFragment(mProfile);

        verify(mPrefChangeRegistrarJni).add(1L, Pref.TTC_KEYBOARD_SHORTCUT);
        verify(mPrefChangeRegistrarJni).add(1L, Pref.TTC_HOLD_TO_SPEAK_SHORTCUT);
    }

    @Test
    public void testSearchIndexKeepsShortcutsWithKeyboard() {
        setHardKeyboardConnected(true);
        TtcSettings.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                RuntimeEnvironment.getApplication(), mSettingsIndexData, mProfile);

        verifyNoInteractions(mSettingsIndexData);
    }

    @Test
    public void testSearchIndexOmitsShortcutsWithoutKeyboard() {
        setHardKeyboardConnected(false);
        TtcSettings.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                RuntimeEnvironment.getApplication(), mSettingsIndexData, mProfile);

        verify(mSettingsIndexData)
                .removeEntryForKey(TtcSettings.class.getName(), TtcSettings.PREF_KEYBOARD_SHORTCUT);
        verify(mSettingsIndexData)
                .removeEntryForKey(
                        TtcSettings.class.getName(), TtcSettings.PREF_HOLD_TO_SPEAK_SHORTCUT);
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
