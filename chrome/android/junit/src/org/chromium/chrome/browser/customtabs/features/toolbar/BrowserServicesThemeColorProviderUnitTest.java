// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsIntent.ColorScheme;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider.ThemeColorSource;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class BrowserServicesThemeColorProviderUnitTest {
    private static final int LIGHT_COLOR = Color.GREEN;
    private static final int DARK_COLOR = Color.BLACK;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock public Tab tab;
    @Mock public TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock public CustomTabActivityTabProvider mCustomTabActivityTabProvider;
    @Mock public TabObserverRegistrar mTabObserverRegistrar;

    private Context mContext;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    private BrowserServicesThemeColorProvider createThemeColorProvider(
            BrowserServicesIntentDataProvider intentDataProvider) {
        return new BrowserServicesThemeColorProvider(
                mContext,
                intentDataProvider,
                mTopUiThemeColorProvider,
                mCustomTabActivityTabProvider,
                mTabObserverRegistrar);
    }

    private BrowserServicesIntentDataProvider buildCctIntentDataProvider(
            @ColorScheme int colorScheme,
            @Nullable CustomTabColorSchemeParams schemeParams,
            boolean isOpenedByChrome,
            boolean isIncognito) {
        CustomTabsSession session =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(mContext, ChromeLauncherActivity.class));
        var builder = new CustomTabsIntent.Builder(session);
        if (schemeParams != null) {
            builder.setColorSchemeParams(colorScheme, schemeParams);
        } else {
            builder.setColorScheme(colorScheme);
        }

        var intent = builder.build().intent;
        IntentUtils.setForceIsTrustedIntentForTesting(isOpenedByChrome);

        if (isIncognito) {
            return new IncognitoCustomTabIntentDataProvider(intent, mContext, colorScheme);
        } else {
            return new CustomTabIntentDataProvider(intent, mContext, colorScheme);
        }
    }

    @Test
    public void testTabOpenedByChromeHasIntentColor_UseDefaultTheme() {
        var colorParams =
                new CustomTabColorSchemeParams.Builder().setToolbarColor(DARK_COLOR).build();
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        colorParams,
                        /* isOpenedByChrome= */ true,
                        /* isIncognito= */ false);

        assertEquals(
                "Intent theme is expected",
                ThemeColorSource.INTENT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        intentDataProvider, false, null));
    }

    @Test
    public void testReparentingTabOpenedByChromeNoIntentColor_UseDefaultTheme() {
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ true,
                        /* isIncognito= */ false);

        assertEquals(
                "Default theme is expected",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        intentDataProvider, false, null));
    }

    @Test
    public void testCCTOpenedByChromeNoIntentColor_UsePageTheme() {
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ true,
                        /* isIncognito= */ false);

        assertEquals(
                "Page theme is expected",
                ThemeColorSource.WEB_PAGE_THEME,
                BrowserServicesThemeColorProvider.computeColorSource(
                        intentDataProvider, false, tab));
    }

    @Test
    public void testFullscreenWebApp_UseDefaultTheme() {
        BrowserServicesIntentDataProvider webAppIntentDataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        when(webAppIntentDataProvider.isOpenedByChrome()).thenReturn(false);
        when(webAppIntentDataProvider.getWebappExtras())
                .thenReturn(buildWebAppExtras(DisplayMode.FULLSCREEN));
        assertEquals(
                "Default theme is expected, to keep status bar visible",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        webAppIntentDataProvider, false, tab));
    }

    @Test
    public void testUseTabThemeColor() {
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);

        assertEquals(
                "Default theme is expected by default",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        intentDataProvider, false, tab));
    }

    @Test
    public void testIntentHasCustomColor() {
        var colorSchemeParams =
                new CustomTabColorSchemeParams.Builder().setToolbarColor(DARK_COLOR).build();
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ colorSchemeParams,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);

        assertEquals(
                "Intent theme is expected",
                ThemeColorSource.INTENT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        intentDataProvider, false, null));
    }

    @Test
    public void testDefaultColorTheme() {
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);

        assertEquals(
                "Default theme is expected",
                ThemeColorSource.BROWSER_DEFAULT,
                BrowserServicesThemeColorProvider.computeColorSource(
                        intentDataProvider, false, null));
    }

    @Test
    public void testLightColorTabTheme_TabColorWithLightScheme() {
        // emulate not incognito tab with page theme
        when(tab.getThemeColor()).thenReturn(LIGHT_COLOR);
        when(mTopUiThemeColorProvider.calculateColor(eq(tab), eq(LIGHT_COLOR)))
                .thenReturn(LIGHT_COLOR);
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);
        var themeColorProvider = createThemeColorProvider(intentDataProvider);

        // check
        var actual = themeColorProvider.calculateTheme(ThemeColorSource.WEB_PAGE_THEME, tab);
        assertEquals("Should be tab color", LIGHT_COLOR, actual.color);
        assertEquals(
                "Should be light scheme",
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                actual.brandedColorScheme);
    }

    @Test
    public void testDarkColorTabTheme_TabColorWithDarkScheme() {
        // emulate not incognito tab with page theme
        when(tab.getThemeColor()).thenReturn(DARK_COLOR);
        when(mTopUiThemeColorProvider.calculateColor(eq(tab), eq(DARK_COLOR)))
                .thenReturn(DARK_COLOR);
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);
        var themeColorProvider = createThemeColorProvider(intentDataProvider);

        // check
        var actual = themeColorProvider.calculateTheme(ThemeColorSource.WEB_PAGE_THEME, tab);
        assertEquals("Should be tab color", DARK_COLOR, actual.color);
        assertEquals(
                "Should be dark scheme",
                BrandedColorScheme.DARK_BRANDED_THEME,
                actual.brandedColorScheme);
    }

    @Test
    public void testLightColorIntentTheme_IntentColorLightScheme() {
        // emulate not incognito tab with intent theme
        var colorSchemeParams =
                new CustomTabColorSchemeParams.Builder().setToolbarColor(LIGHT_COLOR).build();
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ colorSchemeParams,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);
        var themeColorProvider = createThemeColorProvider(intentDataProvider);

        // check
        var actual = themeColorProvider.calculateTheme(ThemeColorSource.INTENT, tab);
        assertEquals("Should be intent color", LIGHT_COLOR, actual.color);
        assertEquals(
                "Should be light scheme",
                BrandedColorScheme.LIGHT_BRANDED_THEME,
                actual.brandedColorScheme);
    }

    @Test
    public void testDarkColorIntentTheme_IntentColorDarkScheme() {
        // emulate not incognito tab with intent theme
        var colorSchemeParams =
                new CustomTabColorSchemeParams.Builder().setToolbarColor(DARK_COLOR).build();
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ colorSchemeParams,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);
        var themeColorProvider = createThemeColorProvider(intentDataProvider);

        // check
        var actual = themeColorProvider.calculateTheme(ThemeColorSource.INTENT, tab);
        assertEquals("Should be intent color", DARK_COLOR, actual.color);
        assertEquals(
                "Should be dark scheme",
                BrandedColorScheme.DARK_BRANDED_THEME,
                actual.brandedColorScheme);
    }

    @Test
    public void testChromeTheme() {
        // emulate not incognito tab with chrome default theme
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ false);
        var themeColorProvider = createThemeColorProvider(intentDataProvider);

        // check
        var actual = themeColorProvider.calculateTheme(ThemeColorSource.BROWSER_DEFAULT, tab);
        assertEquals(
                "Should be app default color",
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ false),
                actual.color);
        assertEquals(
                "Should be chrome default scheme",
                BrandedColorScheme.APP_DEFAULT,
                actual.brandedColorScheme);
    }

    @Test
    public void testIncognitoTheme() {
        // emulate incognito tab with chrome default theme
        var intentDataProvider =
                buildCctIntentDataProvider(
                        COLOR_SCHEME_LIGHT,
                        /* schemeParams= */ null,
                        /* isOpenedByChrome= */ false,
                        /* isIncognito= */ true);
        var themeColorProvider = createThemeColorProvider(intentDataProvider);

        // check
        var actual = themeColorProvider.calculateTheme(ThemeColorSource.BROWSER_DEFAULT, tab);
        assertEquals(
                "Should be incognito color",
                SurfaceColorUpdateUtils.getDefaultThemeColor(mContext, /* isIncognito= */ true),
                actual.color);
        assertEquals(
                "Should be incognito scheme",
                BrandedColorScheme.INCOGNITO,
                actual.brandedColorScheme);
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
