// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;

import androidx.annotation.Px;

import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;

/**
 * Class responsible for how the Partial Chrome Custom Tabs are displayed on the screen.
 * It creates and handles the supported size strategies for Partial Chrome Custom Tabs based on the
 * intent extras values provided by the embedder, the window size, and the device state.
 */
public class PartialCustomTabDisplayManager
        extends CustomTabHeightStrategy implements ConfigurationChangedObserver {
    private final Activity mActivity;
    private final @Px int mUnclampedInitialHeight;
    private final boolean mIsFixedHeight;
    private final OnResizedCallback mOnResizedCallback;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final FullscreenManager mFullscreenManager;
    private final boolean mIsTablet;
    private final boolean mInteractWithBackground;
    private final PartialCustomTabVersionCompat mVersionCompat;

    private PartialCustomTabBaseStrategy mStrategy;

    public PartialCustomTabDisplayManager(Activity activity, @Px int initialHeight,
            boolean isFixedHeight, OnResizedCallback onResizedCallback,
            ActivityLifecycleDispatcher lifecycleDispatcher, FullscreenManager fullscreenManager,
            boolean isTablet, boolean interactWithBackground) {
        mActivity = activity;
        mUnclampedInitialHeight = initialHeight;
        mIsFixedHeight = isFixedHeight;
        mOnResizedCallback = onResizedCallback;
        mFullscreenManager = fullscreenManager;
        mIsTablet = isTablet;
        mInteractWithBackground = interactWithBackground;

        mActivityLifecycleDispatcher = lifecycleDispatcher;
        lifecycleDispatcher.register(this);

        mVersionCompat = PartialCustomTabVersionCompat.create(mActivity, this::updatePosition);
        mStrategy = createSizeStrategy();
    }

    @PartialCustomTabBaseStrategy.PartialCustomTabType
    public int getActiveStrategyType() {
        return mStrategy.getStrategyType();
    }

    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        mStrategy.onShowSoftInput(softKeyboardRunnable);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mStrategy = createSizeStrategy();
    }

    /**
     * @see {@link org.chromium.chrome.browser.lifecycle.InflationObserver#onPostInflationStartup()}
     */
    @Override
    public void onPostInflationStartup() {
        mStrategy.onPostInflationStartup();
    }

    /**
     * Returns false if we didn't change the Window background color, true otherwise.
     */
    @Override
    public boolean changeBackgroundColorForResizing() {
        return mStrategy.changeBackgroundColorForResizing();
    }

    /**
     * Provide this class with the required views and values so it can set up the strategy.
     *
     * @param coordinatorView Coordinator view to insert the UI handle for the users to resize the
     *                        custom tab.
     * @param toolbar The {@link CustomTabToolbar} to set up the strategy.
     * @param toolbarCornerRadius The custom tab corner radius in pixels.
     */
    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        mStrategy.onToolbarInitialized(coordinatorView, toolbar, toolbarCornerRadius);
    }

    /**
     * @see {@link BaseCustomTabRootUiCoordinator#handleCloseAnimation()}
     */
    @Override
    public void handleCloseAnimation(Runnable finishRunnable) {
        mStrategy.handleCloseAnimation(finishRunnable);
    }

    /**
     * Set the scrim value to apply to partial CCT UI.
     * @param scrimFraction Scrim fraction.
     */
    @Override
    public void setScrimFraction(float scrimFraction) {
        mStrategy.setScrimFraction(scrimFraction);
    }

    // FindToolbarObserver implementation.

    @Override
    public void onFindToolbarShown() {
        mStrategy.onFindToolbarShown();
    }

    @Override
    public void onFindToolbarHidden() {
        mStrategy.onFindToolbarHidden();
    }

    /**
     * Destroy the strategy object.
     */
    @Override
    public void destroy() {
        mStrategy.destroy();
    }

    private @PartialCustomTabBaseStrategy.PartialCustomTabType int calculatePartialCustomTabType() {
        @PartialCustomTabBaseStrategy.PartialCustomTabType
        int type;

        // TODO: This is just placeholder logic used for now just to create a PCCT as a bottom sheet
        // when we are in landscape and side-sheet when in portrait. Will be updated to decide based
        // on breakpoint once that functionality is implemented.
        if (mVersionCompat.getDisplayHeight() > mVersionCompat.getDisplayWidth()) {
            type = PartialCustomTabBaseStrategy.PartialCustomTabType.BOTTOM_SHEET;
        } else {
            type = PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET;
        }

        return type;
    }

    private PartialCustomTabBaseStrategy createSizeStrategy() {
        @PartialCustomTabBaseStrategy.PartialCustomTabType
        int type = calculatePartialCustomTabType();

        switch (type) {
            case PartialCustomTabBaseStrategy.PartialCustomTabType.BOTTOM_SHEET: {
                return new PartialCustomTabHeightStrategy(mActivity, mUnclampedInitialHeight,
                        mIsFixedHeight, mOnResizedCallback, mActivityLifecycleDispatcher,
                        mFullscreenManager, mIsTablet, mInteractWithBackground);
            }
            case PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET: {
                return new PartialCustomTabSideSheetStrategy(mActivity, mUnclampedInitialHeight,
                        mIsFixedHeight, mOnResizedCallback, mFullscreenManager, mIsTablet,
                        mInteractWithBackground);
            }
            default: {
                assert false : "Partial Custom Tab type not supported: " + type;
            }
        }

        return null;
    }

    private void updatePosition() {}
}
