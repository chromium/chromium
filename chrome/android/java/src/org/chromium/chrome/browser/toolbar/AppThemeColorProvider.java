// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** A ThemeColorProvider for the app theme (incognito or standard theming). */
public class AppThemeColorProvider extends ThemeColorProvider implements IncognitoStateObserver {
    /** Primary color for standard mode. */
    private final int mStandardPrimaryColor;

    /** Primary color for incognito mode. */
    private final int mIncognitoPrimaryColor;

    /** Used to know when incognito mode is entered or exited. */
    private IncognitoStateProvider mIncognitoStateProvider;

    /** Used to know the Layout state. */
    private LayoutStateProvider mLayoutStateProvider;

    /** Observer to know when Layout state is changed, e.g show/hide. */
    private final LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    /** Whether app is in incognito mode. */
    private boolean mIsIncognito;

    /** Whether app is in overview mode. */
    private boolean mIsOverviewVisible;

    /** The activity {@link Context}. */
    private final Context mActivityContext;

    AppThemeColorProvider(Context context) {
        super(context);

        mActivityContext = context;
        mStandardPrimaryColor = ChromeColors.getDefaultThemeColor(context, false);
        mIncognitoPrimaryColor = ChromeColors.getDefaultThemeColor(context, true);

        mLayoutStateObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onStartedShowing(@LayoutType int layoutType, boolean showToolbar) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    mIsOverviewVisible = true;
                    updateTheme();
                }
            }

            @Override
            public void onStartedHiding(
                    @LayoutType int layoutType, boolean showToolbar, boolean delayAnimation) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    mIsOverviewVisible = false;
                    updateTheme();
                }
            }
        };
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        mIsIncognito = isIncognito;
        updateTheme();
    }

    void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void updateTheme() {
        final boolean shouldUseIncognitoBackground = mIsIncognito
                && (!mIsOverviewVisible
                        || ToolbarColors.canUseIncognitoToolbarThemeColorInOverview(
                                mActivityContext));

        updatePrimaryColor(
                shouldUseIncognitoBackground ? mIncognitoPrimaryColor : mStandardPrimaryColor,
                false);
        final @BrandedColorScheme int brandedColorScheme = shouldUseIncognitoBackground
                ? BrandedColorScheme.INCOGNITO
                : BrandedColorScheme.APP_DEFAULT;
        final ColorStateList iconTint =
                ThemeUtils.getThemedToolbarIconTint(mActivityContext, brandedColorScheme);
        updateTint(iconTint, brandedColorScheme);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
        }
    }
}
