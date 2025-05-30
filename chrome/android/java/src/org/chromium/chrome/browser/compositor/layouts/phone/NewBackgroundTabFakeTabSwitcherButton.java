// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabAnimationHostView.CROSS_FADE_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.animation.RunOnNextLayout;
import org.chromium.ui.animation.RunOnNextLayoutDelegate;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The fake tab switcher button for the new background tab animation. This is used during the new
 * background tab animation to stand in for the real tab switcher button for a few reasons 1) it
 * avoids having to reach into the toolbar to manipulate it externally, 2) the animation spec calls
 * for scaling up and down the tab switcher button and it is safer to fake this, 3) some variants of
 * the animation don't have a visible toolbar so this fake representation would need to exist
 * regardless.
 */
public class NewBackgroundTabFakeTabSwitcherButton extends FrameLayout implements RunOnNextLayout {
    @VisibleForTesting /* package */ static final long TRANSLATE_DURATION_MS = 200L;
    @VisibleForTesting /* package */ static final long SHRINK_DURATION_MS = 300L;
    @VisibleForTesting /* package */ static final long SCALE_DOWN_DURATION_MS = 50L;

    @IntDef({
        TranslateDirection.UP,
        TranslateDirection.DOWN,
    })
    @Retention(RetentionPolicy.SOURCE)
    /* package */ @interface TranslateDirection {
        int UP = 0;
        int DOWN = 1;
    }

    private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;

    private FrameLayout mInnerContainer;
    private ImageView mTabSwitcherButtonView;
    private TabSwitcherDrawable mTabSwitcherDrawable;

    private @BrandedColorScheme int mBrandedColorScheme;
    private int mTabCount;
    private boolean mIsIncognito;

    /** Default constructor for inflation. */
    public NewBackgroundTabFakeTabSwitcherButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        mRunOnNextLayoutDelegate = new RunOnNextLayoutDelegate(this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mBrandedColorScheme = BrandedColorScheme.LIGHT_BRANDED_THEME;
        mTabCount = 0;
        mIsIncognito = false;
        mTabSwitcherDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        getContext(), mBrandedColorScheme, TabSwitcherDrawableLocation.TAB_TOOLBAR);
        setBrandedColorScheme(mBrandedColorScheme);
        setTabCount(mTabCount, mIsIncognito);
        setNotificationIconStatus(false);

