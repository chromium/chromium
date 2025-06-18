// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.content.Intent;
import android.graphics.Color;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
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
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testMinUiModePreSdk35_ResolveToStandalone() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.MINIMAL_UI));

        assertEquals(
                "Should resolve to standalone",
                DisplayMode.MINIMAL_UI,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testMinUiModeEnabled_ResolveToMinUI() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.MINIMAL_UI));

        assertEquals(
                "Should resolve to minimal ui",
                DisplayMode.MINIMAL_UI,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @EnableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testBrowserModeWithMinUiEnabled_ResolveToMinUi() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.BROWSER));

        assertEquals(
                "Should resolve to minimal ui",
                DisplayMode.MINIMAL_UI,
                intentDataProvider.getResolvedDisplayMode());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @DisableFeatures({ChromeFeatureList.ANDROID_MINIMAL_UI_LARGE_SCREEN})
    public void testBrowserModeWithMinUiDisabled_ResolveToStandalone() {
        var intentDataProvider =
                buildWebAppIntentDataProvider(mIntent, buildWebAppExtras(DisplayMode.BROWSER));

        assertEquals(
                "Should resolve to standalone",
                DisplayMode.STANDALONE,
                intentDataProvider.getResolvedDisplayMode());
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
