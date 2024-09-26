// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.content.Context;
import android.graphics.Color;
import android.view.View;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;

/**
 * Maintains the status bar color for a {@link Window}.
 *
 * <p>TODO(crbug.com/40915553): Prevent initialization of StatusBarColorController for automotive.
 */
public class StatusBarColorController
        implements DestroyObserver,
                TopToolbarCoordinator.UrlExpansionObserver,
                StatusIndicatorCoordinator.StatusIndicatorObserver,
                UrlFocusChangeListener,
                OmniboxSuggestionsDropdownScrollListener,
                TopToolbarCoordinator.ToolbarColorObserver {
    public static final @ColorInt int UNDEFINED_STATUS_BAR_COLOR = Color.TRANSPARENT;
    public static final @ColorInt int DEFAULT_STATUS_BAR_COLOR = Color.argb(0x01, 0, 0, 0);

    /** Provides the base status bar color. */
    public interface StatusBarColorProvider {
        /**
         * @return The base status bar color to override default colors used in the
         *         {@link StatusBarColorController}. If this returns
         *         {@link #DEFAULT_STATUS_BAR_COLOR}, {@link StatusBarColorController} will use the
         *         default status bar color.
         *         If this returns a color other than {@link #UNDEFINED_STATUS_BAR_COLOR} and
         *         {@link #DEFAULT_STATUS_BAR_COLOR}, the {@link StatusBarColorController} will
         *         always use the color provided by this method to adjust the status bar color.
         *         This color may be used as-is or adjusted due to a scrim overlay.
         */
        @ColorInt
        int getBaseStatusBarColor(Tab tab);
    }

    private final Window mWindow;
    private final boolean mIsTablet;
    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private final StatusBarColorProvider mStatusBarColorProvider;
    private final ActivityTabProvider.ActivityTabTabObserver mStatusBarColorTabObserver;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final TopUiThemeColorProvider mTopUiThemeColor;
    private final @ColorInt int mStandardPrimaryBgColor;
    private final @ColorInt int mIncognitoPrimaryBgColor;
    private final @ColorInt int mStandardDefaultThemeColor;
    private final @ColorInt int mIncognitoDefaultThemeColor;
    private final @ColorInt int mActiveOmniboxDefaultColor;
    private final @ColorInt int mIncognitoActiveOmniboxColor;
    private final @ColorInt int mStandardScrolledOmniboxColor;
    private final @ColorInt int mIncognitoScrolledOmniboxColor;
    private final @ColorInt int mBackgroundColorForNtp;
    private boolean mToolbarColorChanged;
    private @ColorInt int mToolbarColor;

    private @Nullable TabModelSelector mTabModelSelector;
    private CallbackController mCallbackController = new CallbackController();
    private @Nullable Tab mCurrentTab;
    private boolean mIsInOverviewMode;
    private boolean mIsIncognitoBranded;
    private boolean mIsOmniboxFocused;
    private boolean mAreSuggestionsScrolled;

    private @ColorInt int mScrimColor = ScrimProperties.INVALID_COLOR;
    private float mStatusBarScrimFraction;

    private boolean mShouldUpdateStatusBarColorForNtp;
    private @ColorInt int mStatusIndicatorColor;
    private @ColorInt int mStatusBarColorWithoutStatusIndicator;

    // Tab strip transition states.
    private boolean mTabStripHiddenOnTablet;
    private @ColorInt int mTabStripTransitionOverlayColor = ScrimProperties.INVALID_COLOR;
    private float mTabStripTransitionOverlayAlpha;
    private boolean mAllowToolbarColorOnTablets;

    private final LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                @Override
                public void onStartedShowing(@LayoutType int layoutType) {
                    if (layoutType != LayoutType.TAB_SWITCHER) {
                        return;
                    }
                    mIsInOverviewMode = true;
                }

                @Override
                public void onFinishedHiding(@LayoutType int layoutType) {
                    if (layoutType != LayoutType.TAB_SWITCHER) {
                        return;
                    }
                    mIsInOverviewMode = false;
                    updateStatusBarColor();
                }
            };

    /**
     * Constructs a StatusBarColorController.
     *
     * @param window The Android app window, used to access decor view and set the status color.
     * @param isTablet Whether the current context is on a tablet.
     * @param context The Android context used to load colors.
     * @param statusBarColorProvider An implementation of {@link StatusBarColorProvider}.
     * @param layoutManagerSupplier Supplies the layout manager.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param topUiThemeColorProvider The {@link ThemeColorProvider} for top UI.
     */
    public StatusBarColorController(
            Window window,
            boolean isTablet,
            Context context,
            StatusBarColorProvider statusBarColorProvider,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ActivityTabProvider tabProvider,
            TopUiThemeColorProvider topUiThemeColorProvider) {
        mWindow = window;
        mIsTablet = isTablet;
        mStatusBarColorProvider = statusBarColorProvider;
        mAllowToolbarColorOnTablets = false;

        mStandardPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(context, false);
        mIncognitoPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(context, true);
        mStandardDefaultThemeColor = ChromeColors.getDefaultThemeColor(context, false);
        mIncognitoDefaultThemeColor = ChromeColors.getDefaultThemeColor(context, true);
        mBackgroundColorForNtp =
                ChromeColors.getSurfaceColor(
                        context, R.dimen.home_surface_background_color_elevation);
        mStatusIndicatorColor = UNDEFINED_STATUS_BAR_COLOR;

        // TODO(b/41494931): Share code with LocationBarCoordinator's constructor.
        mActiveOmniboxDefaultColor =
                ChromeColors.getSurfaceColor(
                        context, R.dimen.omnibox_suggestion_dropdown_bg_elevation);
        mIncognitoActiveOmniboxColor = context.getColor(R.color.omnibox_dropdown_bg_incognito);
        // TODO(b/41494931): Share code with ToolbarPhone#getToolbarDefaultColor().
        mStandardScrolledOmniboxColor =
                ChromeColors.getSurfaceColor(context, R.dimen.toolbar_text_box_elevation);
        mIncognitoScrolledOmniboxColor =
                context.getColor(R.color.default_bg_color_dark_elev_2_baseline);

        mStatusBarColorTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(tabProvider) {
                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        updateStatusBarColor();
                    }

                    @Override
                    public void onDidChangeThemeColor(Tab tab, @ColorInt int color) {
                        updateStatusBarColor();
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        final boolean newShouldUpdateStatusBarColorForNtp = isStandardNtp();
                        // Also update the status bar color if the content was previously an NTP,
                        // because an NTP can use a different status bar color than the default
                        // theme color. In this case, the theme color might not change, and thus
                        // #onDidChangeThemeColor
                        // might not get called.
                        if (mShouldUpdateStatusBarColorForNtp
                                || newShouldUpdateStatusBarColorForNtp) {
                            updateStatusBarColor();
                        }
                        mShouldUpdateStatusBarColorForNtp = newShouldUpdateStatusBarColorForNtp;
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        // Stop referring to the Tab once detached from an activity. Will be
                        // restored by |onObservingDifferentTab|.
                        if (window == null) mCurrentTab = null;
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        // Make sure that #mCurrentTab is cleared because #onObservingDifferentTab()
                        // might not be notified early enough when
                        // #onUrlExpansionPercentageChanged() is
                        // called.
                        mCurrentTab = null;
                        mShouldUpdateStatusBarColorForNtp = false;
                    }

                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        mCurrentTab = tab;
                        mShouldUpdateStatusBarColorForNtp = isStandardNtp();

                        // |tab == null| means we're switching tabs - by the tab switcher or by
                        // swiping on the omnibox. These cases are dealt with differently,
                        // elsewhere.
                        if (tab == null) return;
                        updateStatusBarColor();
                    }
                };

        mCurrentTabModelObserver =
                (tabModel) -> {
                    mIsIncognitoBranded = tabModel.isIncognitoBranded();
                    // When opening a new Incognito Tab from a normal Tab (or vice versa), the
                    // status bar color is updated. However, this update is triggered after the
                    // animation, so we update here for the duration of the new Tab animation.
                    // See https://crbug.com/917689.
                    updateStatusBarColor();
                };

        if (layoutManagerSupplier != null) {
            layoutManagerSupplier.addObserver(
                    mCallbackController.makeCancelable(
                            layoutManager -> {
                                assert layoutManager != null;
                                mLayoutStateProvider = layoutManager;
                                mLayoutStateProvider.addObserver(mLayoutStateObserver);
                            }));
        }

        activityLifecycleDispatcher.register(this);
        mTopUiThemeColor = topUiThemeColorProvider;
        mToolbarColorChanged = false;
    }

    // DestroyObserver implementation.
    @Override
    public void onDestroy() {
        mStatusBarColorTabObserver.destroy();
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    // TopToolbarCoordinator.UrlExpansionObserver implementation.
    @Override
    public void onUrlExpansionProgressChanged() {
        if (mShouldUpdateStatusBarColorForNtp) updateStatusBarColor();
    }

    // TopToolbarCoordinator.ToolbarColorObserver implementation.
    @Override
    public void onToolbarColorChanged(@ColorInt int color) {
        // Status bar color on tablets could change when the tab strip transition is enabled,
        // where it might be required for the status bar color to match the toolbar color when the
        // tab strip is hidden.
        if (mIsTablet && !mAllowToolbarColorOnTablets) return;

        // Set mToolbarColorChanged to true as an extra check to prevent rendering status bar with
        // default color if toolbar never changes, for example, in dark mode.
        mToolbarColorChanged = true;
        mToolbarColor = color;
        updateStatusBarColor();
    }

    // StatusIndicatorCoordinator.StatusIndicatorObserver implementation.
    @Override
    public void onStatusIndicatorColorChanged(@ColorInt int newColor) {
        mStatusIndicatorColor = newColor;
        updateStatusBarColor();
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mIsOmniboxFocused = hasFocus;
        updateStatusBarColor();
    }

    @Override
    public void onSuggestionDropdownScroll() {
        mAreSuggestionsScrolled = true;
        updateStatusBarColor();
    }

    @Override
    public void onSuggestionDropdownOverscrolledToTop() {
        mAreSuggestionsScrolled = false;
        updateStatusBarColor();
    }

    /**
     * Update the scrim color on the status bar.
     * @param scrimColor The scrim color int.
     */
    public void setScrimColor(@ColorInt int scrimColor) {
        mScrimColor = scrimColor;
    }

    /**
     * @return The current scrim color for the status bar.
     */
    public @ColorInt int getScrimColorForTesting() {
        return mScrimColor;
    }

    /**
     * Update the scrim amount on the status bar.
     * @param fraction The scrim fraction in range [0, 1].
     */
    public void setStatusBarScrimFraction(float fraction) {
        mStatusBarScrimFraction = fraction;
        updateStatusBarColor();
    }

    /**
     * Add the tab strip transition scrim overlay on the status bar during a tab strip transition.
     *
     * @param overlayColor The overlay color.
     * @param overlayAlpha The alpha that |overlayColor| should have on the status bar color.
     */
    public void setTabStripColorOverlay(@ColorInt int overlayColor, float overlayAlpha) {
        assert mIsTablet;
        if (!mAllowToolbarColorOnTablets) return;
        mTabStripTransitionOverlayColor = overlayColor;
        mTabStripTransitionOverlayAlpha = overlayAlpha;
        updateStatusBarColor();
    }

    /**
     * @param tabModelSelector The {@link TabModelSelector} to check whether incognito model is
     *                         selected.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        assert mTabModelSelector == null : "mTabModelSelector should only be set once.";
        mTabModelSelector = tabModelSelector;
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
            mIsIncognitoBranded = mTabModelSelector.isIncognitoBrandedModelSelected();
            updateStatusBarColor();
        }
    }

    /** Calculate and update the status bar's color. */
    public void updateStatusBarColor() {
        mStatusBarColorWithoutStatusIndicator = calculateBaseStatusBarColor();
        @ColorInt
        int statusBarColor = applyStatusBarIndicatorColor(mStatusBarColorWithoutStatusIndicator);
        statusBarColor = applyTabStripOverlay(statusBarColor);
        statusBarColor = applyCurrentScrimToColor(statusBarColor);
        setStatusBarColor(mWindow, statusBarColor);
    }

    /**
     * Set whether the tab strip is hidden or visible on a tablet. This state will be used to
     * determine the base status bar color on a tablet.
     *
     * @param tabStripHiddenOnTablet Whether the tab strip is hidden or visible on a tablet.
     */
    public void setTabStripHiddenOnTablet(boolean tabStripHiddenOnTablet) {
        assert mIsTablet;
        if (!mAllowToolbarColorOnTablets) return;
        mTabStripHiddenOnTablet = tabStripHiddenOnTablet;
    }

    /**
     * @return The status bar color without the status indicator's color taken into consideration.
     *         However, scrimming isn't included since it's managed completely by this class.
     */
    public @ColorInt int getStatusBarColorWithoutStatusIndicator() {
        return mStatusBarColorWithoutStatusIndicator;
    }

    private @ColorInt int calculateBaseStatusBarColor() {
        // Return overridden status bar color from StatusBarColorProvider if specified.
        @ColorInt
        int baseStatusBarColor = mStatusBarColorProvider.getBaseStatusBarColor(mCurrentTab);
        if (baseStatusBarColor == DEFAULT_STATUS_BAR_COLOR) {
            return calculateDefaultStatusBarColor();
        }
        if (baseStatusBarColor != UNDEFINED_STATUS_BAR_COLOR) {
            return baseStatusBarColor;
        }

        if (mIsTablet) {
            // When applicable, the status bar should use the default tab strip color, that is not
            // affected by an activity focus change.
            return mTabStripHiddenOnTablet
                    ? mToolbarColor
                    : TabUiThemeUtil.getTabStripBackgroundColor(
                            mWindow.getContext(), mIsIncognitoBranded);
        }

        // When Omnibox gains focus, we want to clear the status bar theme color.
        // The theme should be restored when Omnibox focus clears.
        if (mIsOmniboxFocused) {
            // If the flag is enabled, we will use the toolbar color.
            if (mToolbarColorChanged) return mToolbarColor;
            return calculateDefaultStatusBarColor();
        }

        // Return status bar color in overview mode.
        if (mIsInOverviewMode) {
            // Toolbar will notify status bar color controller about the toolbar color during
            // overview animation.
            return mToolbarColor;
        }

        // Return New Tab Page background color in New Tab Page.
        if (isStandardNtp()) {
            return mBackgroundColorForNtp;
        }

        // Return status bar color to match the toolbar.
        // If the flag is enabled, we will use the toolbar color.
        if (mToolbarColorChanged) return mToolbarColor;
        return mTopUiThemeColor.getThemeColorOrFallback(
                mCurrentTab, calculateDefaultStatusBarColor());
    }

    /** Calculates the default status bar color based on the incognito state. */
    private @ColorInt int calculateDefaultStatusBarColor() {
        if (!mIsOmniboxFocused) {
            return mIsIncognitoBranded ? mIncognitoDefaultThemeColor : mStandardDefaultThemeColor;
        }

        if (mAreSuggestionsScrolled) {
            return mIsIncognitoBranded
                    ? mIncognitoScrolledOmniboxColor
                    : mStandardScrolledOmniboxColor;
        } else {
            return mIsIncognitoBranded ? mIncognitoActiveOmniboxColor : mActiveOmniboxDefaultColor;
        }
    }

    /**
     * Set device status bar to a given color. Also, set the status bar icons to a dark color if
     * needed.
     * @param window The current window of the UI view.
     * @param color The color that the status bar should be set to.
     */
    public static void setStatusBarColor(Window window, @ColorInt int color) {
        if (UiUtils.isSystemUiThemingDisabled()) return;

        final View root = window.getDecorView().getRootView();
        boolean needsDarkStatusBarIcons = !ColorUtils.shouldUseLightForegroundOnBackground(color);
        UiUtils.setStatusBarIconColor(root, needsDarkStatusBarIcons);
        UiUtils.setStatusBarColor(window, color);
    }

    /**
     * Takes status bar indicator into account in status bar color computation.
     *
     * @param darkenedBaseColor The status bar color without the status indicator's color taken into
     *     consideration. (as specified in {@link getStatusBarColorWithoutStatusIndicator()}).
     * @return The resulting color.
     */
    private @ColorInt int applyStatusBarIndicatorColor(@ColorInt int darkenedBaseColor) {
        if (mStatusIndicatorColor == UNDEFINED_STATUS_BAR_COLOR) return darkenedBaseColor;

        return mStatusIndicatorColor;
    }

    /**
     * Get the scrim applied color if the scrim is showing. Otherwise, return the original color.
     * @param color Color to maybe apply scrim to.
     * @return The resulting color.
     */
    private @ColorInt int applyCurrentScrimToColor(@ColorInt int color) {
        if (mScrimColor == ScrimProperties.INVALID_COLOR) {
            final View root = mWindow.getDecorView().getRootView();
            final Context context = root.getContext();
            mScrimColor = context.getColor(R.color.default_scrim_color);
        }
        // Apply a color overlay if the scrim is showing.
        return ColorUtils.overlayColor(color, mScrimColor, mStatusBarScrimFraction);
    }

    /**
     * Apply and get the tab strip overlay applied color if the strip transition scrim is showing.
     *
     * @param color Base color to apply the overlay to.
     */
    private @ColorInt int applyTabStripOverlay(@ColorInt int color) {
        // If the tab strip is transitioning on a tablet, apply the strip overlay on the status bar.
        if (mIsTablet && mTabStripTransitionOverlayAlpha > 0) {
            return ColorUtils.getColorWithOverlay(
                    color, mTabStripTransitionOverlayColor, mTabStripTransitionOverlayAlpha);
        }
        return color;
    }

    /**
     * Determines whether the status bar color could use the toolbar color on tablets when certain
     * conditions are met. This is currently supported on ChromeTabbedActivity's only.
     *
     * @param allowToolbarColorOnTablets Whether the status bar color could use the toolbar color.
     */
    // TODO(crbug.com/324313724): Support this for other activities.
    public void setAllowToolbarColorOnTablets(boolean allowToolbarColorOnTablets) {
        mAllowToolbarColorOnTablets = allowToolbarColorOnTablets;
    }

    /**
     * @return Whether or not the current tab is a new tab page in standard mode.
     */
    private boolean isStandardNtp() {
        return mCurrentTab != null && mCurrentTab.getNativePage() instanceof NewTabPage;
    }
}
