// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.NavigationBarColorProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

import java.util.function.Supplier;

/** Controls the bottom system navigation bar color for the provided {@link Window}. */
@NullMarked
class TabbedNavigationBarColorController
        implements BottomAttachedUiObserver.Observer, NavigationBarColorProvider {
    /** The amount of time transitioning from one color to another should take in ms. */
    public static final long NAVBAR_COLOR_TRANSITION_DURATION_MS = 150;

    private final Context mContext;
    private final FullscreenManager mFullScreenManager;

    // May be null if we return from the constructor early. Otherwise will be set.
    private final @Nullable TabModelSelector mTabModelSelector;
    private final @Nullable TabModelSelectorObserver mTabModelSelectorObserver;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final FullscreenManager.@Nullable Observer mFullscreenObserver;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final ObservableSupplier<Integer> mOverviewColorSupplier;
    private final Callback<Integer> mOnOverviewColorChanged = color -> updateNavigationBarColor();
    private final Callback<EdgeToEdgeController> mEdgeToEdgeRegisterChangeObserverCallback;
    private EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;
    private final BottomAttachedUiObserver mBottomAttachedUiObserver;
    private final TabObserver mTabObserver;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private @Nullable LayoutStateProvider mLayoutManager;
    private @Nullable LayoutStateObserver mLayoutStateObserver;
    private CallbackController mCallbackController = new CallbackController();

    /**
     * The color intended for the navigation bar, as well as any similar UI (such as the bottom chin
     * in edge-to-edge). This may differ from the window navigation bar color when that color is
     * transparent (as in edge-to-edge mode).
     */
    private @ColorInt int mNavigationBarColor;

    /**
     * The target color for the {@link Window}'s navigation bar. This will have a value set during
     * animations, and will be null otherwise.
     */
    private @Nullable @ColorInt Integer mTargetNavigationBarColor;

    private boolean mForceDarkNavigationBarColor;
    private boolean mIsInFullscreen;
    private @ColorInt int mCurrentScrimColor;
    private @Nullable EdgeToEdgeController mEdgeToEdgeController;
    private @Nullable ChangeObserver mEdgeToEdgeChangeObserver;
    private @Nullable Tab mActiveTab;
    private @Nullable @ColorInt Integer mBottomAttachedUiColor;
    private boolean mForceShowDivider;
    private boolean mOverviewMode;
    private @Nullable ValueAnimator mNavbarColorTransitionAnimation;
    private @Nullable Boolean mEnabledBottomChinForTesting;

    /**
     * Creates a new {@link TabbedNavigationBarColorController} instance.
     *
     * @param context Used to load resources.
     * @param tabModelSelector The {@link TabModelSelector} used to determine which tab model is
     *     selected.
     * @param layoutManagerSupplier An {@link ObservableSupplier} for the {@link LayoutManager}
     *     associated with the containing activity.
     * @param fullscreenManager The {@link FullscreenManager} used to determine if fullscreen is
     *     enabled.
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController} to detect when
     *     the UI is being drawn edge to edge so the navigation bar color can be changed
     *     appropriately.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for interacting with and
     *     checking the state of the bottom browser controls.
     * @param browserControlsStateProvider A {@link BrowserControlsStateProvider} to watch for
     *     changes to the browser controls.
     * @param snackbarManagerSupplier Supplies a {@link SnackbarManager} to watch for snackbars
     *     being shown.
     * @param contextualSearchManagerSupplier Supplies a {@link ContextualSearchManager} to watch
     *     for changes to contextual search and the overlay panel.
     * @param bottomSheetController A {@link BottomSheetController} to interact with and watch for
     *     changes to the bottom sheet.
     * @param omniboxSuggestionsVisualState An optional {@link OmniboxSuggestionsVisualState} for
     *     access to the visual state of the omnibox suggestions.
     * @param manualFillingComponentSupplier Supplies the {@link ManualFillingComponent} for
     *     observing the visual state of keyboard accessories.
     * @param overviewColorSupplier Notifies when the overview color changes.
     * @param insetObserver An {@link InsetObserver} to listen for changes to the window insets.
     * @param edgeToEdgeSystemBarColorHelper Helps setting nav bar colors when in edge-to-edge.
     */
    TabbedNavigationBarColorController(
            Context context,
            TabModelSelector tabModelSelector,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            BottomControlsStacker bottomControlsStacker,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            ObservableSupplier<ContextualSearchManager> contextualSearchManagerSupplier,
            BottomSheetController bottomSheetController,
            @Nullable OmniboxSuggestionsVisualState omniboxSuggestionsVisualState,
            ManualFillingComponentSupplier manualFillingComponentSupplier,
            ObservableSupplier<Integer> overviewColorSupplier,
            InsetObserver insetObserver,
            EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper) {
        this(
                context,
                tabModelSelector,
                layoutManagerSupplier,
                fullscreenManager,
                edgeToEdgeControllerSupplier,
                overviewColorSupplier,
                edgeToEdgeSystemBarColorHelper,
                new BottomAttachedUiObserver(
                        bottomControlsStacker,
                        browserControlsStateProvider,
                        snackbarManagerSupplier.get(),
                        contextualSearchManagerSupplier,
                        bottomSheetController,
                        omniboxSuggestionsVisualState,
                        manualFillingComponentSupplier,
                        insetObserver));
    }

    @VisibleForTesting
    TabbedNavigationBarColorController(
            Context context,
            TabModelSelector tabModelSelector,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ObservableSupplier<Integer> overviewColorSupplier,
            EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper,
            BottomAttachedUiObserver bottomAttachedUiObserver) {
        mContext = context;
        mFullScreenManager = fullscreenManager;
        mEdgeToEdgeSystemBarColorHelper = edgeToEdgeSystemBarColorHelper;

        mBottomAttachedUiObserver = bottomAttachedUiObserver;
        mBottomAttachedUiObserver.addObserver(this);

        mTabModelSelector = tabModelSelector;
        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onChange() {
                        updateActiveTab();
                    }
                };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        mCurrentTabModelObserver = (tabModel) -> updateNavigationBarColor();
        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onBackgroundColorChanged(Tab tab, int color) {
                        updateNavigationBarColor(
                                /* forceShowDivider= */ false, /* disableAnimation= */ false);
                    }
                };
        mFullscreenObserver =
                new FullscreenManager.Observer() {
                    @Override
                    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                        mIsInFullscreen = true;
                        updateNavigationBarColor();
                    }

                    @Override
                    public void onExitFullscreen(Tab tab) {
                        mIsInFullscreen = false;
                        updateNavigationBarColor();
                    }
                };
        mFullScreenManager.addObserver(mFullscreenObserver);
        layoutManagerSupplier.addObserver(
                mCallbackController.makeCancelable(this::setLayoutManager));

        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mEdgeToEdgeRegisterChangeObserverCallback =
                (controller) -> {
                    if (mEdgeToEdgeController != null) {
                        mEdgeToEdgeController.unregisterObserver(
                                assumeNonNull(mEdgeToEdgeChangeObserver));
                    }
                    mEdgeToEdgeController = controller;
                    mEdgeToEdgeChangeObserver =
                            (bottomInset, isDrawingToEdge, isPageOptInToEdge) -> {
                                updateNavigationBarColor(
                                        /* forceShowDivider= */ false,
                                        /* disableAnimation= */ false);
                            };
                    mEdgeToEdgeController.registerObserver(mEdgeToEdgeChangeObserver);
                };
        mEdgeToEdgeControllerSupplier.addObserver(mEdgeToEdgeRegisterChangeObserverCallback);

        mOverviewColorSupplier = overviewColorSupplier;
        mOverviewColorSupplier.addObserver(mOnOverviewColorChanged);
        mOverviewMode = false;

        // TODO(crbug.com/40560014): Observe tab loads to restrict black bottom nav to
        // incognito NTP.

        updateNavigationBarColor();
    }

    /** Destroy this {@link TabbedNavigationBarColorController} instance. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
        if (mActiveTab != null) mActiveTab.removeObserver(mTabObserver);
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(mLayoutStateObserver);
            mOverviewColorSupplier.removeObserver(mOnOverviewColorChanged);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        mFullScreenManager.removeObserver(mFullscreenObserver);
        if (mEdgeToEdgeControllerSupplier.get() != null && mEdgeToEdgeChangeObserver != null) {
            mEdgeToEdgeControllerSupplier.get().unregisterObserver(mEdgeToEdgeChangeObserver);
            mEdgeToEdgeChangeObserver = null;
        }
        mEdgeToEdgeControllerSupplier.removeObserver(mEdgeToEdgeRegisterChangeObserverCallback);
        mBottomAttachedUiObserver.removeObserver(this);
        mBottomAttachedUiObserver.destroy();

        if (mNavbarColorTransitionAnimation != null) {
            mNavbarColorTransitionAnimation.cancel();
        }
    }

    @Override
    public void onBottomAttachedColorChanged(
            @ColorInt @Nullable Integer color, boolean forceShowDivider, boolean disableAnimation) {
        mBottomAttachedUiColor = color;
        updateNavigationBarColor(forceShowDivider, disableAnimation);
    }

    /**
     * @param layoutManager The {@link LayoutStateProvider} used to determine whether overview mode
     *     is showing.
     */
    private void setLayoutManager(LayoutManager layoutManager) {
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(assumeNonNull(mLayoutStateObserver));
        }

        mLayoutManager = layoutManager;
        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            updateNavigationBarColor();
                            enableOverviewMode();
                        } else if (layoutType == LayoutType.TOOLBAR_SWIPE
                                && ChromeFeatureList.sNavBarColorAnimation.isEnabled()
                                && isBottomChinEnabled()) {
                            // Hide the nav bar during omnibox swipes.
                            mNavigationBarColor = Color.TRANSPARENT;
                            mEdgeToEdgeSystemBarColorHelper.setNavigationBarColor(
                                    Color.TRANSPARENT);
                            mEdgeToEdgeSystemBarColorHelper.setNavigationBarDividerColor(
                                    Color.TRANSPARENT);
                        }
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType != LayoutType.TAB_SWITCHER) return;
                        updateNavigationBarColor();
                        disableOverviewMode();
                    }

                    @Override
                    public void onFinishedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            updateNavigationBarColor();
                        }
                    }
                };
        mLayoutManager.addObserver(mLayoutStateObserver);
        updateNavigationBarColor();
    }

    private void updateActiveTab() {
        @Nullable Tab activeTab = assumeNonNull(mTabModelSelector).getCurrentTab();
        if (activeTab == mActiveTab) return;

        if (mActiveTab != null) mActiveTab.removeObserver(mTabObserver);
        mActiveTab = activeTab;
        if (mActiveTab != null) mActiveTab.addObserver(mTabObserver);

        // Do not update the navigation bar color if the device is in the middle of a toolbar swipe
        // or animation, this will lead to incorrect colors flashing in the middle of the
        // transition. Later calls to #updateNavigationBarColor() will properly update the color
        // after the swipe is complete.
        if (mLayoutManager != null && mLayoutManager.getActiveLayoutType() == LayoutType.BROWSING) {
            updateNavigationBarColor(/* forceShowDivider= */ false, /* disableAnimation= */ false);
        }
    }

    @SuppressLint("NewApi")
    private void updateNavigationBarColor(boolean forceShowDivider, boolean disableAnimation) {
        assumeNonNull(mTabModelSelector);
        mForceDarkNavigationBarColor = mTabModelSelector.isIncognitoSelected() || mIsInFullscreen;

        final @ColorInt int newNavigationBarColor =
                applyCurrentScrimToColor(getNavigationBarColor(mForceDarkNavigationBarColor));
        if (!disableAnimation
                && mTargetNavigationBarColor != null
                && mTargetNavigationBarColor.equals(newNavigationBarColor)
                && mForceShowDivider == forceShowDivider) {
            return;
        }

        final @ColorInt int currentNavigationBarColor = mNavigationBarColor;
        final @ColorInt int newNavigationBarDividerColor =
                applyCurrentScrimToColor(
                        getNavigationBarDividerColor(
                                mForceDarkNavigationBarColor, forceShowDivider));

        mNavigationBarColor = newNavigationBarColor;
        mForceShowDivider = forceShowDivider;

        endNavigationBarColorAnimationIfRunning();
        if (shouldEnableNavBarBottomChinColorAnimations() && !disableAnimation) {
            animateNavigationBarColor(currentNavigationBarColor, newNavigationBarColor);
        } else {
            mEdgeToEdgeSystemBarColorHelper.setNavigationBarColor(newNavigationBarColor);
            mEdgeToEdgeSystemBarColorHelper.setNavigationBarDividerColor(
                    newNavigationBarDividerColor);
        }
    }

    private void endNavigationBarColorAnimationIfRunning() {
        if (mNavbarColorTransitionAnimation != null
                && mNavbarColorTransitionAnimation.isRunning()) {
            mNavbarColorTransitionAnimation.end();
        }
    }

    private void animateNavigationBarColor(
            @ColorInt int currentNavigationBarColor, @ColorInt int newNavigationBarColor) {
        mNavbarColorTransitionAnimation =
                ValueAnimator.ofFloat(0, 1).setDuration(NAVBAR_COLOR_TRANSITION_DURATION_MS);
        mNavbarColorTransitionAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        mTargetNavigationBarColor = newNavigationBarColor;

        mNavbarColorTransitionAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mTargetNavigationBarColor = null;
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mTargetNavigationBarColor = null;
                    }
                });
        mNavbarColorTransitionAnimation.addUpdateListener(
                (ValueAnimator animation) -> {
                    assert mTargetNavigationBarColor != null;

                    float fraction = animation.getAnimatedFraction();
                    int blendedColor =
                            ColorUtils.blendColorsMultiply(
                                    currentNavigationBarColor, mTargetNavigationBarColor, fraction);
                    int dividerColor =
                            mForceShowDivider
                                    ? getNavigationBarDividerColor(
                                            mForceDarkNavigationBarColor, mForceShowDivider)
                                    : blendedColor;
                    mEdgeToEdgeSystemBarColorHelper.setNavigationBarColor(blendedColor);
                    mEdgeToEdgeSystemBarColorHelper.setNavigationBarDividerColor(dividerColor);
                });
        mNavbarColorTransitionAnimation.start();
    }

    @SuppressLint("NewApi")
    private void updateNavigationBarColor() {
        updateNavigationBarColor(/* forceShowDivider= */ false, /* disableAnimation= */ false);
    }

    /**
     * Update the scrim color on the navigation bar.
     *
     * @param scrimColor The scrim color currently affecting the nav bar, including alpha.
     */
    public void setNavigationBarScrimColor(@ColorInt int scrimColor) {
        mCurrentScrimColor = scrimColor;
        updateNavigationBarColor();
    }

    @ColorInt
    private int getNavigationBarColor(boolean forceDarkNavigationBar) {
        if (mOverviewMode && mOverviewColorSupplier.get() != null) {
            return mOverviewColorSupplier.get();
        } else if (useBottomAttachedUiColor()) {
            return mBottomAttachedUiColor;
        } else if (useActiveTabColor()) {
            return mActiveTab.getBackgroundColor();
        } else {
            return forceDarkNavigationBar
                    ? mContext.getColor(R.color.toolbar_background_primary_dark)
                    : SemanticColorUtils.getBottomSystemNavColor(mContext);
        }
    }

    @VisibleForTesting
    @ColorInt
    int getNavigationBarDividerColor(boolean forceDarkNavigationBar, boolean forceShowDivider) {
        if (mOverviewMode && mOverviewColorSupplier.get() != null) {
            return mOverviewColorSupplier.get();
        } else if (!forceShowDivider && useBottomAttachedUiColor()) {
            return mBottomAttachedUiColor;
        } else if (!forceShowDivider && useActiveTabColor()) {
            return mActiveTab.getBackgroundColor();
        } else {
            return forceDarkNavigationBar
                    ? mContext.getColor(R.color.bottom_system_nav_divider_color_light)
                    : SemanticColorUtils.getBottomSystemNavDividerColor(mContext);
        }
    }

    private @ColorInt int applyCurrentScrimToColor(@ColorInt int color) {
        return ColorUtils.overlayColor(color, mCurrentScrimColor);
    }

    @VisibleForTesting
    void enableOverviewMode() {
        mOverviewMode = true;
    }

    @VisibleForTesting
    void disableOverviewMode() {
        mOverviewMode = false;
    }

    @EnsuresNonNullIf("mBottomAttachedUiColor")
    private boolean useBottomAttachedUiColor() {
        return mBottomAttachedUiColor != null;
    }

    @EnsuresNonNullIf("mActiveTab")
    private boolean useActiveTabColor() {
        return mLayoutManager != null
                && mLayoutManager.getActiveLayoutType() == LayoutType.BROWSING
                && mActiveTab != null;
    }

    void updateActiveTabForTesting() {
        updateActiveTab();
    }

    boolean getUseActiveTabColorForTesting() {
        return useActiveTabColor();
    }

    boolean getUseBottomAttachedUiColorForTesting() {
        return useBottomAttachedUiColor();
    }

    public void setEdgeToEdgeSystemBarColorHelperForTesting(EdgeToEdgeSystemBarColorHelper helper) {
        mEdgeToEdgeSystemBarColorHelper = helper;
    }

    public @Nullable ValueAnimator getNavbarColorTransitionAnimationForTesting() {
        return mNavbarColorTransitionAnimation;
    }

    public void setIsBottomChinEnabledForTesting(boolean isEnabled) {
        mEnabledBottomChinForTesting = isEnabled;
    }

    private boolean shouldEnableNavBarBottomChinColorAnimations() {
        // First check the dedicated feature flag.
        if (!ChromeFeatureList.sNavBarColorAnimation.isEnabled()) {
            return false;
        }
        // Next check whether the bottom chin is enabled.
        if (isBottomChinEnabled() && mEdgeToEdgeControllerSupplier.get() != null) {
            return !ChromeFeatureList.sNavBarColorAnimationDisableBottomChinColorAnimation
                    .getValue();
        }
        // Then check whether e2e everywhere is enabled.
        if (EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled()) {
            return !ChromeFeatureList.sNavBarColorAnimationDisableEdgeToEdgeLayoutColorAnimation
                    .getValue();
        }
        // Disable animations.
        return false;
    }

    private boolean isBottomChinEnabled() {
        if (mEnabledBottomChinForTesting != null) {
            return mEnabledBottomChinForTesting;
        }

        return mContext instanceof Activity
                && EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled((Activity) mContext);
    }

    @Override
    public int getNavigationBarColor() {
        return mNavigationBarColor;
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
        observer.onNavigationBarColorChanged(mNavigationBarColor);
        observer.onNavigationBarDividerChanged(mNavigationBarColor);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }
}
