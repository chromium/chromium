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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Unit tests for {@link AppThemeColorProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppThemeColorProviderUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TintObserver mTintObserver;

    private AppThemeColorProvider mAppThemeColorProvider;
    private Context mContext;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
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
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(true);
        when(mActivityLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(4); // > 3 (unfocused)
        initThemeColorProvider();

        // Simulate incognito state change that updates tint at startup.
        mAppThemeColorProvider.onIncognitoStateChanged(false);

        // Verify.
        var brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        var tint = ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme);
        var unfocusedActivityTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, brandedColorScheme, false);

        assertEquals(
                "Default tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getTint().toString());
        assertEquals(
                "Activity focus tint is not correct.",
                unfocusedActivityTint.toString(),
                mAppThemeColorProvider.getActivityFocusTint().toString());
        verify(mTintObserver).onTintChanged(tint, unfocusedActivityTint, brandedColorScheme);

        // Assume that the activity gained focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(true);

        // Verify.
        assertEquals(
                "Default tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getTint().toString());
        assertEquals(
                "Activity focus tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getActivityFocusTint().toString());
        verify(mTintObserver).onTintChanged(tint, tint, brandedColorScheme);
    }

    @Test
    public void topResumedActivityChanged_NotInDesktopWindow() {
        // Initialize.
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        when(mActivityLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(3); // <= 3 (focused)
        initThemeColorProvider();

        // Assume that the activity lost focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(false);

        // Verify.
        var brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        var tint = ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme);

        assertEquals(
                "Default tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getTint().toString());
        assertEquals(
                "Activity focus tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getActivityFocusTint().toString());

        // Assume that the activity gained focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(true);

        // Verify.
        assertEquals(
                "Default tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getTint().toString());
        assertEquals(
                "Activity focus tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getActivityFocusTint().toString());

        verify(mTintObserver).onTintChanged(tint, tint, brandedColorScheme);
    }

    @Test
    public void topResumedActivityChanged_InDesktopWindow() {
        // Initialize.
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(true);
        when(mActivityLifecycleDispatcher.getCurrentActivityState())
                .thenReturn(3); // <= 3 (focused)
        initThemeColorProvider();

        // Assume that the activity lost focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(false);

        // Verify.
        var brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        var tint = ThemeUtils.getThemedToolbarIconTint(mContext, brandedColorScheme);
        var unfocusedActivityTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, brandedColorScheme, false);

        assertEquals(
                "Default tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getTint().toString());
        assertEquals(
                "Activity focus tint is not correct.",
                unfocusedActivityTint.toString(),
                mAppThemeColorProvider.getActivityFocusTint().toString());
        verify(mTintObserver).onTintChanged(tint, unfocusedActivityTint, brandedColorScheme);

        // Assume that the activity gained focus.
        mAppThemeColorProvider.onTopResumedActivityChanged(true);

        // Verify.
        assertEquals(
                "Default tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getTint().toString());
        assertEquals(
                "Activity focus tint is not correct.",
                tint.toString(),
                mAppThemeColorProvider.getActivityFocusTint().toString());
        verify(mTintObserver).onTintChanged(tint, tint, brandedColorScheme);
    }

    private void initThemeColorProvider() {
        mAppThemeColorProvider =
                new AppThemeColorProvider(
                        mContext, mActivityLifecycleDispatcher, mMultiWindowModeStateDispatcher);
        mAppThemeColorProvider.addTintObserver(mTintObserver);

        verify(mMultiWindowModeStateDispatcher).addObserver(mAppThemeColorProvider);
        verify(mActivityLifecycleDispatcher).register(mAppThemeColorProvider);
        assertNull(
                "Activity focus tint should not be set on instantiation.",
                mAppThemeColorProvider.getActivityFocusTint());
    }
}
