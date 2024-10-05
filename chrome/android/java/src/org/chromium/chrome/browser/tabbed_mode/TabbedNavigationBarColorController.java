// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
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
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetVisualStateProvider;
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
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.NavigationBarColorProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.UiUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;

import java.util.Optional;

/** Controls the bottom system navigation bar color for the provided {@link Window}. */
@RequiresApi(Build.VERSION_CODES.O_MR1)
class TabbedNavigationBarColorController
        implements BottomAttachedUiObserver.Observer, NavigationBarColorProvider {
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
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final @Nullable FullscreenManager.Observer mFullscreenObserver;
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
    private @Nullable @ColorInt Integer mTargetWindowNavigationBarColor;

    private boolean mForceDarkNavigationBarColor;
    private boolean mIsInFullscreen;
    private float mNavigationBarScrimFraction;

    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final Callback<EdgeToEdgeController> mEdgeToEdgeRegisterChangeObserverCallback;
    private EdgeToEdgeController mEdgeToEdgeController;
    @Nullable private ChangeObserver mEdgeToEdgeChangeObserver;
    private @Nullable final BottomAttachedUiObserver mBottomAttachedUiObserver;

    private @Nullable Tab mActiveTab;
    private TabObserver mTabObserver;
    @Nullable private @ColorInt Integer mBottomAttachedUiColor;
    private boolean mForceShowDivider;

    private ValueAnimator mNavbarColorTransitionAnimation;
    private ObserverList<Observer> mObservers = new ObserverList<>();

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
     * @param accessorySheetVisualStateSupplier Supplies an {@link
     *     AccessorySheetVisualStateProvider} to watch for visual changes to the keyboard accessory
     *     sheet.
     * @param insetObserver An {@link InsetObserver} to listen for changes to the window insets.
     */
    TabbedNavigationBarColorController(
            Window window,
            TabModelSelector tabModelSelector,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            @NonNull BottomControlsStacker bottomControlsStacker,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull ObservableSupplier<ContextualSearchManager> contextualSearchManagerSupplier,
            @NonNull BottomSheetController bottomSheetController,
            Optional<OmniboxSuggestionsVisualState> omniboxSuggestionsVisualState,
            @NonNull
                    ObservableSupplier<AccessorySheetVisualStateProvider>
                            accessorySheetVisualStateSupplier,
            InsetObserver insetObserver) {
        this(
                window,
                tabModelSelector,
                layoutManagerSupplier,
                fullscreenManager,
                edgeToEdgeControllerSupplier,
                ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()
                        ? new BottomAttachedUiObserver(
                                bottomControlsStacker,
                                browserControlsStateProvider,
                                snackbarManagerSupplier.get(),
                                contextualSearchManagerSupplier,
                                bottomSheetController,
                                omniboxSuggestionsVisualState,
                                accessorySheetVisualStateSupplier,
                                insetObserver)
                        : null);
    }

    @VisibleForTesting
    TabbedNavigationBarColorController(
            Window window,
            TabModelSelector tabModelSelector,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            @Nullable BottomAttachedUiObserver bottomAttachedUiObserver) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1;

        mWindow = window;
        mRootView = (ViewGroup) mWindow.getDecorView().getRootView();
        mContext = mRootView.getContext();
        mDefaultScrimColor = mContext.getColor(R.color.default_scrim_color);
        mFullScreenManager = fullscreenManager;
        mLightNavigationBar =
                mContext.getResources().getBoolean(R.bool.window_light_navigation_bar);

        mBottomAttachedUiObserver = bottomAttachedUiObserver;
        if (mBottomAttachedUiObserver != null) {
            mBottomAttachedUiObserver.addObserver(this);
        }

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
                        mEdgeToEdgeController.unregisterObserver(mEdgeToEdgeChangeObserver);
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

        // TODO(crbug.com/40560014): Observe tab loads to restrict black bottom nav to
        // incognito NTP.

        updateNavigationBarColor();
    }

    /** Destroy this {@link TabbedNavigationBarColorController} instance. */
    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
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
    public void onBottomAttachedColorChanged(
            @Nullable @ColorInt Integer color, boolean forceShowDivider, boolean disableAnimation) {
        mBottomAttachedUiColor = color;
        updateNavigationBarColor(forceShowDivider, disableAnimation);
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
        updateNavigationBarColor(/* forceShowDivider= */ false, /* disableAnimation= */ false);
    }

    @SuppressLint("NewApi")
    private void updateNavigationBarColor(boolean forceShowDivider, boolean disableAnimation) {
        // 1. Calculate if we force / override the navigation bar color.
        boolean toEdge = isDrawingToEdge();
        boolean forceDarkNavigation = mTabModelSelector.isIncognitoSelected();

        forceDarkNavigation &= !UiUtils.isSystemUiThemingDisabled();
        forceDarkNavigation |= mIsInFullscreen;
        mForceDarkNavigationBarColor = forceDarkNavigation;

        // 2. Calculate colors and store update states.
        final @ColorInt int newNavigationBarColor = getNavigationBarColor(forceDarkNavigation);
        final @ColorInt int dividerColor =
                getNavigationBarDividerColor(forceDarkNavigation, forceShowDivider);
        // Check the window for the current navigation bar color - though ideally all window
        // navigation bar color changes would be done through this class, it is possible for other
        // classes to have changed the color (directly or through applying certain themes/styling.
        final @ColorInt int currentWindowNavigationBarColor = mWindow.getNavigationBarColor();
        final @ColorInt int newWindowNavigationBarColor =
                toEdge ? Color.TRANSPARENT : newNavigationBarColor;
        final @ColorInt int windowDividerColor = toEdge ? Color.TRANSPARENT : dividerColor;

        boolean updateDivider = mForceShowDivider != forceShowDivider;
        boolean updateNavBarColor = mNavigationBarColor != newNavigationBarColor;
        boolean updateDividerColor = updateNavBarColor || updateDivider;
        boolean alreadyAnimatingToWindowNavBarColor =
                mTargetWindowNavigationBarColor != null
                        && mTargetWindowNavigationBarColor.equals(newWindowNavigationBarColor);
        boolean updateWindowNavBarColor =
                currentWindowNavigationBarColor != newWindowNavigationBarColor
                        && !alreadyAnimatingToWindowNavBarColor;

        mNavigationBarColor = newNavigationBarColor;
        mForceShowDivider = forceShowDivider;

        // 3. Notify observer about color updates.
        if (updateNavBarColor) {
            for (NavigationBarColorProvider.Observer observer : mObservers) {
                observer.onNavigationBarColorChanged(mNavigationBarColor);
            }
        }
        if (updateDividerColor) {
            for (NavigationBarColorProvider.Observer observer : mObservers) {
                observer.onNavigationBarDividerChanged(dividerColor);
            }
        }

        // 4. Perform updates to the system nav bar when needed.
        if (!updateWindowNavBarColor && !updateDivider) return;

        boolean animateColorUpdate =
                ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()
                        && !isNavBarColorAnimationDisabled()
                        && !disableAnimation;

        endNavigationBarColorAnimationIfRunning();
        if (toEdge) {
            // When setting a transparent navbar for drawing toEdge, the system navbar contrast
            // should not be enforced - otherwise, some devices will apply a scrim to the navbar.
            mWindow.setNavigationBarContrastEnforced(false);
            // When drawing to edge, the new window nav bar color is always transparent.
            // This is called only once when |currentWindowNavigationBarColor| is another color.
            mWindow.setNavigationBarColor(Color.TRANSPARENT);
            // TODO (crbug.com/370526173) Refactor to a single method setting both navbar and
            // divider colors.
            setWindowNavigationBarDividerColor(windowDividerColor);
        } else if (animateColorUpdate) { // if (!toEdge)
            animateNavigationBarColor(currentWindowNavigationBarColor, newWindowNavigationBarColor);
        } else { // if (!toEdge && !animateColorUpdate)
            mWindow.setNavigationBarColor(newWindowNavigationBarColor);
            setWindowNavigationBarDividerColor(windowDividerColor);
            UiUtils.setNavigationBarIconColor(
                    mRootView,
                    ColorUtils.isHighLuminance(
                            ColorUtils.calculateLuminance(newNavigationBarColor)));
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
        mTargetWindowNavigationBarColor = newNavigationBarColor;

        mNavbarColorTransitionAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mTargetWindowNavigationBarColor = null;
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mTargetWindowNavigationBarColor = null;
                    }
                });
        mNavbarColorTransitionAnimation.addUpdateListener(
                (ValueAnimator animation) -> {
                    assert mTargetWindowNavigationBarColor != null;

                    float fraction = animation.getAnimatedFraction();
                    int blendedColor =
                            ColorUtils.blendColorsMultiply(
                                    currentNavigationBarColor,
                                    mTargetWindowNavigationBarColor,
                                    fraction);
                    mWindow.setNavigationBarColor(blendedColor);

                    if (mForceShowDivider) {
                        setWindowNavigationBarDividerColor(
                                getNavigationBarDividerColor(
                                        mForceDarkNavigationBarColor, mForceShowDivider));
                    } else {
                        setWindowNavigationBarDividerColor(blendedColor);
                    }
                    UiUtils.setNavigationBarIconColor(
                            mRootView,
                            ColorUtils.isHighLuminance(
                                    ColorUtils.calculateLuminance(blendedColor)));
                });
        mNavbarColorTransitionAnimation.start();
    }

    @SuppressLint("NewApi")
    private void updateNavigationBarColor() {
        updateNavigationBarColor(/* forceShowDivider= */ false, /* disableAnimation= */ false);
    }

    @SuppressLint("NewApi")
    private void setWindowNavigationBarDividerColor(int navigationBarDividerColor) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(navigationBarDividerColor);
        }
    }

    /**
     * Update the scrim amount on the navigation bar.
     *
     * @param fraction The scrim fraction in range [0, 1].
     */
    public void setNavigationBarScrimFraction(float fraction) {
        mNavigationBarScrimFraction = fraction;
        @ColorInt
        int scrimNavigationBarColor =
                applyCurrentScrimToColor(getNavigationBarColor(mForceDarkNavigationBarColor));
        mWindow.setNavigationBarColor(scrimNavigationBarColor);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mWindow.setNavigationBarDividerColor(
                    applyCurrentScrimToColor(
                            getNavigationBarDividerColor(mForceDarkNavigationBarColor, false)));
        }

        // Adjust the color of navigation bar icons based on color state of the navigation bar.
        UiUtils.setNavigationBarIconColor(
                mRootView,
                ColorUtils.isHighLuminance(ColorUtils.calculateLuminance(scrimNavigationBarColor)));
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
    int getNavigationBarDividerColor(boolean forceDarkNavigationBar, boolean forceShowDivider) {
        if (!forceShowDivider && useBottomAttachedUiColor()) {
            return mBottomAttachedUiColor;
        }
        if (!forceShowDivider && useActiveTabColor()) {
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
                && mBottomAttachedUiColor != null;
    }

    private boolean useActiveTabColor() {
        return ChromeFeatureList.sNavBarColorMatchesTabBackground.isEnabled()
                && mLayoutManager != null
                && mLayoutManager.getActiveLayoutType() == LayoutType.BROWSING
                && mActiveTab != null;
    }

    /**
     * Indicates whether the page is drawing to edge, either due to being on a page that's opted
     * into edge-to-edge or to displaying the bottom chin.
     */
    private boolean isDrawingToEdge() {
        return mEdgeToEdgeControllerSupplier != null
                && mEdgeToEdgeControllerSupplier.get() != null
                && mEdgeToEdgeControllerSupplier.get().isDrawingToEdge();
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

    private static boolean isNavBarColorAnimationDisabled() {
        return TabbedSystemUiCoordinator.NAV_BAR_COLOR_ANIMATION_DISABLED_CACHED_PARAM.getValue();
    }

    @Override
    public int getNavigationBarColor() {
        return mNavigationBarColor;
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }
}
