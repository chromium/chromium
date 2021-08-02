// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.res.Resources;
import android.os.Build;
import android.view.ViewGroup;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CallbackController;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.device.DeviceClassManager;
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
    private @Nullable OverviewModeBehavior mOverviewModeBehavior;
    private @Nullable OverviewModeObserver mOverviewModeObserver;
    private CallbackController mCallbackController = new CallbackController();

    private boolean mForceDarkNavigationBarColor;
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
            OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1;

        mWindow = window;
        mRootView = (ViewGroup) mWindow.getDecorView().getRootView();
        mResources = mRootView.getResources();
        mDefaultScrimColor =
                ApiCompatibilityUtils.getColor(mResources, R.color.default_scrim_color);

        // If we're not using a light navigation bar, it will always be the same dark color so
        // there's no need to register observers and manipulate coloring.
        if (!mResources.getBoolean(R.bool.window_light_navigation_bar)) {
            mTabModelSelector = null;
            mTabModelSelectorObserver = null;
            return;
        }

        mTabModelSelector = tabModelSelector;
        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                updateNavigationBarColor();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        overviewModeBehaviorSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setOverviewModeBehavior));

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
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
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
        UiUtils.setNavigationBarIconColor(mRootView, !mForceDarkNavigationBarColor);
    }

    @Override
    public void onEnterVr() {}

    @SuppressLint("NewApi")
    private void updateNavigationBarColor() {
        boolean overviewVisible = mOverviewModeBehavior != null
                && mOverviewModeBehavior.overviewVisible() && !mOverviewModeHiding;

        boolean forceDarkNavigation;
        if (DeviceClassManager.enableAccessibilityLayout()
                || TabUiFeatureUtilities.isGridTabSwitcherEnabled(mRootView.getContext())) {
            forceDarkNavigation = mTabModelSelector.isIncognitoSelected();
        } else {
            forceDarkNavigation = mTabModelSelector.isIncognitoSelected() && !overviewVisible;
        }

        forceDarkNavigation &= !UiUtils.isSystemUiThemingDisabled();

        if (mForceDarkNavigationBarColor == forceDarkNavigation) return;

        mForceDarkNavigationBarColor = forceDarkNavigation;

        mWindow.setNavigationBarColor(getNavigationBarColor(mForceDarkNavigationBarColor));
        setNavigationBarDividerColor();
        UiUtils.setNavigationBarIconColor(mRootView, !mForceDarkNavigationBarColor);
    }

    @SuppressLint("NewApi")
    private void setNavigationBarDividerColor() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(
                    getNavigationBarDividerColor(mForceDarkNavigationBarColor));
        }
    }

    /**
     * Update the scrim amount on the navigation bar.
     * @param fraction The scrim fraction in range [0, 1].
     */
    public void setNavigationBarScrimFraction(float fraction) {
        mNavigationBarScrimFraction = fraction;
        mWindow.setNavigationBarColor(
                applyCurrentScrimToColor(getNavigationBarColor(mForceDarkNavigationBarColor)));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(applyCurrentScrimToColor(
                    getNavigationBarDividerColor(mForceDarkNavigationBarColor)));
        }

        // Adjust the color of navigation bar icons based on color state of the navigation bar.
        if (MathUtils.areFloatsEqual(1f, fraction)) {
            UiUtils.setNavigationBarIconColor(mRootView, false);
        } else if (MathUtils.areFloatsEqual(0f, fraction)) {
            UiUtils.setNavigationBarIconColor(mRootView, true);
        }
    }

    private @ColorInt int getNavigationBarColor(boolean forceDarkNavigationBar) {
        return ApiCompatibilityUtils.getColor(mResources,
                forceDarkNavigationBar ? R.color.toolbar_background_primary_dark
                                       : R.color.bottom_system_nav_color);
    }

    private @ColorInt int getNavigationBarDividerColor(boolean forceDarkNavigationBar) {
        return ApiCompatibilityUtils.getColor(mResources,
                forceDarkNavigationBar ? R.color.bottom_system_nav_divider_color_light
                                       : R.color.bottom_system_nav_divider_color);
    }

    private @ColorInt int applyCurrentScrimToColor(@ColorInt int color) {
        // Apply a color overlay.
        float scrimColorAlpha = (mDefaultScrimColor >>> 24) / 255f;
        int scrimColorOpaque = mDefaultScrimColor & 0xFF000000;
        return ColorUtils.getColorWithOverlay(
                color, scrimColorOpaque, mNavigationBarScrimFraction * scrimColorAlpha, true);
    }
}
