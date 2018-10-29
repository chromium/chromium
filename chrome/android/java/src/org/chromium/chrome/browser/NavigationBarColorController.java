// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.vr.VrModeObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.ui.UiUtils;

/**
 * Controls the bottom system navigation bar color for the provided {@link Window}.
 */
@TargetApi(Build.VERSION_CODES.O_MR1)
public class NavigationBarColorController implements VrModeObserver {
    private final Window mWindow;
    private final ViewGroup mRootView;
    private final Resources mResources;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final OverviewModeBehavior mOverviewModeBehavior;
    private final OverviewModeObserver mOverviewModeObserver;

    private boolean mUseLightNavigation;
    private boolean mOverviewModeHiding;

    /**
     * Creates a new {@link NavigationBarColorController} instance.
     * @param window The {@link Window} this controller should operate on.
     * @param tabModelSelector The {@link TabModelSelector} used to determine which tab model is
     *                         selected.
     * @param overviewModeBehavior The {@link OverviewModeObserver} used to determine whether
     *                             overview mode is showing.
     */
    public NavigationBarColorController(Window window, TabModelSelector tabModelSelector,
            OverviewModeBehavior overviewModeBehavior) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1;

        mWindow = window;
        mRootView = (ViewGroup) mWindow.getDecorView().getRootView();
        mResources = mRootView.getResources();
        mUseLightNavigation = true;

        mTabModelSelector = tabModelSelector;
        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                updateNavigationBarColor();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        // TODO(https://crbug.com/806054): Observe tab loads to restrict black bottom nav to
        // incognito NTP.

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

        VrModuleProvider.registerVrModeObserver(this);
    }

    /**
     * Destroy this {@link NavigationBarColorController} instance.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        VrModuleProvider.unregisterVrModeObserver(this);
    }

    @Override
    public void onExitVr() {
        // The platform ignores the light navigation bar system UI flag when launching an Activity
        // in VR mode, so we need to restore it when VR is exited.
        updateSystemUiVisibility(mUseLightNavigation);
    }

    @Override
    public void onEnterVr() {}

    private void updateNavigationBarColor() {
        boolean overviewVisible = mOverviewModeBehavior.overviewVisible() && !mOverviewModeHiding;

        boolean useLightNavigation;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                || DeviceClassManager.enableAccessibilityLayout()) {
            useLightNavigation = !mTabModelSelector.isIncognitoSelected();
        } else {
            useLightNavigation = !mTabModelSelector.isIncognitoSelected() || overviewVisible;
        }

        useLightNavigation &= !UiUtils.isSystemUiThemingDisabled();

        if (mUseLightNavigation == useLightNavigation) return;

        mUseLightNavigation = useLightNavigation;

        mWindow.setNavigationBarColor(useLightNavigation
                        ? ApiCompatibilityUtils.getColor(
                                  mResources, R.color.bottom_system_nav_color)
                        : Color.BLACK);

        setNavigationBarColor(useLightNavigation);

        updateSystemUiVisibility(useLightNavigation);
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

    private void updateSystemUiVisibility(boolean useLightNavigation) {
        int visibility = mRootView.getSystemUiVisibility();
        if (useLightNavigation) {
            visibility |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        } else {
            visibility &= ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        }
        mRootView.setSystemUiVisibility(visibility);
    }
}
