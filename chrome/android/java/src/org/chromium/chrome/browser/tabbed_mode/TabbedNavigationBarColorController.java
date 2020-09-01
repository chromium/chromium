// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.view.ViewGroup;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.ui.UiUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.vr.VrModeObserver;

/**
 * Controls the bottom system navigation bar color for the provided {@link Window}.
 */
@TargetApi(Build.VERSION_CODES.O_MR1)
class TabbedNavigationBarColorController implements VrModeObserver {
    private final Window mWindow;
    private final ViewGroup mRootView;
    private final Resources mResources;
    private final @ColorInt int mDefaultScrimColor;

    // May be null if we return from the constructor early. Otherwise will be set.
    private final @Nullable TabModelSelector mTabModelSelector;
    private final @Nullable TabModelSelectorObserver mTabModelSelectorObserver;
    private @Nullable ObservableSupplier<OverviewModeBehavior> mOverviewModeBehaviorSupplier;
    private @Nullable Callback<OverviewModeBehavior> mOverviewModeSupplierCallback;
    private @Nullable OverviewModeBehavior mOverviewModeBehavior;
    private @Nullable OverviewModeObserver mOverviewModeObserver;

    private boolean mUseLightNavigation;
    private boolean mOverviewModeHiding;
    private float mNavigationBarScrimFraction;

    /**
     * Creates a new {@link TabbedNavigationBarColorController} instance.
     * @param window The {@link Window} this controller should operate on.
     * @param tabModelSelector The {@link TabModelSelector} used to determine which tab model is
     *                         selected.
     * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
     *         {@link OverviewModeBehavior} associated with the containing activity.
     */
    TabbedNavigationBarColorController(Window window, TabModelSelector tabModelSelector,
            ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1;

        mWindow = window;
        mRootView = (ViewGroup) mWindow.getDecorView().getRootView();
        mResources = mRootView.getResources();
        mDefaultScrimColor = ApiCompatibilityUtils.getColor(mResources, R.color.black_alpha_65);

        // If we're not using a light navigation bar, it will always be black so there's no need
        // to register observers and manipulate coloring.
        if (!mResources.getBoolean(R.bool.window_light_navigation_bar)) {
            mTabModelSelector = null;
            mTabModelSelectorObserver = null;
            return;
        }

        mUseLightNavigation = true;

        mTabModelSelector = tabModelSelector;
        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                updateNavigationBarColor();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mOverviewModeBehaviorSupplier = overviewModeBehaviorSupplier;
        mOverviewModeSupplierCallback = this::setOverviewModeBehavior;
        mOverviewModeBehaviorSupplier.addObserver(mOverviewModeSupplierCallback);

        // TODO(https://crbug.com/806054): Observe tab loads to restrict black bottom nav to
        // incognito NTP.

        updateNavigationBarColor();

        VrModuleProvider.registerVrModeObserver(this);
    }

    /**
     * Destroy this {@link TabbedNavigationBarColorController} instance.
     */
    public void destroy() {
        if (mTabModelSelector != null) mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        if (mOverviewModeBehaviorSupplier != null) {
            mOverviewModeBehaviorSupplier.removeObserver(mOverviewModeSupplierCallback);
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
        VrModuleProvider.unregisterVrModeObserver(this);
    }

    /**
     * @param overviewModeBehavior The {@link OverviewModeBehavior} used to determine whether
     *                             overview mode is showing.
     */
    private void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }

        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mOverviewModeHiding = false;
                updateNavigationBarColor();
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mOverviewModeHiding = true;
                updateNavigationBarColor();
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mOverviewModeHiding = false;
            }
        };
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        updateNavigationBarColor();
    }

    @Override
    public void onExitVr() {
        // The platform ignores the light navigation bar system UI flag when launching an Activity
        // in VR mode, so we need to restore it when VR is exited.
        UiUtils.setNavigationBarIconColor(mRootView, mUseLightNavigation);
    }

    @Override
    public void onEnterVr() {}

    @SuppressLint("NewApi")
    private void updateNavigationBarColor() {
        boolean overviewVisible = mOverviewModeBehavior != null
                && mOverviewModeBehavior.overviewVisible() && !mOverviewModeHiding;

        boolean useLightNavigation;
        if (ChromeFeatureList.isInitialized()
                && (ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                        || DeviceClassManager.enableAccessibilityLayout()
                        || TabUiFeatureUtilities.isGridTabSwitcherEnabled())) {
            useLightNavigation = !mTabModelSelector.isIncognitoSelected();
        } else {
            useLightNavigation = !mTabModelSelector.isIncognitoSelected() || overviewVisible;
        }

        useLightNavigation &= !UiUtils.isSystemUiThemingDisabled();

        if (mUseLightNavigation == useLightNavigation) return;

        mUseLightNavigation = useLightNavigation;

        mWindow.setNavigationBarColor(useLightNavigation ? ApiCompatibilityUtils.getColor(
                                              mResources, R.color.bottom_system_nav_color)
                                                         : Color.BLACK);

        setNavigationBarColor(useLightNavigation);

        UiUtils.setNavigationBarIconColor(mRootView, useLightNavigation);
    }

    @SuppressLint("NewApi")
    private void setNavigationBarColor(boolean useLightNavigation) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(useLightNavigation
                            ? ApiCompatibilityUtils.getColor(
                                    mResources, R.color.bottom_system_nav_divider_color)
                            : Color.BLACK);
        }
    }

    /**
     * Update the scrim amount on the navigation bar. Note that we only update when the navigation
     * bar color is in light mode.
     * @param fraction The scrim fraction in range [0, 1].
     */
    public void setNavigationBarScrimFraction(float fraction) {
        if (!mUseLightNavigation) {
            return;
        }
        mNavigationBarScrimFraction = fraction;
        mWindow.setNavigationBarColor(applyCurrentScrimToColor(
                ApiCompatibilityUtils.getColor(mResources, R.color.bottom_system_nav_color)));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(
                    applyCurrentScrimToColor(ApiCompatibilityUtils.getColor(
                            mResources, R.color.bottom_system_nav_divider_color)));
        }

        // Adjust the color of navigation bar icons based on color state of the navigation bar.
        if (MathUtils.areFloatsEqual(1f, fraction)) {
            UiUtils.setNavigationBarIconColor(mRootView, false);
        } else if (MathUtils.areFloatsEqual(0f, fraction)) {
            UiUtils.setNavigationBarIconColor(mRootView, true);
        }
    }

    private @ColorInt int applyCurrentScrimToColor(@ColorInt int color) {
        // Apply a color overlay.
        float scrimColorAlpha = (mDefaultScrimColor >>> 24) / 255f;
        int scrimColorOpaque = mDefaultScrimColor & 0xFF000000;
        return ColorUtils.getColorWithOverlay(
                color, scrimColorOpaque, mNavigationBarScrimFraction * scrimColorAlpha, true);
    }
}
