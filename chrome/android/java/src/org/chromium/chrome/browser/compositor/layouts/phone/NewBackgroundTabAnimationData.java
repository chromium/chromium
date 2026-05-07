// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabAnimationHostView.AnimationType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/**
 * Holds a snapshot of the tab switcher button state and associated layout colors for the duration
 * of the new background tab animation.
 *
 * <p>This class combines the computation and storage of animation data to avoid redundant UI
 * queries and ensure data consistency during the animation.
 */
@NullMarked
public class NewBackgroundTabAnimationData {
    private final Context mContext;
    private final View mAnimationHostView;
    private final ToolbarManager mToolbarManager;
    private final NonNullObservableSupplier<Float> mNtpSearchBoxTransitionPercentageSupplier;
    private final boolean mIsBottomBarEnabled;
    private final Rect mTabSwitcherButtonRect = new Rect();

    // View related cache.
    private @Nullable View mBottomBarTabSwitcherButton;
    private @Nullable View mToolbarTabSwitcherButton;

    // Snapshotted state.
    private boolean mIsBottomBarVisible;
    private boolean mIsPositionOnTop;
    private @Nullable View mTabSwitcherButton;
    private @ColorInt int mPrimaryColor;
    private @BrandedColorScheme int mBrandedColorScheme;
    private @Nullable ColorStateList mIconTint;
    private @AnimationType int mAnimationType;

    // Default dimensions cache. Only height is needed for manual calculation when scrolled off.
    // Width and bottom bar dimensions are handled by getGlobalVisibleRect.
    private final int mDefaultToolbarButtonHeight;

    /**
     * Creates an instance of {@link NewBackgroundTabAnimationData}.
     *
     * @param animationHostView The host view for animations.
     * @param toolbarManager The {@link ToolbarManager} instance.
     */
    public NewBackgroundTabAnimationData(View animationHostView, ToolbarManager toolbarManager) {
        mAnimationHostView = animationHostView;
        mContext = animationHostView.getContext();
        mToolbarManager = toolbarManager;
        mNtpSearchBoxTransitionPercentageSupplier =
                toolbarManager.getNtpSearchBoxTransitionPercentageSupplier();
        mIsBottomBarEnabled = BottomBarConfigUtils.isBottomBarEnabled(mContext);

        Resources resources = mContext.getResources();
        mDefaultToolbarButtonHeight =
                resources.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
    }

