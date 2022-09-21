// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/**
 * Unit tests for the error message helper bridge.
 * */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordManagerErrorMessageHelperBridgeTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock
    private Profile mProfile;

    @Mock
    private PrefService mPrefService;

    @Mock
    private UserPrefs.Natives mUserPrefsJniMock;

    private SharedPreferencesManager mSharedPrefsManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfile);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        mSharedPrefsManager = SharedPreferencesManager.getInstance();
    }

    @After
    public void tearDown() {
        mSharedPrefsManager.removeKey(ChromePreferenceKeys.SYNC_ERROR_PROMPT_SHOWN_AT_TIME);
        mFakeTimeTestRule.resetTimes();
    }

    @Test
    public void testNotEnoughTimeSinceLastUI() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        final long timeOfSyncPrompt = timeOfFirstUpmPrompt
                + PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_PROMPT_SHOWN_AT_TIME, timeOfSyncPrompt);
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS);
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowErrorUi());
    }

    @Test
    public void testNotEnoughTimeSinceLastSyncUI() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS + 1);
        final long timeOfSyncPrompt = TimeUtils.currentTimeMillis()
                - PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_PROMPT_SHOWN_AT_TIME, timeOfSyncPrompt);
        assertFalse(PasswordManagerErrorMessageHelperBridge.shouldShowErrorUi());
    }

    @Test
    public void testEnoughTimeSinceBothUis() {
        final long timeOfFirstUpmPrompt = TimeUtils.currentTimeMillis();
        final long timeOfSyncPrompt = timeOfFirstUpmPrompt
                + PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_TO_SYNC_ERROR_MS;

        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfFirstUpmPrompt));
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_PROMPT_SHOWN_AT_TIME, timeOfSyncPrompt);
        mFakeTimeTestRule.advanceMillis(
                PasswordManagerErrorMessageHelperBridge.MINIMAL_INTERVAL_BETWEEN_PROMPTS_MS + 1);
        assertTrue(PasswordManagerErrorMessageHelperBridge.shouldShowErrorUi());
    }

    @Test
    public void testSaveErrorUIShownTimestamp() {
        final long currentTimeMs = TimeUtils.currentTimeMillis();
        final long timeIncrementMs = 30;
        mFakeTimeTestRule.advanceMillis(timeIncrementMs);
        PasswordManagerErrorMessageHelperBridge.saveErrorUiShownTimestamp();
        verify(mPrefService)
                .setString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP,
                        Long.toString(currentTimeMs + timeIncrementMs));
    }
}
