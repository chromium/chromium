// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider.ThemeColorSource;
import org.chromium.chrome.browser.tab.Tab;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class BrowserServicesThemeColorProviderUnitTest {

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock public BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock public ColorProvider mColorProvider;
    @Mock public Tab tab;

    @Before
    public void setup() {
        when(mIntentDataProvider.getColorProvider()).thenReturn(mColorProvider);
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(false);
        when(mIntentDataProvider.getCustomTabMode()).thenReturn(CustomTabProfileType.REGULAR);
        when(mIntentDataProvider.getColorProvider()).thenReturn(mColorProvider);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(false);
    }

    @Test
    public void testTabOpenedByChromeHasIntentColor_UseDefaultTheme() {
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(true);
        assertEquals(
                "Intent theme is expected",
                ThemeColorSource.INTENT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        mIntentDataProvider, false, null));
    }

    @Test
    public void testReparentingTabOpenedByChromeNoIntentColor_UseDefaultTheme() {
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        assertEquals(
                "Default theme is expected",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        mIntentDataProvider, false, null));
    }

    @Test
    public void testCCTOpenedByChromeNoIntentColor_UsePageTheme() {
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(true);
        assertEquals(
                "Page theme is expected",
                ThemeColorSource.WEB_PAGE_THEME,
                BrowserServicesThemeColorProvider.computeColorSource(
                        mIntentDataProvider, false, tab));
    }

    @Test
    public void testFullscreenWebApp_UseDefaultTheme() {
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(false);
        when(mIntentDataProvider.getWebappExtras())
                .thenReturn(buildWebAppExtras(DisplayMode.FULLSCREEN));
        assertEquals(
                "Default theme is expected, to keep status bar visible",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        mIntentDataProvider, false, tab));
    }

    @Test
    public void testUseTabThemeColor() {
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(false);
        assertEquals(
                "Default theme is expected by default",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        mIntentDataProvider, false, tab));
    }

    @Test
    public void testIntentHasCustomColor() {
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(false);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(true);
        assertEquals(
                "Intent theme is expected",
                ThemeColorSource.INTENT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        mIntentDataProvider, false, null));
    }

    @Test
    public void testDefaultColorTheme() {
        when(mIntentDataProvider.isOpenedByChrome()).thenReturn(false);
        when(mColorProvider.hasCustomToolbarColor()).thenReturn(false);
        assertEquals(
                "Default theme is expected",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        mIntentDataProvider, false, null));
    }

    private static WebappExtras buildWebAppExtras(final int displayMode) {
        return new WebappExtras(
                /* id= */ "",
                /* url= */ "",
                /* scopeUrl= */ "",
                /* icon= */ new WebappIcon(),
                /* name= */ "",
                /* shortName= */ "",
                /* displayMode= */ displayMode,
                /* orientation= */ 0,
                /* source= */ 0,
                /* backgroundColor= */ 0,
                /* darkBackgroundColor= */ 0,
                /* defaultBackgroundColor= */ 0,
                /* isIconGenerated= */ false,
                /* isIconAdaptive= */ false,
                /* shouldForceNavigation= */ false);
    }
}
