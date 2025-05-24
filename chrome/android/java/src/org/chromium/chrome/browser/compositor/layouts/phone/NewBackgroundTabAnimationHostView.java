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
import android.view.animation.Interpolator;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.NewTabAnimationUtils.NewTabAnim;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.animation.ViewCurvedMotionAnimatorFactory;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Host view for the new background tab animation. */
public class NewBackgroundTabAnimationHostView extends FrameLayout {
    /* package */ static final long CROSS_FADE_DURATION_MS = 150L;
    private static final long PATH_ARC_DURATION_MS = 300L;
    private static final long NEW_PATH_ARC_DURATION_MS = 400L;
    private static final long LINK_SCALE_DURATION_MS = 160L;
    private static final long NEW_LINK_SCALE_DURATION_MS = 192L;
    private static final long TRANSLATE_DELAY_DURATION_MS = 100L;
    private static final long SHRINK_DELAY_DURATION_MS = 50L;

    @IntDef({
        AnimationType.UNINITIALIZED,
        AnimationType.DEFAULT,
        AnimationType.NTP_PARTIAL_SCROLL,
        AnimationType.NTP_FULL_SCROLL,
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface AnimationType {
        int UNINITIALIZED = 0;
        int DEFAULT = 1;
        int NTP_PARTIAL_SCROLL = 2;
        int NTP_FULL_SCROLL = 3;
    }

    private NewBackgroundTabFakeTabSwitcherButton mFakeTabSwitcherButton;
    private ImageView mLinkIcon;
    private @AnimationType int mAnimationType;
    private boolean mIsTopToolbar;
    private int mStatusBarHeight;
    private int mXOffset;

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
    }

    /**
     * Returns the {@link AnimatorSet} for the new background tab animation.
     *
     * @param originX x-coordinate for the start point.
     * @param originY y-coordinate for the start point.
     */
    /* package */ AnimatorSet getAnimatorSet(float originX, float originY) {
        assert mAnimationType != AnimationType.UNINITIALIZED;
        int[] target = new int[2];
        mFakeTabSwitcherButton.getButtonLocation(target, mXOffset, mStatusBarHeight);
        target[0] -= Math.round(mLinkIcon.getWidth() / 2f);
        target[1] -= Math.round(mLinkIcon.getHeight() / 2f);

        // TODO(crbug.com/419065710): Clean up versions.
        boolean isNewDuration = ChromeFeatureList.sShowNewTabAnimationsNewDuration.getValue();
        @NewTabAnim int version = ChromeFeatureList.sShowNewTabAnimationsVersion.getValue();
        long pathDuration = isNewDuration ? NEW_PATH_ARC_DURATION_MS : PATH_ARC_DURATION_MS;

        AnimatorSet transitionAnimator = getTransitionAnimator(isNewDuration);
        ObjectAnimator pathAnimator =
                getPathArcAnimator(originX, originY, target[0], target[1], pathDuration, version);
        AnimatorSet backgroundAnimation = new AnimatorSet();
        AnimatorSet fakeTabSwitcherAnimator;

        if (mAnimationType == AnimationType.DEFAULT) {
            fakeTabSwitcherAnimator =
                    mFakeTabSwitcherButton.getShrinkAnimator(/* incrementCount= */ true);

            if (version == NewTabAnim.BOUNCE
                    || version == NewTabAnim.BOUNCE_DECELERATE_WITH_DELAY) {
                fakeTabSwitcherAnimator.setInterpolator(
                        Interpolators.NEW_BACKGROUND_TAB_ANIMATION_BOUNCE_INTERPOLATOR);
                fakeTabSwitcherAnimator.setStartDelay(SHRINK_DELAY_DURATION_MS);
                transitionAnimator.setStartDelay(SHRINK_DELAY_DURATION_MS);
            } else if (version == NewTabAnim.BOUNCE_DECELERATE) {
                fakeTabSwitcherAnimator.setInterpolator(
                        Interpolators.NEW_BACKGROUND_TAB_ANIMATION_BOUNCE_INTERPOLATOR);
            } else {
                fakeTabSwitcherAnimator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
            }
            backgroundAnimation
                    .play(transitionAnimator)
                    .with(fakeTabSwitcherAnimator)
                    .after(pathAnimator);
        } else {
            transitionAnimator.setStartDelay(pathDuration - CROSS_FADE_DURATION_MS);
            fakeTabSwitcherAnimator =
                    mFakeTabSwitcherButton.getTranslateAnimator(
                            NewBackgroundTabFakeTabSwitcherButton.TranslateDirection.UP);
            fakeTabSwitcherAnimator.setStartDelay(TRANSLATE_DELAY_DURATION_MS);
            AnimatorSet scaleAnimator = mFakeTabSwitcherButton.getScaleDownAnimator();

            backgroundAnimation
                    .play(pathAnimator)
                    .with(transitionAnimator)
                    .before(scaleAnimator)
                    .before(fakeTabSwitcherAnimator);
        }
        return backgroundAnimation;
    }

    /**
     * Prepares the animation.
     *
     * @param tabSwitcherButton The real Tab Switcher Button.
     * @param isNtp True if the current tab is the regular Ntp.
     * @param isIncognito True if the current tab is an incognito tab.
     * @param isTopToolbar True if current tab has a top toolbar.
     * @param backgroundColor The current color of the toolbar.
     * @param tabCount The tab count to display.
     * @param toolbarHeight Current height of the toolbar in the screen (absolute y-coordinate in
     *     the screen).
     * @param statusBarHeight The status bar height to calculate the y-offset within the screen.
     * @param xOffset Offset for cases where the screen can't draw from x = 0.
     * @param ntpToolbarTransitionPercentage To know if the search box is in the toolbar position.
     */
    /* package */ void setUpAnimation(
            ToggleTabStackButton tabSwitcherButton,
            boolean isNtp,
            boolean isIncognito,
            boolean isTopToolbar,
            @ColorInt int backgroundColor,
            int tabCount,
            int toolbarHeight,
            int statusBarHeight,
            int xOffset,
            float ntpToolbarTransitionPercentage) {
        mStatusBarHeight = statusBarHeight;
        mXOffset = xOffset;
        mIsTopToolbar = isTopToolbar;
        mFakeTabSwitcherButton.setTabCount(tabCount, isIncognito);

        Context context = getContext();
        @BrandedColorScheme
        int brandedColorScheme =
                ThemeUtils.getBrandedColorScheme(context, backgroundColor, isIncognito);
        mFakeTabSwitcherButton.setBrandedColorScheme(brandedColorScheme);

        Rect tabSwitcherRect = new Rect();
        boolean tabSwitcherButtonIsVisible =
                tabSwitcherButton.getGlobalVisibleRect(tabSwitcherRect);
        int horizontalMargin = tabSwitcherRect.left - xOffset;
        int verticalMargin = toolbarHeight - statusBarHeight;

        if (tabSwitcherButtonIsVisible || !isNtp) {
            mAnimationType = AnimationType.DEFAULT;
            mFakeTabSwitcherButton.setButtonColor(backgroundColor);
            mFakeTabSwitcherButton.setNotificationIconStatus(
                    tabSwitcherButton.shouldShowNotificationIcon());
        } else {
            mFakeTabSwitcherButton.setUpNtpAnimation(/* incrementCount= */ true);
            if (ntpToolbarTransitionPercentage == 1f) {
                mAnimationType = AnimationType.NTP_FULL_SCROLL;
                verticalMargin +=
                        Math.round(
                                context.getResources()
                                        .getDimension(R.dimen.toolbar_height_no_shadow));
            } else {
                mAnimationType = AnimationType.NTP_PARTIAL_SCROLL;
            }
        }
        mFakeTabSwitcherButton.setMargin(verticalMargin, horizontalMargin);
    }

    /**
     * Returns the {@link ObjectAnimator} for the path arc animation.
     *
     * @param originX x-coordinate for the start point.
     * @param originY y-coordinate for the start point.
     * @param finalX x-coordinate for the end point.
     * @param finalY y-coordinate for the end point.
     * @param duration Animation duration in ms.
     * @param version The {@link NewTabAnim} animation version.
     */
    private ObjectAnimator getPathArcAnimator(
            float originX,
            float originY,
            float finalX,
            float finalY,
            long duration,
            @NewTabAnim int version) {
        boolean isClockwise = mIsTopToolbar ? (originX >= finalX) : (originX <= finalX);

        ObjectAnimator animator =
                ViewCurvedMotionAnimatorFactory.build(
                        mLinkIcon, originX, originY, finalX, finalY, isClockwise);
        animator.setDuration(duration);

        Interpolator pathInterpolator;
        pathInterpolator =
                switch (version) {
                    case NewTabAnim.BOUNCE -> Interpolators
                            .NEW_BACKGROUND_TAB_ANIMATION_SECOND_PATH_INTERPOLATOR;
                    case NewTabAnim.DECELERATE,
                            NewTabAnim.BOUNCE_DECELERATE,
                            NewTabAnim.BOUNCE_DECELERATE_WITH_DELAY -> Interpolators
                            .EMPHASIZED_DECELERATE;
                    default -> Interpolators.NEW_BACKGROUND_TAB_ANIMATION_PATH_INTERPOLATOR;
                };
        animator.setInterpolator(pathInterpolator);

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
    private AnimatorSet getTransitionAnimator(boolean isNewDuration) {
        AnimatorSet animatorSet = new AnimatorSet();
        ObjectAnimator fadeAnimator = ObjectAnimator.ofFloat(mLinkIcon, View.ALPHA, 1f, 0f);

        if (mAnimationType == AnimationType.DEFAULT) {
            mLinkIcon.setPivotX(mLinkIcon.getMeasuredWidth() / 2f);
            mLinkIcon.setPivotY(mLinkIcon.getMeasuredHeight() / 2f);

            ObjectAnimator scaleXAnimator = ObjectAnimator.ofFloat(mLinkIcon, View.SCALE_X, 1f, 0f);
            ObjectAnimator scaleYAnimator = ObjectAnimator.ofFloat(mLinkIcon, View.SCALE_Y, 1f, 0f);
            animatorSet.playTogether(scaleXAnimator, scaleYAnimator, fadeAnimator);
            animatorSet.setDuration(
                    isNewDuration ? NEW_LINK_SCALE_DURATION_MS : LINK_SCALE_DURATION_MS);
            animatorSet.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        } else {
            AnimatorSet fakeTabSwitcherAnimator = mFakeTabSwitcherButton.getScaleFadeAnimator();
            animatorSet.playTogether(fakeTabSwitcherAnimator, fadeAnimator);
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
