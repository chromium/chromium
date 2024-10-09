// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_BOTTOM;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Handler;
import android.util.SparseBooleanArray;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.AnimRes;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventProvider;

/**
 * Class responsible for how the Partial Chrome Custom Tabs are displayed on the screen.
 * It creates and handles the supported size strategies for Partial Chrome Custom Tabs based on the
 * intent extras values provided by the embedder, the window size, and the device state.
 */
public class PartialCustomTabDisplayManager extends CustomTabHeightStrategy
        implements ConfigurationChangedObserver {
    static final int CREATE_STRATEGY_DELAY_CONFIG_CHANGE_MS = 150;
    static final int WINDOW_WIDTH_COMPACT_CUTOFF_DP = 600;

    private final Activity mActivity;
    private final BrowserServicesIntentDataProvider mIntentData;
    private final int mBreakPointDp;
    private final OnResizedCallback mOnResizedCallback;
    private final OnActivityLayoutCallback mOnActivityLayoutCallback;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final FullscreenManager mFullscreenManager;
    private final boolean mIsTablet;
    private final PartialCustomTabVersionCompat mVersionCompat;
    private final SparseBooleanArray mLastMaximizeState = new SparseBooleanArray();

    // Simple factory interface creating a new SizeStrategy. Facilitates testing.
    interface SizeStrategyCreator {
        PartialCustomTabBaseStrategy createForType(
                @PartialCustomTabType int type,
                BrowserServicesIntentDataProvider intentData,
                boolean startMaximized);
    }

    private PartialCustomTabBaseStrategy mStrategy;
    private @PartialCustomTabType int mCurrentPartialCustomTabType;

    private View mToolbarCoordinatorView;
    private CustomTabToolbar mCustomTabToolbar;
    private int mToolbarCornerRadius;
    private PartialCustomTabHandleStrategyFactory mHandleStrategyFactory;
    private SizeStrategyCreator mSizeStrategyCreator = this::createSizeStrategy;
    private Supplier<TouchEventProvider> mTouchEventProvider;
    private Supplier<Tab> mTab;

    public PartialCustomTabDisplayManager(
            Activity activity,
            BrowserServicesIntentDataProvider intentData,
            Supplier<TouchEventProvider> touchEventProvider,
            Supplier<Tab> tab,
            OnResizedCallback onResizedCallback,
            OnActivityLayoutCallback onActivityLayoutCallback,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            FullscreenManager fullscreenManager,
            boolean isTablet) {
        mActivity = activity;
        mIntentData = intentData;
        mTouchEventProvider = touchEventProvider;
        mTab = tab;
        mOnResizedCallback = onResizedCallback;
        mOnActivityLayoutCallback = onActivityLayoutCallback;
        mFullscreenManager = fullscreenManager;
        mIsTablet = isTablet;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        lifecycleDispatcher.register(this);

        mVersionCompat = PartialCustomTabVersionCompat.create(mActivity, this::updatePosition);
        mHandleStrategyFactory = new PartialCustomTabHandleStrategyFactory();
        mBreakPointDp = calculateBreakPoint(intentData.getActivityBreakPoint());
        mCurrentPartialCustomTabType = calculatePartialCustomTabType();
        mStrategy =
                mSizeStrategyCreator.createForType(mCurrentPartialCustomTabType, intentData, false);
    }

    public @PartialCustomTabType int getActiveStrategyType() {
        return mStrategy.getStrategyType();
    }

    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        mStrategy.onShowSoftInput(softKeyboardRunnable);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        int type = calculatePartialCustomTabType();
        if (type != mCurrentPartialCustomTabType) {
            if (mStrategy != null) {
                mStrategy.destroy(); // May update the internal states.
                mLastMaximizeState.put(mStrategy.getStrategyType(), mStrategy.isMaximized());
            }
            boolean startMaximized = mLastMaximizeState.get(type, false);
            mStrategy = mSizeStrategyCreator.createForType(type, mIntentData, startMaximized);
            mCurrentPartialCustomTabType = type;
            mStrategy.setToolbar(mToolbarCoordinatorView, mCustomTabToolbar);
            new Handler()
                    .postDelayed(
                            () -> {
                                mStrategy.onToolbarInitialized(
                                        mToolbarCoordinatorView,
                                        mCustomTabToolbar,
                                        mToolbarCornerRadius);
                                mStrategy.onPostInflationStartup();
                                // TODO(http://crbug.com/1406107): Creating a new strategy type is
                                // basically a resize so we need to make sure to call
                                // #onActivityResized here as
                                // well
                            },
                            CREATE_STRATEGY_DELAY_CONFIG_CHANGE_MS);
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

    /** Returns false if we didn't change the Window background color, true otherwise. */
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
    public boolean handleCloseAnimation(Runnable finishRunnable) {
        return mStrategy.handleCloseAnimation(finishRunnable);
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

    /** Destroy the strategy object. */
    @Override
    public void destroy() {
        mStrategy.destroy();
    }

    private static int calculateBreakPoint(int unclampedBreakPointDp) {
        return Math.max(unclampedBreakPointDp, WINDOW_WIDTH_COMPACT_CUTOFF_DP);
    }

    private @PartialCustomTabType int calculatePartialCustomTabType() {
        int initialWidth = mIntentData.getInitialActivityWidth();
        int initialHeight = mIntentData.getInitialActivityHeight();
        return calculatePartialCustomTabType(
                mActivity,
                initialWidth,
                initialHeight,
                mVersionCompat::getDisplayWidthDp,
                mBreakPointDp);
    }

    @VisibleForTesting
    static @PartialCustomTabType int calculatePartialCustomTabType(
            Activity activity,
            int initialWidth,
            int initialHeight,
            Supplier<Integer> displayWidthDpSupplier,
            int breakPointDp) {
        if (MultiWindowUtils.getInstance().isInMultiWindowMode(activity)) {
            return PartialCustomTabType.FULL_SIZE;
        }
        if (initialWidth == 0 && initialHeight == 0) {
            return PartialCustomTabType.FULL_SIZE;
        }
        int displayWidthDp = -1;
        if (initialWidth > 0 && initialHeight > 0) {
            if (displayWidthDp < 0) displayWidthDp = displayWidthDpSupplier.get();
            return displayWidthDp < breakPointDp
                    ? PartialCustomTabType.BOTTOM_SHEET
                    : PartialCustomTabType.SIDE_SHEET;
        }
        if (initialWidth > 0) {
            if (displayWidthDp < 0) displayWidthDp = displayWidthDpSupplier.get();
            return displayWidthDp < breakPointDp
                    ? PartialCustomTabType.FULL_SIZE
                    : PartialCustomTabType.SIDE_SHEET;
        }
        if (initialHeight > 0) {
            return PartialCustomTabType.BOTTOM_SHEET;
        }
        assert false : "Unreachable";
        return PartialCustomTabType.FULL_SIZE;
    }

    /**
     * Get the start animation resource ID to override the default with.
     * @param activity Activity to get window resource from.
     * @param provider Intent data provider from which to extract necessary info.
     * @param defaultResId Default start animation resource ID.
     * @return Start resource ID if an override was found, or the default one if not.
     */
    public static @AnimRes int getStartAnimationOverride(
            Activity activity,
            BrowserServicesIntentDataProvider provider,
            @AnimRes int defaultResId) {
        // Initialize VersionCompat lazily using a supplier since in many cases (for normal CCTs)
        // |calculatePartialCustomTabType| won't need the object and will early out.
        Supplier<Integer> displayWidthDpSupplier =
                () -> {
                    var versionCompat =
                            PartialCustomTabVersionCompat.create(
                                    activity, CallbackUtils.emptyRunnable());
                    return versionCompat.getDisplayWidthDp();
                };
        @PartialCustomTabType
        int type =
                calculatePartialCustomTabType(
                        activity,
                        provider.getInitialActivityWidth(),
                        provider.getInitialActivityHeight(),
                        displayWidthDpSupplier,
                        calculateBreakPoint(provider.getActivityBreakPoint()));

        @AnimRes int start_anim_id = defaultResId;
        if (type == PartialCustomTabType.BOTTOM_SHEET || type == PartialCustomTabType.FULL_SIZE) {
            start_anim_id = R.anim.slide_in_up;
        } else if (type == PartialCustomTabType.SIDE_SHEET) {
            int behavior = provider.getSideSheetSlideInBehavior();
            if (behavior == ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_BOTTOM) {
                start_anim_id = R.anim.slide_in_up;
            } else if (behavior == ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE) {
                boolean sheetOnRight =
                        PartialCustomTabSideSheetStrategy.isSheetOnRight(
                                provider.getSideSheetPosition());
                start_anim_id = sheetOnRight ? R.anim.slide_in_right : R.anim.slide_in_left;
            } else {
                assert false : "Invalide slide-in behavior";
            }
        }
        return start_anim_id;
    }

    private PartialCustomTabBaseStrategy createSizeStrategy(
            @PartialCustomTabType int type,
            BrowserServicesIntentDataProvider intentData,
            boolean maximized) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.PartialCustomTabType", type, PartialCustomTabType.COUNT);

        return switch (type) {
            case PartialCustomTabType.BOTTOM_SHEET -> new PartialCustomTabBottomSheetStrategy(
                    mActivity,
                    mIntentData,
                    mTouchEventProvider,
                    mTab,
                    mOnResizedCallback,
                    mOnActivityLayoutCallback,
                    mActivityLifecycleDispatcher,
                    mFullscreenManager,
                    mIsTablet,
                    maximized,
                    mHandleStrategyFactory);
            case PartialCustomTabType.SIDE_SHEET -> new PartialCustomTabSideSheetStrategy(
                    mActivity,
                    mIntentData,
                    mOnResizedCallback,
                    mOnActivityLayoutCallback,
                    mFullscreenManager,
                    mIsTablet,
                    maximized,
                    mHandleStrategyFactory);
            case PartialCustomTabType.FULL_SIZE -> new PartialCustomTabFullSizeStrategy(
                    mActivity,
                    mIntentData,
                    mOnResizedCallback,
                    mOnActivityLayoutCallback,
                    mFullscreenManager,
                    mIsTablet,
                    mHandleStrategyFactory);
            default -> {
                assert false : "Partial Custom Tab type not supported: " + type;
                yield null;
            }
        };
    }

    private void updatePosition() {}

    SizeStrategyCreator getSizeStrategyCreatorForTesting() {
        return mSizeStrategyCreator;
    }

    PartialCustomTabBaseStrategy getSizeStrategyForTesting() {
        return mStrategy;
    }

    int getBreakPointDpForTesting() {
        return mBreakPointDp;
    }

    void setMocksForTesting(
            ViewGroup coordinatorLayout,
            CustomTabToolbar toolbar,
            View toolbarCoordinator,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory,
            SizeStrategyCreator sizeStrategyCreator) {
        mToolbarCoordinatorView = toolbarCoordinator;
        mCustomTabToolbar = toolbar;
        mHandleStrategyFactory = handleStrategyFactory;
        mSizeStrategyCreator = sizeStrategyCreator;
        mStrategy.setMockViewForTesting(coordinatorLayout, toolbar, toolbarCoordinator);
    }
}
