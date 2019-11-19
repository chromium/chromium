// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper.MenuButtonState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.chrome.browser.ui.widget.highlight.PulseDrawable;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * The overflow menu button.
 */
public class MenuButton extends FrameLayout implements TintObserver {
    /** The {@link ImageButton} for the menu button. */
    private ImageButton mMenuImageButton;

    /** The view for the update badge. */
    private ImageView mUpdateBadgeView;
    private boolean mUseLightDrawables;

    private AppMenuButtonHelper mAppMenuButtonHelper;

    private boolean mHighlightingMenu;
    private PulseDrawable mHighlightDrawable;

    private boolean mSuppressAppMenuUpdateBadge;
    private AnimatorSet mMenuBadgeAnimatorSet;
    private boolean mIsMenuBadgeAnimationRunning;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /** The menu button text label. */
    private TextView mLabel;

    /** The wrapper View that contains the menu button and the label. */
    private View mWrapper;

    public MenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuImageButton = findViewById(R.id.menu_button);
        mUpdateBadgeView = findViewById(R.id.menu_badge);
    }

    public void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mAppMenuButtonHelper = appMenuButtonHelper;
        View touchView = mWrapper != null ? mWrapper : mMenuImageButton;
        if (mWrapper != null) mWrapper.setOnTouchListener(mAppMenuButtonHelper);
        mMenuImageButton.setOnTouchListener(mAppMenuButtonHelper);
        touchView.setAccessibilityDelegate(mAppMenuButtonHelper.getAccessibilityDelegate());
    }

    public AppMenuButtonHelper getAppMenuButtonHelper() {
        return mAppMenuButtonHelper;
    }

    public View getMenuBadge() {
        return mUpdateBadgeView;
    }

    public ImageButton getImageButton() {
        return mMenuImageButton;
    }

    /**
     * @param wrapper The wrapping View of this button.
     */
    public void setWrapperView(ViewGroup wrapper) {
        mWrapper = wrapper;
        mWrapper.setOnClickListener(null);
        mLabel = mWrapper.findViewById(R.id.menu_button_label);
        if (FeatureUtilities.isLabeledBottomToolbarEnabled()) mLabel.setVisibility(View.VISIBLE);
    }

    /**
     * Sets the update badge to visible.
     *
     * @param visible Whether the update badge should be visible. Always sets visibility to GONE
     *                if the update type does not require a badge.
     * TODO(crbug.com/865801): Clean this up when MenuButton and UpdateMenuItemHelper is MVCed.
     */
    private void setUpdateBadgeVisibility(boolean visible) {
        mUpdateBadgeView.setVisibility(visible ? View.VISIBLE : View.GONE);
        if (visible) updateImageResources();
        updateContentDescription(visible);
    }

    private void updateImageResources() {
        MenuButtonState buttonState = UpdateMenuItemHelper.getInstance().getUiState().buttonState;
        if (buttonState == null) return;
        @DrawableRes
        int drawable = mUseLightDrawables ? buttonState.lightBadgeIcon : buttonState.darkBadgeIcon;
        mUpdateBadgeView.setImageDrawable(
                ApiCompatibilityUtils.getDrawable(getResources(), drawable));
    }

    /**
     * Show the update badge on the app menu button.
     * @param animate Whether to animate the showing of the update badge.
     */
    public void showAppMenuUpdateBadgeIfAvailable(boolean animate) {
        if (mUpdateBadgeView == null || mMenuImageButton == null || mSuppressAppMenuUpdateBadge
                || !isBadgeAvailable()) {
            return;
        }

        updateImageResources();
        updateContentDescription(true);
        if (!animate || mIsMenuBadgeAnimationRunning) {
            setUpdateBadgeVisibility(true);
            return;
        }

        // Set initial states.
        mUpdateBadgeView.setAlpha(0.f);
        mUpdateBadgeView.setVisibility(View.VISIBLE);

        mMenuBadgeAnimatorSet = createShowUpdateBadgeAnimation(mMenuImageButton, mUpdateBadgeView);

        mMenuBadgeAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mIsMenuBadgeAnimationRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }
        });

        mMenuBadgeAnimatorSet.start();
    }

    /**
     * Remove the update badge on the app menu button.
     * @param animate Whether to animate the hiding of the update badge.
     */
    public void removeAppMenuUpdateBadge(boolean animate) {
        if (mUpdateBadgeView == null || !isShowingAppMenuUpdateBadge()) return;
        updateContentDescription(false);

        if (!animate) {
            setUpdateBadgeVisibility(false);
            return;
        }

        if (mIsMenuBadgeAnimationRunning && mMenuBadgeAnimatorSet != null) {
            mMenuBadgeAnimatorSet.cancel();
        }

        // Set initial states.
        mMenuImageButton.setAlpha(0.f);

        mMenuBadgeAnimatorSet = createHideUpdateBadgeAnimation(mMenuImageButton, mUpdateBadgeView);

        mMenuBadgeAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mIsMenuBadgeAnimationRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }
        });

        mMenuBadgeAnimatorSet.start();
    }

    /**
     * @param suppress Whether to prevent the update badge from being show. This is currently only
     *                 used to prevent the badge from being shown in the tablet tab switcher.
     */
    public void setAppMenuUpdateBadgeSuppressed(boolean suppress) {
        mSuppressAppMenuUpdateBadge = suppress;
        if (mSuppressAppMenuUpdateBadge) {
            removeAppMenuUpdateBadge(false);
        } else {
            showAppMenuUpdateBadgeIfAvailable(false);
        }
    }

    /**
     * @return Whether the update badge is showing.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        return mUpdateBadgeView.getVisibility() == View.VISIBLE;
    }

    private static boolean isBadgeAvailable() {
        return UpdateMenuItemHelper.getInstance().getUiState().buttonState != null;
    }

    /**
     * Sets the content description for the menu button.
     * @param isUpdateBadgeVisible Whether the update menu badge is visible.
     */
    private void updateContentDescription(boolean isUpdateBadgeVisible) {
        if (isUpdateBadgeVisible) {
            MenuButtonState buttonState =
                    UpdateMenuItemHelper.getInstance().getUiState().buttonState;
            assert buttonState != null : "No button state when trying to show the badge.";
            mMenuImageButton.setContentDescription(
                    getResources().getString(buttonState.menuContentDescription));
        } else {
            mMenuImageButton.setContentDescription(
                    getResources().getString(R.string.accessibility_toolbar_btn_menu));
        }
    }

    /**
     * Sets the menu button's background depending on whether or not we are highlighting and whether
     * or not we are using light or dark assets.
     */
    public void setMenuButtonHighlightDrawable() {
        // Return if onFinishInflate didn't finish
        if (mMenuImageButton == null) return;

        if (mHighlightingMenu) {
            if (mHighlightDrawable == null) {
                mHighlightDrawable = PulseDrawable.createCircle(getContext());
                mHighlightDrawable.setInset(ViewCompat.getPaddingStart(mMenuImageButton),
                        mMenuImageButton.getPaddingTop(),
                        ViewCompat.getPaddingEnd(mMenuImageButton),
                        mMenuImageButton.getPaddingBottom());
            }
            mHighlightDrawable.setUseLightPulseColor(
                    getContext().getResources(), mUseLightDrawables);
            setBackground(mHighlightDrawable);
            mHighlightDrawable.start();
        } else {
            setBackground(null);
        }
    }

    public void setMenuButtonHighlight(boolean highlight) {
        mHighlightingMenu = highlight;
        setMenuButtonHighlightDrawable();
    }

    public void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
    }

    @Override
    public void onTintChanged(ColorStateList tintList, boolean useLight) {
        ApiCompatibilityUtils.setImageTintList(mMenuImageButton, tintList);
        mUseLightDrawables = useLight;
        updateImageResources();

        if (mLabel != null) mLabel.setTextColor(tintList);
    }

    public void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(this);
            mThemeColorProvider = null;
        }
    }

    /**
     * Creates an {@link AnimatorSet} for showing the update badge that is displayed on top
     * of the app menu button.
     *
     * @param menuButton The {@link View} containing the app menu button.
     * @param menuBadge The {@link View} containing the update badge.
     * @return An {@link AnimatorSet} to run when showing the update badge.
     */
    private static AnimatorSet createShowUpdateBadgeAnimation(
            final View menuButton, final View menuBadge) {
        // Create badge ObjectAnimators.
        ObjectAnimator badgeFadeAnimator = ObjectAnimator.ofFloat(menuBadge, View.ALPHA, 1.f);
        badgeFadeAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);

        int pixelTranslation = menuBadge.getResources().getDimensionPixelSize(
                R.dimen.menu_badge_translation_y_distance);
        ObjectAnimator badgeTranslateYAnimator =
                ObjectAnimator.ofFloat(menuBadge, View.TRANSLATION_Y, pixelTranslation, 0.f);
        badgeTranslateYAnimator.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);

        // Create menu button ObjectAnimator.
        ObjectAnimator menuButtonFadeAnimator = ObjectAnimator.ofFloat(menuButton, View.ALPHA, 0.f);
        menuButtonFadeAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        // Create AnimatorSet and listeners.
        AnimatorSet set = new AnimatorSet();
        set.playTogether(badgeFadeAnimator, badgeTranslateYAnimator, menuButtonFadeAnimator);
        set.setDuration(350);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                // Make sure the menu button is visible again.
                menuButton.setAlpha(1.f);
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                // Jump to the end state if the animation is canceled.
                menuBadge.setAlpha(1.f);
                menuBadge.setTranslationY(0.f);
                menuButton.setAlpha(1.f);
            }
        });

        return set;
    }

    /**
     * Creates an {@link AnimatorSet} for hiding the update badge that is displayed on top
     * of the app menu button.
     *
     * @param menuButton The {@link View} containing the app menu button.
     * @param menuBadge The {@link View} containing the update badge.
     * @return An {@link AnimatorSet} to run when hiding the update badge.
     */
    private static AnimatorSet createHideUpdateBadgeAnimation(
            final View menuButton, final View menuBadge) {
        // Create badge ObjectAnimator.
        ObjectAnimator badgeFadeAnimator = ObjectAnimator.ofFloat(menuBadge, View.ALPHA, 0.f);
        badgeFadeAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);

        // Create menu button ObjectAnimator.
        ObjectAnimator menuButtonFadeAnimator = ObjectAnimator.ofFloat(menuButton, View.ALPHA, 1.f);
        menuButtonFadeAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);

        // Create AnimatorSet and listeners.
        AnimatorSet set = new AnimatorSet();
        set.playTogether(badgeFadeAnimator, menuButtonFadeAnimator);
        set.setDuration(200);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                menuBadge.setVisibility(View.GONE);
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                // Jump to the end state if the animation is canceled.
                menuButton.setAlpha(1.f);
                menuBadge.setVisibility(View.GONE);
            }
        });

        return set;
    }
}
