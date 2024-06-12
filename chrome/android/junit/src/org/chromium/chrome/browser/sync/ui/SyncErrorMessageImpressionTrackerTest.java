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
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;

/** Unit tests for SyncErrorMessageImpressionTracker. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SyncErrorMessageImpressionTrackerTest {
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private PrefService mPrefService;

    private SharedPreferencesManager mSharedPrefsManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSharedPrefsManager = ChromeSharedPreferences.getInstance();
    }

    @After
    public void tearDown() {
        mSharedPrefsManager.removeKey(ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME);
        mFakeTimeTestRule.resetTimes();
    }

    @Test
    public void testNotEnoughTimeSinceLastSyncErrorUI() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);
        // Make sure not enough time passed since the time of the first sync error message.
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS);

        // Pretend enough time has passed since the last password manager message.
        final long timeOfPwmMessage =
                TimeUtils.currentTimeMillis()
                        - SyncErrorMessageImpressionTracker.MINIMAL_DURATION_TO_PWM_ERROR_UI_MS
                        - 1;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfPwmMessage));

        assertFalse(SyncErrorMessageImpressionTracker.canShowNow(mPrefService));
    }

    @Test
    public void testNotEnoughTimeSinceLastPwmUI() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);
        // Make sure enough time passed since the last sync error UI.
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS + 1);

        // Pretend not enough time passed since last password manager UI.
        final long timeOfPwmMessage =
                TimeUtils.currentTimeMillis()
                        - SyncErrorMessageImpressionTracker.MINIMAL_DURATION_TO_PWM_ERROR_UI_MS;
        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfPwmMessage));

        assertFalse(SyncErrorMessageImpressionTracker.canShowNow(mPrefService));
    }

    @Test
    public void testEnoughTimeSinceBothUis() {
        final long timeOfFirstSyncMessage = TimeUtils.currentTimeMillis();
        mSharedPrefsManager.writeLong(
                ChromePreferenceKeys.SYNC_ERROR_MESSAGE_SHOWN_AT_TIME, timeOfFirstSyncMessage);

        // Make sure enough time passed since the last sync error UI.
        mFakeTimeTestRule.advanceMillis(
                SyncErrorMessageImpressionTracker.MINIMAL_DURATION_BETWEEN_UI_MS + 1);

        // Pretend enough time passed since the last password manager error UI.
        final long timeOfPwmMessage =
                TimeUtils.currentTimeMillis()
                        - SyncErrorMessageImpressionTracker.MINIMAL_DURATION_TO_PWM_ERROR_UI_MS
                        - 1;

        when(mPrefService.getString(Pref.UPM_ERROR_UI_SHOWN_TIMESTAMP))
                .thenReturn(Long.toString(timeOfPwmMessage));

        assertTrue(SyncErrorMessageImpressionTracker.canShowNow(mPrefService));
    }
}
