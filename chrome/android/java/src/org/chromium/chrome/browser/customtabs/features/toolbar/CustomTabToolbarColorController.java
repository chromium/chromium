// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType.INCOGNITO;

import android.app.Activity;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;

/** Maintains the toolbar color for {@link CustomTabActivity}. */
@ActivityScope
public class CustomTabToolbarColorController {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ToolbarColorType.THEME_COLOR,
        ToolbarColorType.DEFAULT_COLOR,
        ToolbarColorType.INTENT_TOOLBAR_COLOR
    })
    public @interface ToolbarColorType {
        int THEME_COLOR = 0;
        int DEFAULT_COLOR = 1;
        // BrowserServicesIntentDataProvider#getToolbarColor() should be used.
        int INTENT_TOOLBAR_COLOR = 2;
    }

    /**
     * Interface used to receive a predicate that tells if the current tab is in preview mode.
     * This makes the {@link #computeToolbarColorType()} test-friendly.
     */
    public interface BooleanFunction {
        boolean get();
    }

    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Activity mActivity;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final CustomTabActivityTabProvider mTabProvider;
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;

    private ToolbarManager mToolbarManager;
    private boolean mUseTabThemeColor;

    @Inject
    public CustomTabToolbarColorController(
            BrowserServicesIntentDataProvider intentDataProvider,
            Activity activity,
            CustomTabActivityTabProvider tabProvider,
            TabObserverRegistrar tabObserverRegistrar,
            TopUiThemeColorProvider topUiThemeColorProvider) {
        mIntentDataProvider = intentDataProvider;
        mActivity = activity;
        mTabProvider = tabProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
        mTopUiThemeColorProvider = topUiThemeColorProvider;
    }

    /**
     * Computes the toolbar color type.
     * Returns a 'type' instead of a color so that the function can be used by non-toolbar UI
     * surfaces with different values for {@link ToolbarColorType.DEFAULT_COLOR}.
     */
    public static int computeToolbarColorType(
            BrowserServicesIntentDataProvider intentDataProvider,
            boolean useTabThemeColor,
            @Nullable Tab tab) {
        if (intentDataProvider.isOpenedByChrome()) {
            if (intentDataProvider.getColorProvider().hasCustomToolbarColor()) {
                return ToolbarColorType.INTENT_TOOLBAR_COLOR;
            }
            return (tab == null) ? ToolbarColorType.DEFAULT_COLOR : ToolbarColorType.THEME_COLOR;
        }

        if (shouldUseDefaultThemeColorForFullscreen(intentDataProvider)) {
            return ToolbarColorType.DEFAULT_COLOR;
        }

        if (tab != null && useTabThemeColor) {
            return ToolbarColorType.THEME_COLOR;
        }

        return intentDataProvider.getColorProvider().hasCustomToolbarColor()
                ? ToolbarColorType.INTENT_TOOLBAR_COLOR
                : ToolbarColorType.DEFAULT_COLOR;
    }

    /**
     * Notifies the ColorController that the ToolbarManager has been created and is ready for
     * use. ToolbarManager isn't passed directly to the constructor because it's not guaranteed to
     * be initialized yet.
     */
    public void onToolbarInitialized(ToolbarManager manager) {
        mToolbarManager = manager;
        assert manager != null : "Toolbar manager not initialized";

        observeTabToUpdateColor();

        updateColor();
    }

    private void observeTabToUpdateColor() {
        mTabObserverRegistrar.registerActivityTabObserver(
                new CustomTabTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        // Update the color when the page load finishes.
                        updateColor();
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        // Update the color on every new URL.
                        updateColor();
                    }

                    @Override
                    public void onDidChangeThemeColor(Tab tab, int color) {
                        updateColor();
                    }

                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        updateColor();
                    }

                    @Override
                    public void onObservingDifferentTab(@NonNull Tab tab) {
                        updateColor();
                    }
                });
    }

    /**
     * Sets whether the tab's theme color should be used for the toolbar and triggers an update of
     * the toolbar color if needed.
     */
    public void setUseTabThemeColor(boolean useTabThemeColor) {
        if (mUseTabThemeColor == useTabThemeColor) return;

        mUseTabThemeColor = useTabThemeColor;
        updateColor();
    }

    /** Updates the color of the Activity's CCT Toolbar. */
    private void updateColor() {
        if (mToolbarManager == null) return;

        mToolbarManager.setShouldUpdateToolbarPrimaryColor(true);
        final Tab tab = mTabProvider.getTab();
        final @ToolbarColorType int toolbarColorType =
                computeToolbarColorType(mIntentDataProvider, mUseTabThemeColor, tab);
        final @ColorInt int color = computeColor(tab, toolbarColorType);
        final @BrandedColorScheme int brandedColorScheme =
                computeBrandedColorScheme(toolbarColorType, color);
        final ColorStateList tint =
                ThemeUtils.getThemedToolbarIconTint(mActivity, brandedColorScheme);
        mToolbarManager.onThemeColorChanged(color, false);
        mToolbarManager.onTintChanged(tint, tint, brandedColorScheme);
        mToolbarManager.setShouldUpdateToolbarPrimaryColor(false);
    }

    private @ColorInt int computeColor(Tab tab, @ToolbarColorType int toolbarColorType) {
        // TODO(b/300419189): Pass the CCT Top Bar Color in AGSA intent after Page Insights Hub is
        // launched
        if (GoogleBottomBarCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnableGoogleBottomBarForIntent(mIntentDataProvider)) {
            return mActivity.getColor(R.color.google_bottom_bar_background_color);
        }
        return switch (toolbarColorType) {
            case ToolbarColorType.THEME_COLOR -> mTopUiThemeColorProvider.calculateColor(
                    tab, tab.getThemeColor());
            case ToolbarColorType.DEFAULT_COLOR -> getDefaultColor();
            case ToolbarColorType.INTENT_TOOLBAR_COLOR -> mIntentDataProvider
                    .getColorProvider()
                    .getToolbarColor();
            default -> getDefaultColor();
        };
    }

    private @BrandedColorScheme int computeBrandedColorScheme(
            @ToolbarColorType int toolbarColorType, @ColorInt int toolbarColor) {
        final boolean isIncognitoBranded = mIntentDataProvider.getCustomTabMode() == INCOGNITO;
        return switch (toolbarColorType) {
            case ToolbarColorType.THEME_COLOR -> OmniboxResourceProvider.getBrandedColorScheme(
                    mActivity, isIncognitoBranded, toolbarColor);
            case ToolbarColorType.DEFAULT_COLOR -> isIncognitoBranded
                    ? BrandedColorScheme.INCOGNITO
                    : BrandedColorScheme.APP_DEFAULT;
            case ToolbarColorType.INTENT_TOOLBAR_COLOR -> ColorUtils
                            .shouldUseLightForegroundOnBackground(toolbarColor)
                    ? BrandedColorScheme.DARK_BRANDED_THEME
                    : BrandedColorScheme.LIGHT_BRANDED_THEME;
            default -> BrandedColorScheme.APP_DEFAULT;
        };
    }

    private int getDefaultColor() {
        return ChromeColors.getDefaultThemeColor(
                mActivity, mIntentDataProvider.getCustomTabMode() == INCOGNITO);
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
}
