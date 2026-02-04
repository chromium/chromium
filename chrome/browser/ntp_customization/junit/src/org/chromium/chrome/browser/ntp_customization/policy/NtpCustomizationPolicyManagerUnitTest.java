// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.policy;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Unit tests for {@link NtpCustomizationPolicyManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationPolicyManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private Profile mOriginalProfile;
    @Mock private PrefService mPrefService;
    @Mock private PrefChangeRegistrar mPrefChangeRegistrar;
    @Captor private ArgumentCaptor<PrefChangeRegistrar.PrefObserver> mPrefObserverCaptor;

    private NtpCustomizationPolicyManager mManager;

    @Before
    public void setUp() {
        when(mProfile.getOriginalProfile()).thenReturn(mOriginalProfile);
        UserPrefs.setPrefServiceForTesting(mPrefService);
        NtpCustomizationPolicyManager.setPrefChangeRegistrarForTesting(mPrefChangeRegistrar);

        mManager = new NtpCustomizationPolicyManager();
        mManager.onFinishNativeInitialization(mProfile);
    }

    @After
    public void tearDown() {
        mManager.resetSharedPreferenceForTesting();
    }

    @Test
    public void testIsNtpCustomBackgroundEnabled_Default() {
        assertTrue(mManager.isNtpCustomBackgroundEnabled());
    }

    @Test
    public void testConstructor_FromSharedPreferences() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED, false);
        NtpCustomizationPolicyManager manager = new NtpCustomizationPolicyManager();
        assertFalse(manager.isNtpCustomBackgroundEnabled());
    }

    @Test
    public void testOnPreferenceChange() {
        verify(mPrefChangeRegistrar)
                .addObserver(eq(Pref.NTP_CUSTOM_BACKGROUND_DICT), mPrefObserverCaptor.capture());
        var sharedPreferencesManager = ChromeSharedPreferences.getInstance();

        // Initial values.
        assertTrue(mManager.isNtpCustomBackgroundEnabled());
        assertTrue(
                sharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED, true));

        // Disable NtpCustomBackgroundEnabled policy.
        when(mPrefService.isManagedPreference(Pref.NTP_CUSTOM_BACKGROUND_DICT)).thenReturn(true);
        mPrefObserverCaptor.getValue().onPreferenceChange();

        // Verifies that the isNtpCustomBackgroundEnabled() returns the same return value while the
        // shared preference is updated.
        assertTrue(mManager.isNtpCustomBackgroundEnabled());
        assertFalse(
                sharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED, true));

        // Enable NtpCustomBackgroundEnabled policy.
        when(mPrefService.isManagedPreference(Pref.NTP_CUSTOM_BACKGROUND_DICT)).thenReturn(false);
        mPrefObserverCaptor.getValue().onPreferenceChange();

        assertTrue(mManager.isNtpCustomBackgroundEnabled());
        assertTrue(
                sharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED, false));
    }

    @Test
    public void testGetNtpCustomBackgroundEnabledPolicyValue() {
        when(mPrefService.isManagedPreference(Pref.NTP_CUSTOM_BACKGROUND_DICT)).thenReturn(true);
        assertFalse(mManager.getNtpCustomBackgroundEnabledPolicyValue());

        when(mPrefService.isManagedPreference(Pref.NTP_CUSTOM_BACKGROUND_DICT)).thenReturn(false);
        assertTrue(mManager.getNtpCustomBackgroundEnabledPolicyValue());
    }

    @Test
    public void testOnDeferredStartup_Disabled() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED, false);
        NtpCustomizationPolicyManager manager = new NtpCustomizationPolicyManager();
        assertFalse(manager.isNtpCustomBackgroundEnabled());

        // Test case for background image type.
        var sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE,
                NtpBackgroundType.IMAGE_FROM_DISK);
        sharedPreferencesManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR, Color.BLUE);

        manager.onDeferredStartup();
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR));

        // Test case for background image type.
        sharedPreferencesManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE,
                NtpBackgroundType.CHROME_COLOR);
        sharedPreferencesManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID,
                NtpThemeColorId.NTP_COLORS_BLUE);

        manager.onDeferredStartup();
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID));
    }

    @Test
    public void testOnDeferredStartup_Enabled() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED, true);
        NtpCustomizationPolicyManager manager = new NtpCustomizationPolicyManager();
        assertTrue(manager.isNtpCustomBackgroundEnabled());

        // Set some keys that should NOT be cleared.
        var sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE,
                NtpBackgroundType.IMAGE_FROM_DISK);

        manager.onDeferredStartup();
        assertEquals(
                NtpBackgroundType.IMAGE_FROM_DISK,
                sharedPreferencesManager.readInt(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE,
                        NtpBackgroundType.DEFAULT));
    }

    @Test
    public void testDestroy() {
        verify(mPrefChangeRegistrar)
                .addObserver(eq(Pref.NTP_CUSTOM_BACKGROUND_DICT), mPrefObserverCaptor.capture());
        mManager.destroy();

        verify(mPrefChangeRegistrar).removeObserver(eq(Pref.NTP_CUSTOM_BACKGROUND_DICT));
    }
}
