// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType.INCOGNITO;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * {@link ThemeColorProvider} that calculates primary color based on the web app manifest data
 * presented in the {@link BrowserServicesIntentDataProvider}, {@link Tab}'s theme color, and Chrome
 * default color theme (including dynamic colors).
 */
@NullMarked
public class BrowserServicesThemeColorProvider extends ThemeColorProvider {

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ThemeColorSource.WEB_PAGE_THEME,
        ThemeColorSource.BROWSER_DEFAULT,
        ThemeColorSource.INTENT
    })
    public @interface ThemeColorSource {
        int WEB_PAGE_THEME = 0;
        int BROWSER_DEFAULT = 1;
        // BrowserServicesIntentDataProvider#getToolbarColor() should be used.
        int INTENT = 2;
    }

    /** Represents browser service theme */
    public static final class BrowserServiceTheme {
        public final @ColorInt int color;
        public final @BrandedColorScheme int brandedColorScheme;

        /**
         * @param color a primary color.
         * @param brandedColorScheme preferred color scheme.
         */
        public BrowserServiceTheme(
                @ColorInt int color, @BrandedColorScheme int brandedColorScheme) {
            this.color = color;
            this.brandedColorScheme = brandedColorScheme;
        }
    }

    private final Context mContext;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mTabSupplier;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;
    private boolean mShouldUseTabTheme;

    private final TabObserverRegistrar.CustomTabTabObserver mTabObserver =
            new TabObserverRegistrar.CustomTabTabObserver() {
                @Override
                protected void onObservingDifferentTab(@NonNull Tab tab) {
                    updateTheme();
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    updateTheme();
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    updateTheme();
                }

                @Override
                public void onShown(Tab tab, int type) {
                    updateTheme();
                }

                @Override
                public void onDidChangeThemeColor(Tab tab, int color) {
                    updateTheme();
                }
            };

    /**
     * @param context The {@link Context} that is used to retrieve color related resources.
     * @param intentDataProvider intent data provider that contains relevant browser service data,
     *     for example web app manifest properties.
     * @param tabSupplier provides current active tab in the browser service.
     * @param tabRegistrar allows to observe active tab changes, including the tab itself.
     */
    public BrowserServicesThemeColorProvider(
            Context context,
            BrowserServicesIntentDataProvider intentDataProvider,
            TopUiThemeColorProvider topUiThemeColorProvider,
            CustomTabActivityTabProvider tabSupplier,
            TabObserverRegistrar tabRegistrar) {
        super(context);
        mContext = context;
        mIntentDataProvider = intentDataProvider;
        mTabSupplier = tabSupplier;
        mTabObserverRegistrar = tabRegistrar;
        mTopUiThemeColorProvider = topUiThemeColorProvider;

        tabRegistrar.registerActivityTabObserver(mTabObserver);

        updateTheme();
    }

    /**
     * Computes the browser service color type. Returns a 'type' instead of a color so that the
     * function can be used by any UI surface with different values for {@link
     * ThemeColorSource.BROWSER_DEFAULT}.
     */
    public static @ThemeColorSource int computeColorSource(
            BrowserServicesIntentDataProvider intentDataProvider,
            boolean useTabThemeColor,
            @Nullable Tab tab) {
        if (intentDataProvider.isOpenedByChrome()) {
            if (intentDataProvider.getColorProvider().hasCustomToolbarColor()) {
                return ThemeColorSource.INTENT;
            }
            return (tab == null)
                    ? ThemeColorSource.BROWSER_DEFAULT
                    : ThemeColorSource.WEB_PAGE_THEME;
        }

        if (shouldUseDefaultThemeColorForFullscreen(intentDataProvider)) {
            return ThemeColorSource.BROWSER_DEFAULT;
        }

        if (tab != null && useTabThemeColor) {
            return ThemeColorSource.WEB_PAGE_THEME;
        }

        return intentDataProvider.getColorProvider().hasCustomToolbarColor()
                ? ThemeColorSource.INTENT
                : ThemeColorSource.BROWSER_DEFAULT;
    }

    /**
     * Sets whether the tab's theme color should be used for the top controls and triggers an update
     * of the color if needed.
     */
    public void setUseTabTheme(boolean shouldUseTabTheme) {
        if (mShouldUseTabTheme == shouldUseTabTheme) return;

        mShouldUseTabTheme = shouldUseTabTheme;
        updateTheme();
    }

    /**
     * Calculates primary color and color scheme of a tab based on the color source. This method
     * doesn't change internal state of the {@link ThemeColorProvider}.
     *
     * @param colorType color type that represents preferred color and scheme of the specific
     *     browser service.
     * @param tab currently active tab in the browser service.
     * @return {@link BrowserServiceTheme} that represents final theme for the browser service.
     */
    public BrowserServiceTheme calculateTheme(@ThemeColorSource int colorType, @Nullable Tab tab) {
        @ColorInt int color = computeColor(colorType, tab);
        @BrandedColorScheme int brandedColorScheme = computeBrandedColorScheme(colorType, color);
        return new BrowserServiceTheme(color, brandedColorScheme);
    }

    private void updateTheme() {
        @Nullable Tab tab = mTabSupplier.get();
        @ThemeColorSource
        int colorType = computeColorSource(mIntentDataProvider, mShouldUseTabTheme, tab);
        BrowserServiceTheme theme = calculateTheme(colorType, tab);
        ColorStateList tint =
                ThemeUtils.getThemedToolbarIconTint(mContext, theme.brandedColorScheme);

        updatePrimaryColor(theme.color, /* shouldAnimate= */ false);
        // TODO(https://crbug.com/396101043): support tint based on the activity focus state
        updateTint(tint, tint, theme.brandedColorScheme);
    }

    private @ColorInt int computeColor(@ThemeColorSource int colorType, @Nullable Tab tab) {
        final @ColorInt int color =
                switch (colorType) {
                    case ThemeColorSource.WEB_PAGE_THEME -> getTabThemeColor(tab);
                    case ThemeColorSource.INTENT -> mIntentDataProvider
                            .getColorProvider()
                            .getToolbarColor();
                    case ThemeColorSource.BROWSER_DEFAULT -> getDefaultChromeColor();
                    default -> getDefaultChromeColor();
                };

        return ColorUtils.getOpaqueColor(color);
    }

    private @ColorInt int getTabThemeColor(@Nullable Tab tab) {
        assert tab != null;
        return mTopUiThemeColorProvider.calculateColor(tab, tab.getThemeColor());
    }

    private @ColorInt int getDefaultChromeColor() {
        return SurfaceColorUpdateUtils.getDefaultThemeColor(
                mContext, mIntentDataProvider.getCustomTabMode() == INCOGNITO);
    }

    private @BrandedColorScheme int computeBrandedColorScheme(
            @ThemeColorSource int colorType, @ColorInt int color) {
        boolean isIncognitoBranded = mIntentDataProvider.getCustomTabMode() == INCOGNITO;
        return switch (colorType) {
            case ThemeColorSource.WEB_PAGE_THEME -> OmniboxResourceProvider.getBrandedColorScheme(
                    mContext, isIncognitoBranded, color);
            case ThemeColorSource.BROWSER_DEFAULT -> isIncognitoBranded
                    ? BrandedColorScheme.INCOGNITO
                    : BrandedColorScheme.APP_DEFAULT;
            case ThemeColorSource.INTENT -> ColorUtils.shouldUseLightForegroundOnBackground(color)
                    ? BrandedColorScheme.DARK_BRANDED_THEME
                    : BrandedColorScheme.LIGHT_BRANDED_THEME;
            default -> BrandedColorScheme.APP_DEFAULT;
        };
    }

    @Override
    public void destroy() {
        super.destroy();
        mTabObserverRegistrar.unregisterActivityTabObserver(mTabObserver);
    }

    private static boolean shouldUseDefaultThemeColorForFullscreen(
            BrowserServicesIntentDataProvider intentDataProvider) {
        // Don't use the theme color provided by the page if we're in display: fullscreen. This
        // works around an issue where the status bars go transparent and can't be seen on top of
        // the page content when users swipe them in or they appear because the on-screen keyboard
        // was triggered.
        WebappExtras webappExtras = intentDataProvider.getWebappExtras();
        return (webappExtras != null && webappExtras.displayMode == DisplayMode.FULLSCREEN);
    }

    @VisibleForTesting
    TabObserverRegistrar.CustomTabTabObserver getTabObserver() {
        return mTabObserver;
    }
}
