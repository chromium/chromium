// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Handler;
import android.view.View;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
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
    static final int CREATE_STRATEGY_DELAY_CONFIG_CHANGE_MS = 150;

    private final Activity mActivity;
    private final int mBreakPointDp;
    private final @Px int mUnclampedInitialHeight;
    private final @Px int mUnclampedInitialWidth;
    private final boolean mIsFixedHeight;
    private final OnResizedCallback mOnResizedCallback;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final FullscreenManager mFullscreenManager;
    private final boolean mIsTablet;
    private final boolean mInteractWithBackground;
    private final PartialCustomTabVersionCompat mVersionCompat;

    // Simple factory interface creating a new SizeStrategy. Facilitates testing.
    interface SizeStrategyCreator {
        PartialCustomTabBaseStrategy createForType(@PartialCustomTabType int type);
    }

    private PartialCustomTabBaseStrategy mStrategy;
    private @PartialCustomTabType int mCurrentPartialCustomTabType;

    private View mToolbarCoordinatorView;
    private CustomTabToolbar mCustomTabToolbar;
    private int mToolbarCornerRadius;
    private PartialCustomTabHandleStrategyFactory mHandleStrategyFactory;
    private SizeStrategyCreator mSizeStrategyCreator = this::createSizeStrategy;

    public PartialCustomTabDisplayManager(Activity activity, @Px int initialHeight,
            @Px int initialWidth, int breakPointDp, boolean isFixedHeight,
            OnResizedCallback onResizedCallback, ActivityLifecycleDispatcher lifecycleDispatcher,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground) {
        mActivity = activity;
        mUnclampedInitialHeight = initialHeight;
        mUnclampedInitialWidth = initialWidth;
        mBreakPointDp = breakPointDp;
        mIsFixedHeight = isFixedHeight;
        mOnResizedCallback = onResizedCallback;
        mFullscreenManager = fullscreenManager;
        mIsTablet = isTablet;
        mInteractWithBackground = interactWithBackground;

        mActivityLifecycleDispatcher = lifecycleDispatcher;
        lifecycleDispatcher.register(this);

        mVersionCompat = PartialCustomTabVersionCompat.create(mActivity, this::updatePosition);
        mHandleStrategyFactory = new PartialCustomTabHandleStrategyFactory();
        mCurrentPartialCustomTabType = calculatePartialCustomTabType();
        mStrategy = mSizeStrategyCreator.createForType(mCurrentPartialCustomTabType);
    }

    @PartialCustomTabType
    public int getActiveStrategyType() {
        return mStrategy.getStrategyType();
    }

    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        mStrategy.onShowSoftInput(softKeyboardRunnable);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        int type = calculatePartialCustomTabType();
        if (type != mCurrentPartialCustomTabType) {
            new Handler().postDelayed(() -> {
                if (mStrategy != null) mStrategy.destroy();
                mStrategy = mSizeStrategyCreator.createForType(type);
                mCurrentPartialCustomTabType = type;
                mStrategy.onToolbarInitialized(
                        mToolbarCoordinatorView, mCustomTabToolbar, mToolbarCornerRadius);
                mStrategy.onPostInflationStartup();
            }, CREATE_STRATEGY_DELAY_CONFIG_CHANGE_MS);
        } else {
            // If the type of PCCT strategy did not change we can just call into the equivalent
            // method for the given strategy.
            mStrategy.onConfigurationChanged(newConfig.orientation);
        }
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
        mToolbarCoordinatorView = coordinatorView;
        mCustomTabToolbar = toolbar;
        mToolbarCornerRadius = toolbarCornerRadius;

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

    private @PartialCustomTabType int calculatePartialCustomTabType() {
        int displayWidthDp = mVersionCompat.getDisplayWidthDp();

        if (mUnclampedInitialWidth == 0 && mUnclampedInitialHeight == 0) {
            return PartialCustomTabType.FULL_SIZE;
        }

        if (mUnclampedInitialWidth > 0 && mUnclampedInitialHeight > 0) {
            return displayWidthDp < mBreakPointDp ? PartialCustomTabType.BOTTOM_SHEET
                                                  : PartialCustomTabType.SIDE_SHEET;
        }

        if (mUnclampedInitialWidth > 0) {
            return displayWidthDp < mBreakPointDp ? PartialCustomTabType.FULL_SIZE
                                                  : PartialCustomTabType.SIDE_SHEET;
        }

        if (mUnclampedInitialHeight > 0) {
            return PartialCustomTabType.BOTTOM_SHEET;
        }

        return PartialCustomTabType.FULL_SIZE; // unreachable
    }

    private PartialCustomTabBaseStrategy createSizeStrategy(@PartialCustomTabType int type) {
        switch (type) {
            case PartialCustomTabType.BOTTOM_SHEET: {
                return new PartialCustomTabHeightStrategy(mActivity, mUnclampedInitialHeight,
                        mIsFixedHeight, mOnResizedCallback, mActivityLifecycleDispatcher,
                        mFullscreenManager, mIsTablet, mInteractWithBackground,
                        mHandleStrategyFactory);
            }
            case PartialCustomTabType.SIDE_SHEET: {
                return new PartialCustomTabSideSheetStrategy(mActivity, mUnclampedInitialWidth,
                        mOnResizedCallback, mFullscreenManager, mIsTablet, mInteractWithBackground,
                        mHandleStrategyFactory);
            }
            default: {
                assert false : "Partial Custom Tab type not supported: " + type;
            }
        }

        return null;
    }

    private void updatePosition() {}

    @VisibleForTesting
    SizeStrategyCreator getSizeStrategyCreatorForTesting() {
        return mSizeStrategyCreator;
    }

    @VisibleForTesting
    PartialCustomTabBaseStrategy getSizeStrategyForTesting() {
        return mStrategy;
    }

    @VisibleForTesting
    void setMocksForTesting(View toolbar, CustomTabToolbar customTabToolbar,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory,
            SizeStrategyCreator sizeStrategyCreator) {
        mToolbarCoordinatorView = toolbar;
        mCustomTabToolbar = customTabToolbar;
        mHandleStrategyFactory = handleStrategyFactory;
        mSizeStrategyCreator = sizeStrategyCreator;
        mStrategy.setMockViewForTesting(toolbar, customTabToolbar);
    }
}
