// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.animation.ViewCurvedMotionAnimatorFactory;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Host view for the new background tab animation. */
public class NewBackgroundTabAnimationHostView extends FrameLayout {
    private static final long CURVED_MOTION_DURATION_MS = 400L;
    private static final long LINK_SCALE_DURATION_MS = 120L;
    private final int[] mTargetLocation = new int[2];

    @IntDef({
        AnimationType.UNINITIALIZED,
        AnimationType.DEFAULT,
        AnimationType.NTP_PARTIAL_SCROLL,
        AnimationType.NTP_FULL_SCROLL,
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    /*package*/ @interface AnimationType {
        int UNINITIALIZED = 0;
        int DEFAULT = 1;
        int NTP_PARTIAL_SCROLL = 2;
        int NTP_FULL_SCROLL = 3;
    }

    private NewBackgroundTabFakeTabSwitcherButton mFakeTabSwitcherButton;
    private ImageView mLinkIcon;
    private @AnimationType int mAnimationType;
    private boolean mIsRtl;
    private int mYOffset;

    /** Default constructor for inflation. */
    public NewBackgroundTabAnimationHostView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAnimationType = AnimationType.UNINITIALIZED;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLinkIcon = findViewById(R.id.new_tab_background_animation_link_icon);
        setLinkIconTint(SemanticColorUtils.getDefaultIconColor(getContext()));
        mFakeTabSwitcherButton = findViewById(R.id.new_background_tab_fake_tab_switcher_button);
        mIsRtl = LocalizationUtils.isLayoutRtl();
    }

    /**
     * Returns the {@link AnimatorSet} for the new background tab animation.
     *
     * @param originX x-coordinate for the start point.
     * @param originY y-coordinate for the start point.
     * @param statusBarHeight The status bar height (px), if needed for y-offset.
     */
    /* package */ AnimatorSet getAnimatorSet(float originX, float originY, int statusBarHeight) {
        // TODO(crbug.com/40282469): Implement animation for NTP partial and full scroll version.
        // Also, make animation compatible with bottom toolbar.
        assert mAnimationType != AnimationType.UNINITIALIZED;
        AnimatorSet backgroundAnimation = new AnimatorSet();

        if (mAnimationType == AnimationType.DEFAULT) {
            mFakeTabSwitcherButton.getButtonLocation(mTargetLocation, mYOffset + statusBarHeight);
            int endX = mTargetLocation[0] - mLinkIcon.getWidth() / 2;
            int endY = mTargetLocation[1];

            ObjectAnimator curvedAnimator = getCurvedMotionAnimator(originX, originY, endX, endY);
            AnimatorSet transitionAnimator = getTransitionAnimator();
            AnimatorSet fadeOutAnimator = mFakeTabSwitcherButton.getRotateFadeOutAnimator();
            backgroundAnimation.playSequentially(
                    curvedAnimator, transitionAnimator, fadeOutAnimator);
        }

        return backgroundAnimation;
    }

    /**
     * Sets the {@link #mFakeTabSwitcherButton} into the correct status.
     *
     * @param tabSwitcherButton The real Tab Switcher Button.
     * @param tabCount The tab count to display.
     * @param backgroundColor The current color of the toolbar.
     * @param isIncognito true if the current tab is an incognito tab.
     * @param yOffset y-offset to account for the status indicator (ex: no internet connection).
     */
    /* package */ void updateFakeTabSwitcherButton(
            ToggleTabStackButton tabSwitcherButton,
            int tabCount,
            @ColorInt int backgroundColor,
            boolean isIncognito,
            int yOffset) {
        mYOffset = yOffset;
        mFakeTabSwitcherButton.setTabCount(tabCount, isIncognito);

        Rect tabSwitcherRect = new Rect();
        boolean isVisible = tabSwitcherButton.getGlobalVisibleRect(tabSwitcherRect);
        int fakeButtonSideMargin = mFakeTabSwitcherButton.getInnerSidePadding();
        int x = tabSwitcherRect.left - fakeButtonSideMargin;

        if (isVisible) {
            mAnimationType = AnimationType.DEFAULT;
            FrameLayout.LayoutParams params =
                    (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
            params.leftMargin = x;
            params.topMargin = yOffset;
            mFakeTabSwitcherButton.setLayoutParams(params);

            @BrandedColorScheme
            int brandedColorScheme =
                    ThemeUtils.getBrandedColorScheme(getContext(), backgroundColor, isIncognito);
            mFakeTabSwitcherButton.setBrandedColorScheme(brandedColorScheme);

            mFakeTabSwitcherButton.setButtonColor(backgroundColor);
            mFakeTabSwitcherButton.setNotificationIconStatus(
                    tabSwitcherButton.shouldShowNotificationIcon());
            mFakeTabSwitcherButton.setVisibility(View.VISIBLE);
        } else {
            // TODO(crbug.com/40282469): Implement this for NTP partial and scroll version.
            mAnimationType = AnimationType.NTP_FULL_SCROLL;
        }
    }

    /**
     * Returns the {@link ObjectAnimator} for the curved motion animation.
     *
     * @param originX x-coordinate for the start point.
     * @param originY y-coordinate for the start point.
     * @param finalX x-coordinate for the end point.
     * @param finalY y-coordinate for the end point.
     */
    private ObjectAnimator getCurvedMotionAnimator(
            float originX, float originY, float finalX, float finalY) {
        ObjectAnimator animator =
                ViewCurvedMotionAnimatorFactory.build(
                        mLinkIcon, originX, originY, finalX, finalY, /* isClockwise= */ mIsRtl);
        animator.setDuration(CURVED_MOTION_DURATION_MS);
        animator.setInterpolator(Interpolators.NEW_BACKGROUND_TAB_ANIMATION_PATH_INTERPOLATOR);
        animator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        mLinkIcon.setVisibility(View.VISIBLE);
                    }
                });

        return animator;
    }

    /**
     * Returns the {@link AnimatorSet} for the transition between the Link icon and the "very sunny"
     * asset. {@link #mLinkIcon} will do a scale down animation and {@link #mFakeTabSwitcherButton}
     * will start the rotate fade in animation.
     */
    private AnimatorSet getTransitionAnimator() {
        mLinkIcon.setPivotX(mLinkIcon.getMeasuredWidth() / 2f);
        mLinkIcon.setPivotY(0f);

        AnimatorSet rotateFadeInAnimator =
                mFakeTabSwitcherButton.getRotateFadeInAnimator(/* incrementCount= */ true);
        ObjectAnimator scaleXAnimator = ObjectAnimator.ofFloat(mLinkIcon, View.SCALE_X, 1f, 0f);
        scaleXAnimator.setDuration(LINK_SCALE_DURATION_MS);
        ObjectAnimator scaleYAnimator = ObjectAnimator.ofFloat(mLinkIcon, View.SCALE_Y, 1f, 0f);
        scaleYAnimator.setDuration(LINK_SCALE_DURATION_MS);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(rotateFadeInAnimator, scaleXAnimator, scaleYAnimator);
        return animatorSet;
    }

    /**
     * Sets the tint for {@link #mLinkIcon}.
     *
     * @param color The {@link ColorInt} for the tint.
     */
    private void setLinkIconTint(@ColorInt int color) {
        mLinkIcon.setImageTintList(ColorStateList.valueOf(color));
    }

    public @AnimationType int getAnimationTypeForTesting() {
        return mAnimationType;
    }
}
