// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_SYSTEM;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.support.v7.app.AppCompatDelegate;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;

/**
 * Tests for {@link CustomTabNightModeStateController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabNightModeStateControllerTest {
    @Mock
    private PowerSavingModeMonitor mPowerSavingModeMonitor;
    @Mock
    private SystemNightModeMonitor mSystemNightModeMonitor;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private AppCompatDelegate mAppCompatDelegate;
    @Captor
    private ArgumentCaptor<SystemNightModeMonitor.Observer> mSystemNightModeObserverCaptor;
    @Captor
    private ArgumentCaptor<Runnable> mPowerSavingObserverCaptor;

    private CustomTabNightModeStateController mNightModeController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doNothing().when(mSystemNightModeMonitor).addObserver(
                mSystemNightModeObserverCaptor.capture());
        doNothing().when(mPowerSavingModeMonitor).addObserver(
                mPowerSavingObserverCaptor.capture());
        mNightModeController = new CustomTabNightModeStateController(mActivityLifecycleDispatcher,
                mSystemNightModeMonitor, mPowerSavingModeMonitor);
        FeatureUtilities.setNightModeForCustomTabsAvailableForTesting(true);
    }

    @After
    public void tearDown() {
        FeatureUtilities.setNightModeForCustomTabsAvailableForTesting(false);
    }

    @Test
    public void triggersAppCompatDelegate_WhenInitialSchemeIsLight() {
        setSystemNightMode(false);
        initializeWithColorScheme(COLOR_SCHEME_SYSTEM);
        verify(mAppCompatDelegate).setLocalNightMode(AppCompatDelegate.MODE_NIGHT_NO);
    }

    @Test
    public void triggersAppCompatDelegate_WhenInitialSchemeIsDark() {
        setSystemNightMode(true);
        initializeWithColorScheme(COLOR_SCHEME_SYSTEM);
        verify(mAppCompatDelegate).setLocalNightMode(AppCompatDelegate.MODE_NIGHT_YES);
    }

    @Test
    public void nightModeIfOff_WhenSchemeForced() {
        initializeWithColorScheme(COLOR_SCHEME_LIGHT);
        assertFalse(mNightModeController.isInNightMode());
    }

    @Test
    public void nightModeIfOn_WhenSchemeForced() {
        initializeWithColorScheme(COLOR_SCHEME_DARK);
        assertTrue(mNightModeController.isInNightMode());
    }

    @Test
    public void ignoresSystemSetting_WhenSchemeForced() {
        initializeWithColorScheme(COLOR_SCHEME_LIGHT);
        setSystemNightMode(true);
        assertFalse(mNightModeController.isInNightMode());
    }

    @Test
    public void ignoresPowerSaving_WhenSchemeForced() {
        initializeWithColorScheme(COLOR_SCHEME_LIGHT);
        setPowerSavingMode(true);
        assertFalse(mNightModeController.isInNightMode());
    }

    @Test
    public void followsSystemSetting_WhenSchemeIsSystem() {
        setSystemNightMode(true);
        initializeWithColorScheme(COLOR_SCHEME_SYSTEM);
        assertTrue(mNightModeController.isInNightMode());

        setSystemNightMode(false);
        assertFalse(mNightModeController.isInNightMode());
    }

    @Test
    public void followsPowerSavingMode_WhenSchemeIsSystem() {
        setPowerSavingMode(true);
        initializeWithColorScheme(COLOR_SCHEME_SYSTEM);
        assertTrue(mNightModeController.isInNightMode());

        setPowerSavingMode(false);
        assertFalse(mNightModeController.isInNightMode());
    }

    @Test
    public void notifiesObservers_WhenNightModeChanged() {
        NightModeStateProvider.Observer observer = mock(NightModeStateProvider.Observer.class);
        initializeWithColorScheme(COLOR_SCHEME_SYSTEM);
        mNightModeController.addObserver(observer);
        setSystemNightMode(true);
        verify(observer).onNightModeStateChanged();
    }

    @Test
    public void doesntNotifyObservers_WhenNightModeDoesntChange() {
        // Extra calls to observers may lead to unnecessary activity restarts
        setSystemNightMode(true);
        NightModeStateProvider.Observer observer = mock(NightModeStateProvider.Observer.class);
        initializeWithColorScheme(COLOR_SCHEME_SYSTEM);
        mNightModeController.addObserver(observer);
        setPowerSavingMode(true);
        verify(observer, never()).onNightModeStateChanged();
    }

    private void initializeWithColorScheme(int colorScheme) {
        Intent intent = new CustomTabsIntent.Builder().setColorScheme(colorScheme).build().intent;
        mNightModeController.initialize(mAppCompatDelegate, intent);
    }

    private void setPowerSavingMode(boolean isPowerSaving) {
        when(mPowerSavingModeMonitor.powerSavingIsOn()).thenReturn(isPowerSaving);
        for (Runnable observer : mPowerSavingObserverCaptor.getAllValues()) {
            observer.run();
        }
    }

    private void setSystemNightMode(boolean isInNightMode) {
        when(mSystemNightModeMonitor.isSystemNightModeOn()).thenReturn(isInNightMode);
        for (SystemNightModeMonitor.Observer observer :
                mSystemNightModeObserverCaptor.getAllValues()) {
            observer.onSystemNightModeChanged();
        }
    }

}
