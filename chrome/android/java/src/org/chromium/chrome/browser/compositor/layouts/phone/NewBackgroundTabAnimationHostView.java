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
    private static final long CROSS_FADE_DURATION_MS = 150L;
    private static final long SCALE_DURATION_MS = 50L;
    private static final long DELAY_DURATION_MS = 100L;

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
        // TODO(crbug.com/40282469): Make animation compatible with bottom toolbar.
        assert mAnimationType != AnimationType.UNINITIALIZED;
        int[] target = new int[2];
        mFakeTabSwitcherButton.getButtonLocation(target, mYOffset + statusBarHeight);
        target[0] -= mLinkIcon.getWidth() / 2;
        target[1] -=
                mAnimationType == AnimationType.NTP_FULL_SCROLL ? mLinkIcon.getHeight() / 2 : 0;

        AnimatorSet transitionAnimator = getTransitionAnimator();
        ObjectAnimator curvedAnimator =
                getCurvedMotionAnimator(originX, originY, target[0], target[1]);

        AnimatorSet backgroundAnimation = new AnimatorSet();
        AnimatorSet fakeTabSwitcherAnimator;

        if (mAnimationType == AnimationType.DEFAULT) {
            fakeTabSwitcherAnimator = mFakeTabSwitcherButton.getRotateFadeOutAnimator();
            backgroundAnimation.playSequentially(
                    curvedAnimator, transitionAnimator, fakeTabSwitcherAnimator);
        } else {
            transitionAnimator.setStartDelay(CURVED_MOTION_DURATION_MS - CROSS_FADE_DURATION_MS);
            fakeTabSwitcherAnimator =
                    mFakeTabSwitcherButton.getTranslateAnimator(
                            NewBackgroundTabFakeTabSwitcherButton.TranslateDirection.UP, true);
            fakeTabSwitcherAnimator.setStartDelay(DELAY_DURATION_MS);

            ObjectAnimator scaleXAnimator =
                    ObjectAnimator.ofFloat(mFakeTabSwitcherButton, View.SCALE_X, 1.15f, 1f);
            ObjectAnimator scaleYAnimator =
                    ObjectAnimator.ofFloat(mFakeTabSwitcherButton, View.SCALE_Y, 1.15f, 1f);
            AnimatorSet scaleAnimator = new AnimatorSet();
            scaleAnimator.playTogether(scaleXAnimator, scaleYAnimator);
            scaleAnimator.setDuration(SCALE_DURATION_MS);

            backgroundAnimation
                    .play(curvedAnimator)
                    .with(transitionAnimator)
                    .before(scaleAnimator)
                    .before(fakeTabSwitcherAnimator);
        }
        return backgroundAnimation;
    }

    /**
     * Sets the {@link #mFakeTabSwitcherButton} into the correct status.
     *
     * @param tabSwitcherButton The real Tab Switcher Button.
     * @param tabCount The tab count to display.
     * @param backgroundColor The current color of the toolbar.
     * @param isNtp True if the current tab is the regular Ntp.
     * @param isIncognito True if the current tab is an incognito tab.
     * @param yOffset y-offset to account for the status indicator (ex: no internet connection).
     * @param ntpToolbarTransitionPercentage To know if the search box is in the toolbar position.
     */
    /* package */ void updateFakeTabSwitcherButton(
            ToggleTabStackButton tabSwitcherButton,
            int tabCount,
            @ColorInt int backgroundColor,
            boolean isNtp,
            boolean isIncognito,
            int yOffset,
            float ntpToolbarTransitionPercentage) {
        mYOffset = yOffset;
        mFakeTabSwitcherButton.setTabCount(tabCount, isIncognito);

        Rect tabSwitcherRect = new Rect();
        boolean tabIconIsVisible = tabSwitcherButton.getGlobalVisibleRect(tabSwitcherRect);
        int fakeButtonSideMargin = mFakeTabSwitcherButton.getInnerSidePadding();
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFakeTabSwitcherButton.getLayoutParams();
        params.leftMargin = tabSwitcherRect.left - fakeButtonSideMargin;
        params.topMargin = yOffset;

        if (tabIconIsVisible || !isNtp) {
            mAnimationType = AnimationType.DEFAULT;
            @BrandedColorScheme
            int brandedColorScheme =
                    ThemeUtils.getBrandedColorScheme(getContext(), backgroundColor, isIncognito);
            mFakeTabSwitcherButton.setBrandedColorScheme(brandedColorScheme);
            mFakeTabSwitcherButton.setButtonColor(backgroundColor);
            mFakeTabSwitcherButton.setNotificationIconStatus(
                    tabSwitcherButton.shouldShowNotificationIcon());
        } else {
            if (ntpToolbarTransitionPercentage == 1f) {
                mAnimationType = AnimationType.NTP_FULL_SCROLL;
                params.topMargin +=
                        Math.round(
                                getContext()
                                        .getResources()
                                        .getDimension(R.dimen.toolbar_height_no_shadow));
            } else {
                mAnimationType = AnimationType.NTP_PARTIAL_SCROLL;
            }
            mFakeTabSwitcherButton.setAlpha(0f);
        }
        mFakeTabSwitcherButton.setLayoutParams(params);
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
     * Returns the {@link AnimatorSet} for the transition between the Link icon and the Tab icon.
     */
    private AnimatorSet getTransitionAnimator() {
        AnimatorSet animatorSet = new AnimatorSet();

        if (mAnimationType == AnimationType.DEFAULT) {
            mLinkIcon.setPivotX(mLinkIcon.getMeasuredWidth() / 2f);
            mLinkIcon.setPivotY(0f);

            AnimatorSet rotateFadeInAnimator =
                    mFakeTabSwitcherButton.getRotateFadeInAnimator(/* incrementCount= */ true);
            ObjectAnimator scaleXAnimator = ObjectAnimator.ofFloat(mLinkIcon, View.SCALE_X, 1f, 0f);
            scaleXAnimator.setDuration(LINK_SCALE_DURATION_MS);
            ObjectAnimator scaleYAnimator = ObjectAnimator.ofFloat(mLinkIcon, View.SCALE_Y, 1f, 0f);
            scaleYAnimator.setDuration(LINK_SCALE_DURATION_MS);

            animatorSet.playTogether(rotateFadeInAnimator, scaleXAnimator, scaleYAnimator);
        } else {
            ObjectAnimator alphaLinkAnimator =
                    ObjectAnimator.ofFloat(mLinkIcon, View.ALPHA, 1f, 0f);
            ObjectAnimator alphaTabSwitcherAnimator =
                    ObjectAnimator.ofFloat(mFakeTabSwitcherButton, View.ALPHA, 0f, 1f);

            ObjectAnimator scaleXAnimator =
                    ObjectAnimator.ofFloat(mFakeTabSwitcherButton, View.SCALE_X, 0.5f, 1.15f);
            ObjectAnimator scaleYAnimator =
                    ObjectAnimator.ofFloat(mFakeTabSwitcherButton, View.SCALE_Y, 0.5f, 1.15f);

            animatorSet.playTogether(
                    scaleXAnimator, scaleYAnimator, alphaLinkAnimator, alphaTabSwitcherAnimator);
            animatorSet.setDuration(CROSS_FADE_DURATION_MS);
        }
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

    /* package */ @AnimationType
    int getAnimationTypeForTesting() {
        return mAnimationType;
    }
}