        mInnerContainer = findViewById(R.id.new_tab_indicator_inner_container);
        mTabSwitcherButtonView = findViewById(R.id.fake_tab_switcher_button);
        mTabSwitcherButtonView.setImageDrawable(mTabSwitcherDrawable);
    }

    /* package */ void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        mBrandedColorScheme = brandedColorScheme;
        mTabSwitcherDrawable.setTint(
                ThemeUtils.getThemedToolbarIconTint(getContext(), brandedColorScheme));
        mTabSwitcherDrawable.setNotificationBackground(brandedColorScheme);
    }

    /* package */ void setTabCount(int tabCount, boolean isIncognito) {
        mTabCount = tabCount;
        mIsIncognito = isIncognito;
        mTabSwitcherDrawable.updateForTabCount(tabCount, isIncognito);
        mTabSwitcherDrawable.setIncognitoStatus(isIncognito);
    }

    /* package */ void setNotificationIconStatus(boolean shouldShow) {
        mTabSwitcherDrawable.setNotificationIconStatus(shouldShow);
    }

    /**
     * Sets the background color for the fake tab switcher button container to cover the real tab
     * switcher button.
     *
     * @param color The {@link ColorInt} for the {@link #mInnerContainer} background color.
     */
    /* package */ void setButtonColor(@ColorInt int color) {
        mInnerContainer.setBackgroundColor(color);
    }

    /* package */ AnimatorSet getShrinkAnimator(boolean incrementCount) {
        mTabSwitcherButtonView.setAlpha(1f);

        ObjectAnimator scaleXAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.SCALE_X, 1.6f, 1f);
        ObjectAnimator scaleYAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.SCALE_Y, 1.6f, 1f);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(scaleXAnimator, scaleYAnimator);
        animatorSet.setDuration(SHRINK_DURATION_MS);
        animatorSet.setInterpolator(Interpolators.NEW_BACKGROUND_TAB_ANIMATION_BOUNCE_INTERPOLATOR);

        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        if (incrementCount) {
                            incrementTabCount();
                        }
                    }

                    @Override
                    public void onAnimationCancel(Animator animator) {
                        resetState();
                    }
                });
        return animatorSet;
    }

    /* package */ AnimatorSet getTranslateAnimator(@TranslateDirection int direction) {
        float containerHeight = mInnerContainer.getHeight();
        float viewHeight = mTabSwitcherButtonView.getHeight();
        float distance =
                (containerHeight + viewHeight)
                        / 2f
                        * (direction == TranslateDirection.UP ? -1f : 1f);
        ObjectAnimator translateAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.TRANSLATION_Y, 0f, distance);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(translateAnimator);
        animatorSet.setDuration(TRANSLATE_DURATION_MS);
        animatorSet.setInterpolator(
                Interpolators.NEW_BACKGROUND_TAB_ANIMATION_TRANSLATE_INTERPOLATOR);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationCancel(Animator animator) {
                        resetState();
                    }
                });
        return animatorSet;
    }

    /* package */ AnimatorSet getScaleFadeAnimator() {
        ObjectAnimator fadeAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.ALPHA, 0f, 1f);
        ObjectAnimator scaleXAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.SCALE_X, 0.5f, 1.15f);
        ObjectAnimator scaleYAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.SCALE_Y, 0.5f, 1.15f);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(fadeAnimator, scaleXAnimator, scaleYAnimator);
        animatorSet.setDuration(CROSS_FADE_DURATION_MS);

        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationCancel(Animator animator) {
                        resetState();
                    }
                });
        return animatorSet;
    }

    /* package */ AnimatorSet getScaleDownAnimator() {
        ObjectAnimator scaleXAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.SCALE_X, 1.15f, 1f);
        ObjectAnimator scaleYAnimator =
                ObjectAnimator.ofFloat(mTabSwitcherButtonView, View.SCALE_Y, 1.15f, 1f);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(scaleXAnimator, scaleYAnimator);
        animatorSet.setDuration(SCALE_DOWN_DURATION_MS);

        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationCancel(Animator animator) {
                        resetState();
                    }
                });
        return animatorSet;
    }

    /**
     * Gets the center location for the button.
     *
     * @param location Array to store the x and y position.
     * @param xOffset x-offset to account for cases where the screen can't draw from x = 0 (ex:
     *     on-screen navigation buttons).
     * @param yOffset y-offset to account for the status bar and status indicator (ex: no internet
     *     connection).
     */
    /* package */ void getButtonLocation(int[] location, int xOffset, int yOffset) {
        assert location.length == 2;
        // Difficult for test environment.
        if (!BuildConfig.IS_FOR_TEST) {
            assert mTabSwitcherButtonView.getMeasuredWidth() != 0;
            assert mTabSwitcherButtonView.getMeasuredHeight() != 0;
        }

        mTabSwitcherButtonView.getLocationInWindow(location);
        location[0] += mTabSwitcherButtonView.getMeasuredWidth() / 2 - xOffset;
        location[1] += mTabSwitcherButtonView.getMeasuredHeight() / 2 - yOffset;
    }

    /* package */ void setUpNtpAnimation(boolean incrementCount) {
        if (incrementCount) incrementTabCount();

        Resources res = getContext().getResources();
        mTabSwitcherButtonView.setBackgroundResource(R.drawable.new_tab_animation_rounded_rect);
        mTabSwitcherButtonView.setElevation(
                res.getDimension(R.dimen.new_bg_tab_animation_tab_switcher_elevation));
        int padding = res.getDimensionPixelSize(R.dimen.new_bg_tab_animation_padding);
        mTabSwitcherButtonView.setPadding(padding, padding, padding, padding);
        mTabSwitcherButtonView.setAlpha(0f);

        FrameLayout.LayoutParams tabSwitcherParams =
                (FrameLayout.LayoutParams) mTabSwitcherButtonView.getLayoutParams();
        int size = res.getDimensionPixelSize(R.dimen.new_bg_tab_animation_size);
        tabSwitcherParams.width = size;
        tabSwitcherParams.height = size;
        mTabSwitcherButtonView.setLayoutParams(tabSwitcherParams);
    }

    /* package */ void setMargin(int vertical, int horizontal) {
        FrameLayout.LayoutParams containerParams = (FrameLayout.LayoutParams) getLayoutParams();
        containerParams.topMargin = vertical;
        setLayoutParams(containerParams);

        FrameLayout.LayoutParams innerContainerParams =
                (FrameLayout.LayoutParams) mInnerContainer.getLayoutParams();
        innerContainerParams.leftMargin = horizontal;
        mInnerContainer.setLayoutParams(innerContainerParams);
    }

    private void incrementTabCount() {
        setTabCount(mTabCount + 1, mIsIncognito);
    }

    private void resetState() {
        mTabSwitcherButtonView.setTranslationY(0f);
        mTabSwitcherButtonView.setScaleX(1f);
        mTabSwitcherButtonView.setScaleY(1f);
        mTabSwitcherButtonView.setAlpha(1f);

        mInnerContainer.invalidate();
        mTabSwitcherButtonView.invalidate();
        invalidate();
    }

    @Override
    public void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        runOnNextLayoutRunnables();
    }

    @Override
    public void runOnNextLayout(Runnable runnable) {
        mRunOnNextLayoutDelegate.runOnNextLayout(runnable);
    }

    @Override
    public void runOnNextLayoutRunnables() {
        mRunOnNextLayoutDelegate.runOnNextLayoutRunnables();
    }

    /* package */ @BrandedColorScheme
    int getBrandedColorSchemeForTesting() {
        return mBrandedColorScheme;
    }

    /* package */ int getTabCountForTesting() {
        return mTabCount;
    }

    /* package */ @ColorInt
    int getButtonColorForTesting() {
        ColorDrawable background = (ColorDrawable) mInnerContainer.getBackground();
        return background.getColor();
    }

    /* package */ boolean getShowIconNotificationStatusForTesting() {
        return mTabSwitcherDrawable.getShowIconNotificationStatus();
    }

    /* package */ void setTabSwitcherButtonViewAlphaForTesting(float alpha) {
        mTabSwitcherButtonView.setAlpha(alpha);
    }
}
