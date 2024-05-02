// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.view.ViewGroup;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

/** Controls the bottom system navigation bar color for the provided {@link Window}. */
@RequiresApi(Build.VERSION_CODES.O_MR1)
class TabbedNavigationBarColorController implements BottomAttachedUiObserver.Observer {
    /** The amount of time transitioning from one color to another should take in ms. */
    public static final long NAVBAR_COLOR_TRANSITION_DURATION_MS = 250;

    private static final String TAG = "NavBarColorCntrller";
    private final Window mWindow;
    private final ViewGroup mRootView;
    private final Context mContext;
    private final FullscreenManager mFullScreenManager;
    private final @ColorInt int mDefaultScrimColor;
    private final boolean mLightNavigationBar;

    // May be null if we return from the constructor early. Otherwise will be set.
    private final @Nullable TabModelSelector mTabModelSelector;
    private final @Nullable TabModelSelectorObserver mTabModelSelectorObserver;
    private final @Nullable FullscreenManager.Observer mFullscreenObserver;
    private @Nullable LayoutStateProvider mLayoutManager;
    private @Nullable LayoutStateObserver mLayoutStateObserver;
    private CallbackController mCallbackController = new CallbackController();

    private @ColorInt int mNavigationBarColor;
    private boolean mForceDarkNavigationBarColor;
    private boolean mIsInFullscreen;
    private float mNavigationBarScrimFraction;

    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final Callback<EdgeToEdgeController> mEdgeToEdgeRegisterChangeObserverCallback;
    private EdgeToEdgeController mEdgeToEdgeController;
    @Nullable private ChangeObserver mEdgeToEdgeChangeObserver;
    private final BottomAttachedUiObserver mBottomAttachedUiObserver;

    private @Nullable Tab mActiveTab;
    private TabObserver mTabObserver;
    @Nullable private @ColorInt Integer mBottomAttachedUiColor;

    private ValueAnimator mNavbarColorTransitionAnimation;

    /**
     * Creates a new {@link TabbedNavigationBarColorController} instance.
     *
     * @param window The {@link Window} this controller should operate on.
     * @param tabModelSelector The {@link TabModelSelector} used to determine which tab model is
     *     selected.
     * @param layoutManagerSupplier An {@link ObservableSupplier} for the {@link LayoutManager}
     *     associated with the containing activity.
     * @param fullscreenManager The {@link FullscreenManager} used to determine if fullscreen is
     *     enabled.
     * @param edgeToEdgeControllerSupplier Supplies an {@link EdgeToEdgeController} to detect when
     *     the UI is being drawn edge to edge so the navigation bar color can be changed
     *     appropriately.
     * @param browserControlsStateProvider A {@link BrowserControlsStateProvider} to watch for
     *     changes to the browser controls.
     * @param snackbarManagerSupplier Supplies a {@link SnackbarManager} to watch for snackbars
     *     being shown.
     * @param contextualSearchManagerSupplier Supplies a {@link ContextualSearchManager} to watch
     *     for changes to contextual search and the overlay panel.
     * @param bottomSheetController A {@link BottomSheetController} to interact with and watch for
     *     changes to the bottom sheet.
     */
    TabbedNavigationBarColorController(
            Window window,
            TabModelSelector tabModelSelector,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull ObservableSupplier<ContextualSearchManager> contextualSearchManagerSupplier,
            BottomSheetController bottomSheetController) {
        this(
                window,
                tabModelSelector,
                layoutManagerSupplier,
                fullscreenManager,
                edgeToEdgeControllerSupplier,
                new BottomAttachedUiObserver(
                        browserControlsStateProvider,
                        snackbarManagerSupplier.get(),
                        contextualSearchManagerSupplier,
                        bottomSheetController));
    }

    @VisibleForTesting
    TabbedNavigationBarColorController(
            Window window,
            TabModelSelector tabModelSelector,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            BottomAttachedUiObserver bottomAttachedUiObserver) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1;

        mWindow = window;
        mRootView = (ViewGroup) mWindow.getDecorView().getRootView();
        mContext = mRootView.getContext();
        mDefaultScrimColor = mContext.getColor(R.color.default_scrim_color);
        mFullScreenManager = fullscreenManager;
        mLightNavigationBar =
                mContext.getResources().getBoolean(R.bool.window_light_navigation_bar);

        mBottomAttachedUiObserver = bottomAttachedUiObserver;
        mBottomAttachedUiObserver.addObserver(this);