    /**
     * Captures a snapshot of the current UI state for the duration of the animation. This includes
     * computing the visibility of the bottom bar, finding the tab switcher button, calculating its
     * location, and retrieving the colors.
     *
     * <p>This method should be called at the start of the animation to ensure that all getters
     * return consistent, snapshotted data.
     *
     * @param expectedToolbarTop The expected Y coordinate of the toolbar top when visible.
     */
    /* package */ void captureState(
            @Nullable Tab tab, boolean isRegularNtp, int expectedToolbarTop) {
        // Reset target button.
        mTabSwitcherButton = null;
        mTabSwitcherButtonRect.setEmpty();

        if (mIsBottomBarEnabled) updateBottomBarTabSwitcherButtonCalculations(isRegularNtp);

        if (mIsBottomBarVisible) {
            // When the bottom bar is enabled, NewTabAnimationLayout will force it to be visible, so
            // we do not need to worry about the visibility of the bottom bar button.
            mTabSwitcherButton = mBottomBarTabSwitcherButton;
        } else {
            // Fallback to the toolbar button if the bottom bar is not visible.
            updateToolbarTabSwitcherButtonViewIfNull();
            mTabSwitcherButton = mToolbarTabSwitcherButton;
        }

        assumeNonNull(mTabSwitcherButton);
        mIsPositionOnTop =
                !mIsBottomBarVisible && ToolbarPositionController.shouldShowToolbarOnTop(tab);

        boolean tabSwitcherButtonIsVisible =
                mTabSwitcherButton.getGlobalVisibleRect(mTabSwitcherButtonRect);

        if (mIsPositionOnTop) {
            // Override Y coordinates with the calculated expected position to avoid issues when
            // scrolled off.
            // X coordinates and width from getGlobalVisibleRect are still valid.
            mTabSwitcherButtonRect.top = expectedToolbarTop;
            mTabSwitcherButtonRect.bottom = expectedToolbarTop + mDefaultToolbarButtonHeight;
        }

        mBrandedColorScheme = computeBrandedColorScheme(tab);
        mPrimaryColor = computePrimaryColor();
        mIconTint = computeIconTint();

        float ntpToolbarTransitionPercentage = mNtpSearchBoxTransitionPercentageSupplier.get();

        mAnimationType =
                NewBackgroundTabAnimationHostView.calculateAnimationType(
                        tabSwitcherButtonIsVisible,
                        isRegularNtp,
                        ntpToolbarTransitionPercentage,
                        mIsBottomBarVisible);

        if (isRegularNtp
                && mAnimationType == AnimationType.DEFAULT
                && NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false)
                && !mIsBottomBarVisible) {
            mBrandedColorScheme = BrandedColorScheme.DARK_BRANDED_THEME;
            mIconTint = ThemeUtils.getThemedToolbarIconTint(mContext, mBrandedColorScheme);
        }
    }

    /** Returns the calculated animation type. */
    /* package */ @AnimationType
    int getAnimationType() {
        return mAnimationType;
    }

    /** Returns the primary color of the bar containing the button. */
    @ColorInt
    /* package */ int getPrimaryColor() {
        return mPrimaryColor;
    }

    /** Returns the clean bounds of the tab switcher button. */
    /* package */ Rect getTabSwitcherButtonRect() {
        return mTabSwitcherButtonRect;
    }

    /** Returns the actual tab switcher button view. */
    /* package */ View getTabSwitcherButton() {
        assert mTabSwitcherButton != null : "No tab switcher button cached/found.";
        return mTabSwitcherButton;
    }

    /** Returns whether the bar is positioned on top of the screen. */
    /* package */ boolean isPositionOnTop() {
        return mIsPositionOnTop;
    }

    /** Returns the branded color scheme for the bar containing the button. */
    @BrandedColorScheme
    /* package */ int getBrandedColorScheme() {
        return mBrandedColorScheme;
    }

    /** Returns whether the bottom bar is visible. */
    /* package */ boolean isBottomBarVisible() {
        return mIsBottomBarVisible;
    }

    /** Returns the icon tint color. */
    /* package */ ColorStateList getIconTint() {
        assert mIconTint != null;
        return mIconTint;
    }

    private void updateBottomBarTabSwitcherButtonCalculations(boolean isNtp) {
        if (mBottomBarTabSwitcherButton == null) {
            View bottomBar =
                    mAnimationHostView.findViewById(
                            org.chromium.chrome.browser.ui.bottombar.R.id.bottom_bar_container);
            assert bottomBar != null : "Bottom bar view not found";
            mBottomBarTabSwitcherButton = bottomBar.findViewById(R.id.tab_switcher_button);
            assert mBottomBarTabSwitcherButton != null
                    : "Tab switcher button not found in bottom bar";
        }
        mIsBottomBarVisible =
                mIsBottomBarEnabled && !(isNtp && BottomBarConfigUtils.shouldDisableOnNtp());
    }

    private void updateToolbarTabSwitcherButtonViewIfNull() {
        if (mToolbarTabSwitcherButton == null) {
            View toolbar = mAnimationHostView.findViewById(R.id.toolbar);
            assert toolbar != null : "Toolbar view not found";
            mToolbarTabSwitcherButton = toolbar.findViewById(R.id.tab_switcher_button);
            assert mToolbarTabSwitcherButton != null : "Tab switcher button not found in toolbar";
        }
    }

    private @BrandedColorScheme int computeBrandedColorScheme(@Nullable Tab tab) {
        boolean isIncognito = tab != null && tab.isIncognitoBranded();

        if (mIsBottomBarVisible) {
            // TODO(crbug.com/491513154): Update this to use the actual bottom bar color scheme
            // from the provider.
            return isIncognito
                    ? BrandedColorScheme.INCOGNITO
                    : BrandedColorScheme.LIGHT_BRANDED_THEME;
        }

        int color = mToolbarManager.getPrimaryColor();
        return ThemeUtils.getBrandedColorScheme(mContext, color, isIncognito);
    }

    private @ColorInt int computePrimaryColor() {
        if (mIsBottomBarVisible) {
            return BottomBarUtils.getBottomBarBackgroundColor(mContext, mBrandedColorScheme);
        }
        return mToolbarManager.getPrimaryColor();
    }

    private ColorStateList computeIconTint() {
        if (mIsBottomBarVisible) {
            return BottomBarUtils.getIconColorStateList(mContext, mBrandedColorScheme);
        }
        return ThemeUtils.getThemedToolbarIconTint(mContext, mBrandedColorScheme);
    }
}
