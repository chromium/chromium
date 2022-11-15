// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/**
 * Unit tests for SyncErrorMessageImpressionTracker.
 * */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SyncErrorMessageImpressionTrackerTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

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
        mSharedPrefsManager.removeKey(ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME);
        mFakeTimeTestRule.resetTimes();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)
    public void testNotEnoughTimeSinceLastSyncErrorUINoPwm() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS);
        assertFalse(SyncErrorMessageImpressionTracker.canShowNow());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)
    public void testEnoughTimeSinceLastSyncErrorUINoPwm() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS + 1);
        assertTrue(SyncErrorMessageImpressionTracker.canShowNow());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)
    public void testNotEnoughTimeSinceLastSyncErrorUI() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);
        // Make sure not enough time passed since the time of the first sync error message.
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS);

        // Pretend enough time has passed since the last password manager message.
        final long timeOfPwmMessage = TimeUtils.currentTimeMillis()
                - SyncErrorMessageImpressionTracker.MINIMAL_DURATION_TO_PWM_ERROR_UI_MS - 1;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfPwmMessage));

        assertFalse(SyncErrorMessageImpressionTracker.canShowNow());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)
    public void testNotEnoughTimeSinceLastPwmUI() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);
        // Make sure enough time passed since the last sync error UI.
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS + 1);

        // Pretend not enough time passed since last password manager UI.
        final long timeOfPwmMessage = TimeUtils.currentTimeMillis()
                - SyncErrorMessageImpressionTracker.MINIMAL_DURATION_TO_PWM_ERROR_UI_MS;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfPwmMessage));

        assertFalse(SyncErrorMessageImpressionTracker.canShowNow());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ERROR_MESSAGES)
    public void testEnoughTimeSinceBothUis() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);

        // Make sure enough time passed since the last sync error UI.
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS + 1);

        // Pretend enough time passed since the last password manager error UI.
        final long timeOfPwmMessage = TimeUtils.currentTimeMillis()
                - SyncErrorMessageImpressionTracker.MINIMAL_DURATION_TO_PWM_ERROR_UI_MS - 1;

        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfPwmMessage));

        assertTrue(SyncErrorMessageImpressionTracker.canShowNow());
    }
}
