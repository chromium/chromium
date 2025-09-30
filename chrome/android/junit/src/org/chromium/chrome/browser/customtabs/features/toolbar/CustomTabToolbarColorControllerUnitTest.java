// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class CustomTabToolbarColorControllerUnitTest {
    private static final int SCREEN_WIDTH = 800;
    private static final int SCREEN_HEIGHT = 1600;
    private static final int SYS_APP_HEADER_HEIGHT = 40;
    private static final int LEFT_INSET = 50;
    private static final int RIGHT_INSET = 60;
    private static final Rect WINDOW_RECT = new Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_INSET, 0, SCREEN_WIDTH - RIGHT_INSET, SYS_APP_HEADER_HEIGHT);
    private static final int THEME_COLOR = Color.GREEN;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock public DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock public BrowserServicesThemeColorProvider mBrowserServicesThemeColorProvider;
    @Mock public ToolbarManager mToolbarManager;
    @Mock public ColorStateList mThemeColorStateList;
    @Mock public ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private Context mContext;
    private AppHeaderState mAppHeaderState;

    CustomTabToolbarColorController mColorController;

    @Before
    public void setup() {
        setupThemeProvider(
                THEME_COLOR, mThemeColorStateList, BrandedColorScheme.DARK_BRANDED_THEME);

        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    private CustomTabToolbarColorController createController() {
        return new CustomTabToolbarColorController(
                mContext,
                mBrowserServicesThemeColorProvider,
                mDesktopWindowStateManager,
                mIntentDataProvider,
                mActivityLifecycleDispatcher);
    }

    private Intent buildCustomTabIntent() {
        CustomTabsSession session =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(mContext, ChromeLauncherActivity.class));
        return new CustomTabsIntent.Builder(session).build().intent;
    }

    private Intent buildTwaIntent() {
        var intent = buildCustomTabIntent();
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }

    private CustomTabIntentDataProvider buildCustomTabIntentProvider(Intent intent) {
        return new CustomTabIntentDataProvider(intent, mContext, COLOR_SCHEME_LIGHT);
    }

    private void setupCustomTab() {
        var intent = buildCustomTabIntent();
        mIntentDataProvider = buildCustomTabIntentProvider(intent);
    }

    private void setupMinimalUi() {
        var intent = buildTwaIntent();
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.MinimalUiMode().toBundle());
        mIntentDataProvider = buildCustomTabIntentProvider(intent);
    }

    private void setupStandalone() {
        var intent = buildTwaIntent();
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_DISPLAY_MODE,
                new TrustedWebActivityDisplayMode.DefaultMode().toBundle());
        mIntentDataProvider = buildCustomTabIntentProvider(intent);
    }

    private void setupDesktopWindowing(boolean isInDesktopWindow) {
        mAppHeaderState =
                new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, isInDesktopWindow);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);
    }

    private void setupThemeProvider(
            @ColorInt int color, ColorStateList tint, @BrandedColorScheme int scheme) {
        when(mBrowserServicesThemeColorProvider.getThemeColor()).thenReturn(color);
        when(mBrowserServicesThemeColorProvider.getTint()).thenReturn(tint);
        when(mBrowserServicesThemeColorProvider.getActivityFocusTint()).thenReturn(tint);
        when(mBrowserServicesThemeColorProvider.getBrandedColorScheme()).thenReturn(scheme);
    }

    @Test
    public void testInitToolbarManagerNotWebApp_SetThemeFromProvider() {
        setupCustomTab();
        mColorController = createController();
        mColorController.onToolbarInitialized(mToolbarManager);

        ColorStateList expectedFocusTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME, true);

        verify(mToolbarManager).onThemeColorChanged(THEME_COLOR, false);
        verify(mToolbarManager)
                .onTintChanged(
                        mThemeColorStateList,
                        expectedFocusTint,
                        BrandedColorScheme.DARK_BRANDED_THEME);
    }

    @Test
    public void testThemeChange_UpdateToolbarWithNewTheme() {
        // prepare CCT in fullscreen mode
        setupDesktopWindowing(/* isInDesktopWindow= */ false);
        setupCustomTab();
        mColorController = createController();
        mColorController.onToolbarInitialized(mToolbarManager);

        // update provider with new theme
        ColorStateList newTint = mock(ColorStateList.class);
        setupThemeProvider(Color.BLUE, newTint, BrandedColorScheme.LIGHT_BRANDED_THEME);

        // check new color is set
        mColorController.onThemeColorChanged(Color.BLUE, false);
        verify(mToolbarManager).onThemeColorChanged(Color.BLUE, false);

        ColorStateList expectedFocusTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, BrandedColorScheme.LIGHT_BRANDED_THEME, true);

        // check tint is updated
        mColorController.onTintChanged(
                newTint, expectedFocusTint, BrandedColorScheme.LIGHT_BRANDED_THEME);
        verify(mToolbarManager)
                .onTintChanged(newTint, expectedFocusTint, BrandedColorScheme.LIGHT_BRANDED_THEME);
    }

    @Test
    public void testDesktopWindowingModeStandalone_SetThemeFromProvider() {
        // prepare a web app in standalone mode
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupStandalone();
        mColorController = createController();

        // match provider theme
        mColorController.onToolbarInitialized(mToolbarManager);

        ColorStateList expectedFocusTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME, true);
        verify(mToolbarManager).onThemeColorChanged(THEME_COLOR, false);
        verify(mToolbarManager)
                .onTintChanged(
                        mThemeColorStateList,
                        expectedFocusTint,
                        BrandedColorScheme.DARK_BRANDED_THEME);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testDesktopWindowingModeMinUi_SetBrowserDefaultTheme() {
        // prepare web app in desktop windowing
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        setupMinimalUi();
        mColorController = createController();

        // expected theme is the browser default
        @ColorInt int expectedColor = ChromeColors.getDefaultThemeColor(mContext, false);
        @BrandedColorScheme
        int expectedColorScheme =
                ColorUtils.shouldUseLightForegroundOnBackground(expectedColor)
                        ? BrandedColorScheme.DARK_BRANDED_THEME
                        : BrandedColorScheme.LIGHT_BRANDED_THEME;
        ColorStateList expectedTint =
                ThemeUtils.getThemedToolbarIconTint(mContext, expectedColorScheme);
        ColorStateList expectedFocusTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, expectedColorScheme, true);
        // check toolbar is updated with browser default theme
        mColorController.onToolbarInitialized(mToolbarManager);
        verify(mToolbarManager).onThemeColorChanged(expectedColor, false);
        verify(mToolbarManager).onTintChanged(expectedTint, expectedFocusTint, expectedColorScheme);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public void testFullscreenModeMinUi_SetThemeFromProvider() {
        // prepare a web app in min ui mode
        setupDesktopWindowing(/* isInDesktopWindow= */ false);
        setupMinimalUi();
        mColorController = createController();

        ColorStateList expectedFocusTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME, true);

        // match provider theme
        mColorController.onToolbarInitialized(mToolbarManager);
        verify(mToolbarManager).onThemeColorChanged(THEME_COLOR, false);
        verify(mToolbarManager)
                .onTintChanged(
                        mThemeColorStateList,
                        expectedFocusTint,
                        BrandedColorScheme.DARK_BRANDED_THEME);
    }

    @Test
    public void testDesktopWindowingChangedInCCT_UpdateFromThemeProvider() {
        // prepare CCT in fullscreen mode
        setupDesktopWindowing(/* isInDesktopWindow= */ false);
        setupCustomTab();
        mColorController = createController();
        mColorController.onToolbarInitialized(mToolbarManager);

        // enter DW mode
        setupDesktopWindowing(/* isInDesktopWindow= */ true);
        mColorController.getAppHeaderObserver().onDesktopWindowingModeChanged(true);

        ColorStateList expectedFocusTint =
                ThemeUtils.getThemedToolbarIconTintForActivityState(
                        mContext, BrandedColorScheme.DARK_BRANDED_THEME, true);

        // match provider theme, twice because first update comes from the toolbar manager init
        verify(mToolbarManager, times(2)).onThemeColorChanged(THEME_COLOR, false);
        verify(mToolbarManager, times(2))
                .onTintChanged(
                        mThemeColorStateList,
                        expectedFocusTint,
                        BrandedColorScheme.DARK_BRANDED_THEME);
    }
}
