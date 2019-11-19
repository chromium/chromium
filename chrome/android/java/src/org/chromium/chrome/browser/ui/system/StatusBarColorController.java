// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.UiUtils;

/**
 * Maintains the status bar color for a {@link ChromeActivity}.
 */
public class StatusBarColorController
        implements Destroyable, TopToolbarCoordinator.UrlExpansionObserver {
    public static @ColorInt int UNDEFINED_STATUS_BAR_COLOR = Color.TRANSPARENT;

    /**
     * Provides the base status bar color.
     */
    public interface StatusBarColorProvider {
        /**
         * @return The base status bar color to override default colors used in the
         *         {@link StatusBarColorController}. If this returns a color other than
         *         {@link #UNDEFINED_STATUS_BAR_COLOR}, the {@link StatusBarColorController} will
         *         always use the color provided by this method to adjust the status bar color. This
         *         color may be used as-is or adjusted due to a scrim overlay or Android version.
         */
        @ColorInt
        int getBaseStatusBarColor();

        /**
         * @return Whether the color provided at {@link #getBaseStatusBarColor()} is considered a
         *         default theme color. If false, we use a darkened status bar color based on the
         *         base status bar color on pre-M instead of the default status bar color. This
         *         value will be ignored if {@link #getBaseStatusBarColor()} returns
         *         {@link #UNDEFINED_STATUS_BAR_COLOR}.
         */
        boolean isStatusBarDefaultThemeColor();
    }

    private final Window mWindow;
    private final boolean mIsTablet;
    private final @Nullable OverviewModeBehavior mOverviewModeBehavior;
    private final StatusBarColorProvider mStatusBarColorProvider;
    private final ScrimView.StatusBarScrimDelegate mStatusBarScrimDelegate;
    private final ActivityTabProvider.ActivityTabTabObserver mStatusBarColorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    private final @ColorInt int mStandardPrimaryBgColor;
    private final @ColorInt int mIncognitoPrimaryBgColor;
    private final @ColorInt int mStandardDefaultThemeColor;
    private final @ColorInt int mIncognitoDefaultThemeColor;

    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private @Nullable Tab mCurrentTab;
    private boolean mIsInOverviewMode;
    private boolean mIsIncognito;

    private @ColorInt int mScrimColor;
    private float mStatusBarScrimFraction;

    private float mToolbarUrlExpansionPercentage;
    private boolean mShouldUpdateStatusBarColorForNTP;

    /**
     * @param chromeActivity The {@link ChromeActivity} that this class is attached to.
     */
    public StatusBarColorController(ChromeActivity chromeActivity) {
        mWindow = chromeActivity.getWindow();
        mIsTablet = chromeActivity.isTablet();
        mOverviewModeBehavior = chromeActivity.getOverviewModeBehavior();
        mStatusBarColorProvider = chromeActivity;
        mStatusBarScrimDelegate = (fraction) -> {
            mStatusBarScrimFraction = fraction;
            updateStatusBarColor(mCurrentTab);
        };

        Resources resources = chromeActivity.getResources();
        mStandardPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(resources, false);
        mIncognitoPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(resources, true);
        mStandardDefaultThemeColor = ChromeColors.getDefaultThemeColor(resources, false);
        mIncognitoDefaultThemeColor = ChromeColors.getDefaultThemeColor(resources, true);

        mStatusBarColorTabObserver = new ActivityTabProvider.ActivityTabTabObserver(
                chromeActivity.getActivityTabProvider()) {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                updateStatusBarColor(tab);
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                updateStatusBarColor(tab);
            }

            @Override
            public void onContentChanged(Tab tab) {
                final boolean newShouldUpdateStatusBarColorForNTP = isStandardNTP();
                // Also update the status bar color if the content was previously an NTP, because an
                // NTP can use a different status bar color than the default theme color. In this
                // case, the theme color might not change, and thus #onDidChangeThemeColor might
                // not get called.
                if (mShouldUpdateStatusBarColorForNTP || newShouldUpdateStatusBarColorForNTP) {
                    updateStatusBarColor(tab);
                }
                mShouldUpdateStatusBarColorForNTP = newShouldUpdateStatusBarColorForNTP;
            }

            @Override
            public void onDestroyed(Tab tab) {
                // Make sure that #mCurrentTab is cleared because #onObservingDifferentTab() might
                // not be notified early enough when #onUrlExpansionPercentageChanged() is called.
                mCurrentTab = null;
                mShouldUpdateStatusBarColorForNTP = false;
            }

            @Override
            protected void onObservingDifferentTab(Tab tab) {
                mCurrentTab = tab;
                mShouldUpdateStatusBarColorForNTP = isStandardNTP();

                // |tab == null| means we're switching tabs - by the tab switcher or by swiping
                // on the omnibox. These cases are dealt with differently, elsewhere.
                if (tab == null) return;
                updateStatusBarColor(tab);
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mIsIncognito = newModel.isIncognito();

                // When opening a new Incognito Tab from a normal Tab (or vice versa), the
                // status bar color is updated. However, this update is triggered after the
                // animation, so we update here for the duration of the new Tab animation.
                // See https://crbug.com/917689.
                updateStatusBarColor(false);
            }
        };

        if (mOverviewModeBehavior != null) {
            mOverviewModeObserver = new EmptyOverviewModeObserver() {
                @Override
                public void onOverviewModeStartedShowing(boolean showToolbar) {
                    mIsInOverviewMode = true;
                    updateStatusBarColor(false);
                }

                @Override
                public void onOverviewModeFinishedHiding() {
                    mIsInOverviewMode = false;
                    updateStatusBarColor(mCurrentTab);
                }
            };
            mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        }

        chromeActivity.getLifecycleDispatcher().register(this);
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        mStatusBarColorTabObserver.destroy();
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
    }

    // TopToolbarCoordinator.UrlExpansionObserver implementation.
    @Override
    public void onUrlExpansionPercentageChanged(float percentage) {
        mToolbarUrlExpansionPercentage = percentage;
        if (mShouldUpdateStatusBarColorForNTP) updateStatusBarColor(mCurrentTab);
    }

    /**
     * @param tabModelSelector The {@link TabModelSelector} to check whether incognito model is
     *                         selected.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        assert mTabModelSelector == null : "mTabModelSelector should only be set once.";
        mTabModelSelector = tabModelSelector;
        if (mTabModelSelector != null) mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    /**
     * @return The {@link ScrimView.StatusBarScrimDelegate} that adjusts the status bar color based
     *         on the scrim offset.
     */
    public ScrimView.StatusBarScrimDelegate getStatusBarScrimDelegate() {
        return mStatusBarScrimDelegate;
    }

    /**
     * @param isDefaultThemeColor Whether default theme color is used for the status bar color.
     */
    public void updateStatusBarColor(boolean isDefaultThemeColor) {
        setStatusBarColor(calculateBaseStatusBarColor(), isDefaultThemeColor);
    }

    /**
     * @param tab The tab that is currently showing, used to determine whether {@code color} is the
     *            default theme color.
     */
    public void updateStatusBarColor(@Nullable Tab tab) {
        setStatusBarColor(calculateBaseStatusBarColor(), isDefaultThemeColor(tab));
    }

    private @ColorInt int calculateBaseStatusBarColor() {
        // Return overridden status bar color from StatusBarColorProvider if specified.
        final int baseStatusBarColor = mStatusBarColorProvider.getBaseStatusBarColor();
        if (baseStatusBarColor != UNDEFINED_STATUS_BAR_COLOR) return baseStatusBarColor;

        // We don't adjust status bar color for tablet when status bar color is not overridden by
        // StatusBarColorProvider.
        if (mIsTablet) return Color.BLACK;

        // Return status bar color in overview mode.
        if (mIsInOverviewMode) {
            boolean supportsDarkStatusIcons = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
            if (!supportsDarkStatusIcons) return Color.BLACK;

            if (!ChromeFeatureList.isInitialized()
                    || (!ChromeFeatureList.isEnabled(
                                ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                            && !DeviceClassManager.enableAccessibilityLayout()
                            && !FeatureUtilities.isGridTabSwitcherEnabled())) {
                return mStandardPrimaryBgColor;
            }

            return mIsIncognito ? mIncognitoPrimaryBgColor : mStandardPrimaryBgColor;
        }

        // Return status bar color in standard NewTabPage. If location bar is not shown in NTP, we
        // use the tab theme color regardless of the URL expansion percentage.
        if (isLocationBarShownInNTP()) {
            return ColorUtils.getColorWithOverlay(
                    TabThemeColorHelper.getBackgroundColor(mCurrentTab),
                    TabThemeColorHelper.getColor(mCurrentTab), mToolbarUrlExpansionPercentage);
        }

        // Return status bar color to match the toolbar.
        if (mCurrentTab != null) return TabThemeColorHelper.getColor(mCurrentTab);

        // This could happen when tab is not initialized (e.g. on startup).
        return mIsIncognito ? mIncognitoDefaultThemeColor : mStandardDefaultThemeColor;
    }

    private boolean isDefaultThemeColor(Tab tab) {
        if (mStatusBarColorProvider.getBaseStatusBarColor() != UNDEFINED_STATUS_BAR_COLOR) {
            return mStatusBarColorProvider.isStatusBarDefaultThemeColor();
        }

        return tab != null && TabThemeColorHelper.isDefaultColorUsed(tab);
    }

    /**
     * Set device status bar to a given color.
     * @param color The color that the status bar should be set to.
     * @param isDefaultThemeColor Whether {@code color} is the default theme color.
     */
    private void setStatusBarColor(int color, boolean isDefaultThemeColor) {
        if (UiUtils.isSystemUiThemingDisabled()) return;

        int statusBarColor = color;
        boolean supportsDarkStatusIcons = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
        View root = mWindow.getDecorView().getRootView();
        Resources resources = root.getResources();
        if (supportsDarkStatusIcons) {
            if (mScrimColor == 0) {
                mScrimColor = ApiCompatibilityUtils.getColor(resources, R.color.black_alpha_65);
            }
            // Apply a color overlay if the scrim is showing.
            float scrimColorAlpha = (mScrimColor >>> 24) / 255f;
            int scrimColorOpaque = mScrimColor & 0xFF000000;
            statusBarColor = ColorUtils.getColorWithOverlay(
                    statusBarColor, scrimColorOpaque, mStatusBarScrimFraction * scrimColorAlpha);

            boolean needsDarkStatusBarIcons =
                    !ColorUtils.shouldUseLightForegroundOnBackground(statusBarColor);
            ApiCompatibilityUtils.setStatusBarIconColor(root, needsDarkStatusBarIcons);
        } else {
            statusBarColor = isDefaultThemeColor ? Color.BLACK
                                                 : ColorUtils.getDarkenedColorForStatusBar(color);
        }

        ApiCompatibilityUtils.setStatusBarColor(mWindow, statusBarColor);
    }

    /**
     * @return Whether or not the current tab is a new tab page in standard mode.
     */
    private boolean isStandardNTP() {
        return mCurrentTab != null && mCurrentTab.getNativePage() instanceof NewTabPage;
    }

    /**
     * @return Whether or not the fake location bar is shown on the current NTP.
     */
    private boolean isLocationBarShownInNTP() {
        if (!isStandardNTP()) return false;
        final NewTabPage newTabPage = (NewTabPage) mCurrentTab.getNativePage();
        return newTabPage != null && newTabPage.isLocationBarShownInNTP();
    }
}
