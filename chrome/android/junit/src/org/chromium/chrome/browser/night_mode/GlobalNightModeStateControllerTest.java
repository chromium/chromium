// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.ApplicationState.HAS_RUNNING_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_STOPPED_ACTIVITIES;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;

import androidx.appcompat.app.AppCompatDelegate;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.VoidAnswer1;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.night_mode.GlobalNightModeStateControllerTest.ShadowAppCompatDelegate;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link GlobalNightModeStateController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowAppCompatDelegate.class)
public class GlobalNightModeStateControllerTest {
    /**
     * Shadow implementation of {@link androidx.appcompat.app.AppCompatDelegate} that bypass
     * activity recreation. Used as a stop gap to stop test failure due to activity leaks.
     * See https://crbug.com/1347906.
     */
    @Implements(AppCompatDelegate.class)
    public static class ShadowAppCompatDelegate {
        @Implementation
        public static void setDefaultNightMode(int mode) {}
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private NightModeStateProvider.Observer mObserver;

    private GlobalNightModeStateController mGlobalNightModeStateController;

    @Mock private SystemNightModeMonitor mSystemNightModeMonitor;

    private SystemNightModeMonitor.Observer mSystemNightModeObserver;

    @Before
    public void setUp() {
        captureObservers();

        SystemNightModeMonitor.setInstanceForTesting(mSystemNightModeMonitor);
        mGlobalNightModeStateController = new GlobalNightModeStateController();

        mGlobalNightModeStateController.onApplicationStateChange(HAS_RUNNING_ACTIVITIES);

        // Night mode is disabled by default.
        assertFalse(GlobalNightModeStateProviderHolder.getInstance().isInNightMode());
    }

    private void captureObservers() {
        // We need to mock removeObserver as well as addObserver, so can't use ArgumentCaptor.
        doAnswer(
                        answerVoid(
                                (VoidAnswer1<SystemNightModeMonitor.Observer>)
                                        observer -> mSystemNightModeObserver = observer))
                .when(mSystemNightModeMonitor)
                .addObserver(any());
        doAnswer(
                        answerVoid(
                                (VoidAnswer1<SystemNightModeMonitor.Observer>)
                                        observer -> mSystemNightModeObserver = null))
                .when(mSystemNightModeMonitor)
                .removeObserver(any());
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(UI_THEME_SETTING);
    }

    @Test
    public void testUpdateNightMode_SystemNightMode_DefaultsToSystem() {
        // Set preference to system default and verify that the night mode isn't enabled.
        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.SYSTEM_DEFAULT);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        // Enable system night mode and verify night mode is enabled.
        setSystemNightMode(true);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        // Disable system night mode and verify night mode is disabled.
        setSystemNightMode(false);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
    }

    @Test
    public void testUpdateNightMode_Preference() {
        // Set preference to dark theme and verify night mode is enabled.
        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.DARK);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        // Set preference to light theme and verify night mode is disabled.
        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.LIGHT);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        // Regardless of system night mode, night mode is disabled with light theme preference.
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        setSystemNightMode(true);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
    }

    @Test
    public void testStopAndRestart() {
        // Simulate to stop listening to night mode state changes. Verify that night mode state is
        // not changed.
        mGlobalNightModeStateController.onApplicationStateChange(HAS_STOPPED_ACTIVITIES);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        setSystemNightMode(true);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.DARK);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        // Simulate to start listening to night mode state changes. Verify that
        //   1. Night mode state is updated after #start().
        //   2. Night mode state is updated on system night mode or preference changes.
        mGlobalNightModeStateController.onApplicationStateChange(HAS_RUNNING_ACTIVITIES);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.SYSTEM_DEFAULT);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        setSystemNightMode(false);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
    }

    @Test
    public void testObserver() {
        mGlobalNightModeStateController.addObserver(mObserver);

        // Verify that observer is called on night mode state changed from false to true.
        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.DARK);
        assertTrue(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(1)).onNightModeStateChanged();

        // Verify that observer is called when set to light theme.
        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.LIGHT);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(2)).onNightModeStateChanged();

        // Verify that observer is not called after it is removed.
        mGlobalNightModeStateController.removeObserver(mObserver);
        ChromeSharedPreferences.getInstance().writeInt(UI_THEME_SETTING, ThemeType.DARK);
        assertTrue(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(2)).onNightModeStateChanged();
    }

    /**
     * Simulates setting system night mode, and notifies the change.
     *
     * @param isSystemNightModeOn Whether system night mode is enabled or not.
     */
    private void setSystemNightMode(boolean isSystemNightModeOn) {
        when(mSystemNightModeMonitor.isSystemNightModeOn()).thenReturn(isSystemNightModeOn);
        if (mSystemNightModeObserver != null) {
            mSystemNightModeObserver.onSystemNightModeChanged();
        }
    }
}