        mTabModelSelector = tabModelSelector;
        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                        updateNavigationBarColor();
                    }

                    @Override
                    public void onChange() {
                        updateActiveTab();
                    }
                };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onBackgroundColorChanged(Tab tab, int color) {
                        updateNavigationBarColor(getBottomInset());
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
                        mEdgeToEdgeController.unregisterObserver(mEdgeToEdgeChangeObserver);
                    }
                    mEdgeToEdgeController = controller;
                    mEdgeToEdgeChangeObserver =
                            (bottomInset) -> {
                                updateNavigationBarColor(bottomInset);
                            };
                    mEdgeToEdgeController.registerObserver(mEdgeToEdgeChangeObserver);
                };
        mEdgeToEdgeControllerSupplier.addObserver(mEdgeToEdgeRegisterChangeObserverCallback);

        // TODO(crbug.com/40560014): Observe tab loads to restrict black bottom nav to
        // incognito NTP.

        updateNavigationBarColor();
    }

    /** Destroy this {@link TabbedNavigationBarColorController} instance. */
    public void destroy() {
        if (mTabModelSelector != null) mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        if (mActiveTab != null) mActiveTab.removeObserver(mTabObserver);
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(mLayoutStateObserver);
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
        if (mBottomAttachedUiObserver != null) {
            mBottomAttachedUiObserver.removeObserver(this);
            mBottomAttachedUiObserver.destroy();
        }
        if (mNavbarColorTransitionAnimation != null) {
            mNavbarColorTransitionAnimation.cancel();
        }
    }

    @Override
    public void onBottomAttachedColorChanged(@Nullable @ColorInt Integer color) {
        mBottomAttachedUiColor = color;
        updateNavigationBarColor();
    }

    /**
     * @param layoutManager The {@link LayoutStateProvider} used to determine whether overview mode
     *     is showing.
     */
    private void setLayoutManager(LayoutManager layoutManager) {
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(mLayoutStateObserver);
        }

        mLayoutManager = layoutManager;
        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType != LayoutType.TAB_SWITCHER) return;
                        updateNavigationBarColor();
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType != LayoutType.TAB_SWITCHER) return;
                        updateNavigationBarColor();
                    }

                    @Override
                    public void onFinishedShowing(@LayoutType int layoutType) {
                        if (ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()
                                && layoutType == LayoutType.BROWSING) {
                            updateNavigationBarColor();
                        }
                    }
                };
        mLayoutManager.addObserver(mLayoutStateObserver);
        updateNavigationBarColor();
    }

    private void updateActiveTab() {
        if (!ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()) return;

        @Nullable Tab activeTab = mTabModelSelector.getCurrentTab();
        if (activeTab == mActiveTab) return;

        if (mActiveTab != null) mActiveTab.removeObserver(mTabObserver);
        mActiveTab = activeTab;
        if (mActiveTab != null) mActiveTab.addObserver(mTabObserver);
        updateNavigationBarColor(getBottomInset());
    }

    @SuppressLint("NewApi")
    private void updateNavigationBarColor(@Nullable Integer bottomInset) {
        boolean toEdge = bottomInset != null && bottomInset != 0;
        boolean forceDarkNavigation = mTabModelSelector.isIncognitoSelected();

        forceDarkNavigation &= !UiUtils.isSystemUiThemingDisabled();
        forceDarkNavigation |= mIsInFullscreen;

        mForceDarkNavigationBarColor = forceDarkNavigation;
        final @ColorInt int newNavigationBarColor =
                toEdge ? Color.TRANSPARENT : getNavigationBarColor(mForceDarkNavigationBarColor);

        if (mNavigationBarColor == newNavigationBarColor) return;

        @ColorInt int currentNavigationBarColor = mNavigationBarColor;
        mNavigationBarColor = newNavigationBarColor;

        if (ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled() && !toEdge) {
            animateNavigationBarColor(currentNavigationBarColor, newNavigationBarColor);
        } else {
            mWindow.setNavigationBarColor(mNavigationBarColor);

            if (toEdge) return;

            setNavigationBarDividerColor(
                    getNavigationBarDividerColor(mForceDarkNavigationBarColor));
            UiUtils.setNavigationBarIconColor(
                    mRootView, !mForceDarkNavigationBarColor && mLightNavigationBar);
        }
    }

    private void animateNavigationBarColor(
            @ColorInt int currentNavigationBarColor, @ColorInt int newNavigationBarColor) {
        if (mNavbarColorTransitionAnimation != null
                && mNavbarColorTransitionAnimation.isRunning()) {
            mNavbarColorTransitionAnimation.end();
        }
        mNavbarColorTransitionAnimation =
                ValueAnimator.ofFloat(0, 1).setDuration(NAVBAR_COLOR_TRANSITION_DURATION_MS);
        mNavbarColorTransitionAnimation.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        mNavbarColorTransitionAnimation.addUpdateListener(
                (ValueAnimator animation) -> {
                    float fraction = animation.getAnimatedFraction();
                    int blendedColor =
                            ColorUtils.blendColorsMultiply(
                                    currentNavigationBarColor, newNavigationBarColor, fraction);
                    mWindow.setNavigationBarColor(blendedColor);

                    setNavigationBarDividerColor(blendedColor);
                    UiUtils.setNavigationBarIconColor(
                            mRootView,
                            ColorUtils.isHighLuminance(
                                    ColorUtils.calculateLuminance(blendedColor)));
                });
        mNavbarColorTransitionAnimation.start();
    }

    @SuppressLint("NewApi")
    private void updateNavigationBarColor() {
        updateNavigationBarColor(null);
    }

    @SuppressLint("NewApi")
    private void setNavigationBarDividerColor(int navigationBarDividerColor) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(navigationBarDividerColor);
        }
    }

    /**
     * Update the scrim amount on the navigation bar.
     * @param fraction The scrim fraction in range [0, 1].
     */
    public void setNavigationBarScrimFraction(float fraction) {
        if (mEdgeToEdgeControllerSupplier.get() != null
                && mEdgeToEdgeControllerSupplier.get().isToEdge()) {
            return;
        }

        mNavigationBarScrimFraction = fraction;
        mWindow.setNavigationBarColor(
                applyCurrentScrimToColor(getNavigationBarColor(mForceDarkNavigationBarColor)));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(
                    applyCurrentScrimToColor(
                            getNavigationBarDividerColor(mForceDarkNavigationBarColor)));
        }

        // Adjust the color of navigation bar icons based on color state of the navigation bar.
        if (MathUtils.areFloatsEqual(1f, fraction)) {
            UiUtils.setNavigationBarIconColor(mRootView, false);
        } else if (MathUtils.areFloatsEqual(0f, fraction)) {
            UiUtils.setNavigationBarIconColor(mRootView, true);
        }
    }

    @ColorInt
    private int getNavigationBarColor(boolean forceDarkNavigationBar) {
        if (useBottomAttachedUiColor()) {
            return mBottomAttachedUiColor;
        }
        if (useActiveTabColor()) {
            return mActiveTab.getBackgroundColor();
        }

        return forceDarkNavigationBar
                ? mContext.getColor(R.color.toolbar_background_primary_dark)
                : SemanticColorUtils.getBottomSystemNavColor(mWindow.getContext());
    }

    @VisibleForTesting
    @ColorInt
    int getNavigationBarDividerColor(boolean forceDarkNavigationBar) {
        if (useBottomAttachedUiColor()) {
            return mBottomAttachedUiColor;
        }
        if (useActiveTabColor()) {
            return mActiveTab.getBackgroundColor();
        }
        return forceDarkNavigationBar
                ? mContext.getColor(R.color.bottom_system_nav_divider_color_light)
                : SemanticColorUtils.getBottomSystemNavDividerColor(mWindow.getContext());
    }

    private @ColorInt int applyCurrentScrimToColor(@ColorInt int color) {
        return ColorUtils.overlayColor(color, mDefaultScrimColor, mNavigationBarScrimFraction);
    }

    private boolean useBottomAttachedUiColor() {
        return ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()
                && mBottomAttachedUiColor != null
                && getBottomInset() == 0;
    }

    private boolean useActiveTabColor() {
        return ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()
                && mLayoutManager != null
                && mLayoutManager.getActiveLayoutType() == LayoutType.BROWSING
                && mActiveTab != null
                && getBottomInset() == 0;
    }

    private int getBottomInset() {
        return mEdgeToEdgeControllerSupplier != null && mEdgeToEdgeControllerSupplier.get() != null
                ? mEdgeToEdgeControllerSupplier.get().getBottomInset()
                : 0;
    }

    void setLayoutManagerForTesting(LayoutManager layoutManager) {
        setLayoutManager(layoutManager);
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

    int getNavigationBarColorForTesting() {
        return mNavigationBarColor;
    }
}
