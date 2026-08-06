// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.Color;
import android.os.Build;

import androidx.browser.trusted.TrustedWebActivityDisplayMode.ImmersiveMode;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@RunWith(BaseRobolectricTestRunner.class)
public class WebappIntentDataProviderTest {

    private Intent mIntent;

    @Before
    public void setup() {
        mIntent = new Intent();
    }

    private WebappIntentDataProvider buildWebAppIntentDataProvider(
            Intent intent, WebappExtras webappExtras) {
        return new WebappIntentDataProvider(
                intent,
                Color.WHITE,
                /* hasCustomToolbarColor= */ false,
                Color.BLACK,
                /* hasCustomDarkToolbarColor= */ false,
                /* shareData= */ null,
                webappExtras,
                /* webApkExtras= */ null);
    }

    private WebappIntentDataProvider buildWebAppIntentDataProviderWithThemeColor(
            int toolbarColor, boolean hasCustomToolbarColor) {
        return new WebappIntentDataProvider(
                mIntent,
                toolbarColor,
                hasCustomToolbarColor,
                Color.BLACK,
                /* hasCustomDarkToolbarColor= */ false,
                /* shareData= */ null,
                buildWebAppExtras(DisplayMode.STANDALONE),
                /* webApkExtras= */ null);
    }

    private WebappExtras buildWebAppExtras(@DisplayMode.EnumType int displayMode) {
        return new WebappExtras(
                "",
                "",
                "",
                new WebappIcon(),
                "",
                "",
                displayMode,
                0,
                0,
                0,
                0,
                0,
                false,
                false,
                false);
    }

    @Test
    public void testFullscreenMode_ResolveToFullscreen() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.FULLSCREEN));

        assertEquals(
                "Should resolve to fullscreen",
                DisplayMode.FULLSCREEN,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testFullscreenMode_UsesShortEdgesCutoutMode() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.FULLSCREEN));

        assertTrue(
                "Fullscreen mode should use immersive display mode",
                intentDataProvider.getProvidedTwaDisplayMode() instanceof ImmersiveMode);
        assertEquals(
                "Fullscreen mode should draw into short display cutout edges",
                LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                ((ImmersiveMode) intentDataProvider.getProvidedTwaDisplayMode())
                        .layoutInDisplayCutoutMode());
        assertTrue(
                "Fullscreen mode should use sticky immersive mode",
                ((ImmersiveMode) intentDataProvider.getProvidedTwaDisplayMode()).isSticky());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testFullscreenMode_FeatureDisabled_UsesDefaultCutoutMode() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.FULLSCREEN));

        assertTrue(
                "Fullscreen mode should use immersive display mode",
                intentDataProvider.getProvidedTwaDisplayMode() instanceof ImmersiveMode);
        assertEquals(
                "Fullscreen mode should use default display cutout mode when feature is disabled",
                LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                ((ImmersiveMode) intentDataProvider.getProvidedTwaDisplayMode())
                        .layoutInDisplayCutoutMode());
        assertFalse(
                "Fullscreen mode should not use sticky immersive mode when feature is disabled",
                ((ImmersiveMode) intentDataProvider.getProvidedTwaDisplayMode()).isSticky());
    }

    @Test
    public void testStandaloneMode_ResolveToStandalone() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.STANDALONE));

        assertEquals(
                "Should resolve to standalone",
                DisplayMode.STANDALONE,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testMinUiModePreSdk35_ResolveToStandalone() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.MINIMAL_UI));

        assertEquals(
                "Should resolve to standalone",
                DisplayMode.MINIMAL_UI,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    public void testMinUiModeEnabled_ResolveToMinUI() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.MINIMAL_UI));

        assertEquals(
                "Should resolve to minimal ui",
                DisplayMode.MINIMAL_UI,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    public void testBrowserModeWithMinUiEnabled_ResolveToMinUi() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.BROWSER));

        assertEquals(
                "Should resolve to minimal ui",
                DisplayMode.MINIMAL_UI,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_NAVIGATION_BAR_THEME_COLOR)
    public void testNavigationBarColor_UsesThemeColorWhenFeatureEnabled() {
        var intentDataProvider =
                buildWebAppIntentDataProviderWithThemeColor(
                        Color.RED, /* hasCustomToolbarColor= */ true);

        assertEquals(
                "Navigation bar should use the manifest theme color",
                Integer.valueOf(Color.RED),
                intentDataProvider.getLightColorProvider().getNavigationBarColor());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_NAVIGATION_BAR_THEME_COLOR)
    public void testNavigationBarColor_NullWhenNoCustomThemeColor() {
        var intentDataProvider =
                buildWebAppIntentDataProviderWithThemeColor(
                        Color.WHITE, /* hasCustomToolbarColor= */ false);

        assertNull(
                "Navigation bar color should be unset when no theme color is specified",
                intentDataProvider.getLightColorProvider().getNavigationBarColor());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.WEB_APP_NAVIGATION_BAR_THEME_COLOR)
    public void testNavigationBarColor_NullWhenFeatureDisabled() {
        var intentDataProvider =
                buildWebAppIntentDataProviderWithThemeColor(
                        Color.RED, /* hasCustomToolbarColor= */ true);

        assertNull(
                "Navigation bar color should be unset when the feature is disabled",
                intentDataProvider.getLightColorProvider().getNavigationBarColor());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testIsOptionalButtonSupported() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.STANDALONE));
        assertFalse(
                "Webapp should not support optional button",
                intentDataProvider.isOptionalButtonSupported());
    }
}
