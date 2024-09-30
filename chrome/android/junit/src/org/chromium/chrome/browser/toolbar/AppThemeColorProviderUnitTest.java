// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;

/** Unit tests for {@link AppThemeColorProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppThemeColorProviderUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private DesktopWindowStateProvider mDesktopWindowStateProvider;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private TintObserver mTintObserver;

    private AppThemeColorProvider mAppThemeColorProvider;
    private Context mContext;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        when(mDesktopWindowStateProvider.getAppHeaderState()).thenReturn(mAppHeaderState);
        when(mDesktopWindowStateProvider.isInUnfocusedDesktopWindow()).thenReturn(false);
    }

    @After
    public void teardown() {
        if (mAppThemeColorProvider != null) {
            mAppThemeColorProvider.removeTintObserver(mTintObserver);
        }
    }

    @Test
    public void appStartsInUnfocusedDesktopWindow() {
        // Initialize.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);
        when(mDesktopWindowStateProvider.isInUnfocusedDesktopWindow()).thenReturn(true);
        initThemeColorProvider();

        // Simulate incognito state change that updates tint at startup.
        mAppThemeColorProvider.onIncognitoStateChanged(false);

        // Verify.
        var brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        var tint = ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme);
        var unfocusedActivityTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, brandedColorScheme, false);

        assertEquals("Default tint is not correct.", tint, mAppThemeColorProvider.getTint());
        assertEquals(
                "Activity focus tint is not correct.",
                unfocusedActivityTint,
                mAppThemeColorProvider.getActivityFocusTint());
        verify(mTintObserver).onTintChanged(tint, unfocusedActivityTint, brandedColorScheme);
    }

    @Test
    public void topResumedActivityChanged_NotInDesktopWindow() {
        // Initialize.
        initThemeColorProvider();
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);

        // Assume that the activity lost focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(false);

        // Verify.
        var brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        var tint = ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme);

        assertEquals("Default tint is not correct.", tint, mAppThemeColorProvider.getTint());
        assertEquals(
                "Activity focus tint is not correct.",
                tint,
                mAppThemeColorProvider.getActivityFocusTint());

        // Assume that the activity gained focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(true);

        // Verify.
        assertEquals("Default tint is not correct.", tint, mAppThemeColorProvider.getTint());
        assertEquals(
                "Activity focus tint is not correct.",
                tint,
                mAppThemeColorProvider.getActivityFocusTint());

        verify(mTintObserver).onTintChanged(tint, tint, brandedColorScheme);
    }

    @Test
    public void topResumedActivityChanged_InDesktopWindow() {
        // Initialize.
        initThemeColorProvider();
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Assume that the activity lost focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(false);

        // Verify.
        var brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        var tint = ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme);
        var unfocusedActivityTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, brandedColorScheme, false);

        assertEquals("Default tint is not correct.", tint, mAppThemeColorProvider.getTint());
        assertEquals(
                "Activity focus tint is not correct.",
                unfocusedActivityTint,
                mAppThemeColorProvider.getActivityFocusTint());
        verify(mTintObserver).onTintChanged(tint, unfocusedActivityTint, brandedColorScheme);

        // Assume that the activity gained focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(true);

        // Verify.
        assertEquals("Default tint is not correct.", tint, mAppThemeColorProvider.getTint());
        assertEquals(
                "Activity focus tint is not correct.",
                tint,
                mAppThemeColorProvider.getActivityFocusTint());
        verify(mTintObserver).onTintChanged(tint, tint, brandedColorScheme);
    }

    private void initThemeColorProvider() {
        mAppThemeColorProvider =
                new AppThemeColorProvider(
                        mContext, mActivityLifecycleDispatcher, mDesktopWindowStateProvider);
        mAppThemeColorProvider.addTintObserver(mTintObserver);

        verify(mActivityLifecycleDispatcher).register(mAppThemeColorProvider);
        assertNull(
                "Activity focus tint should not be set on instantiation.",
                mAppThemeColorProvider.getActivityFocusTint());
    }
}
