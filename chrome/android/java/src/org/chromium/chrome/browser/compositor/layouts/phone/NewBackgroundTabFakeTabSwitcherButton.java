// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
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
 * for delaying the tab count increment until partway through the animation and it is easier to fake
 * this than intercede in the real tab count supplier, 3) some variants of the animation don't have
 * a visible toolbar so this fake representation would need to exist regardless.
 */
public class NewBackgroundTabFakeTabSwitcherButton extends FrameLayout implements RunOnNextLayout {
    @VisibleForTesting /* package */ static final long ROTATE_FADE_IN_DURATION_MS = 250L;
    @VisibleForTesting /* package */ static final long ROTATE_FADE_OUT_DURATION_MS = 400L;
    @VisibleForTesting /* package */ static final long TRANSLATE_DURATION_MS = 300L;

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
    private ImageView mBackgroundView;
    private ImageView mForegroundView;
    private TabSwitcherDrawable mTabSwitcherDrawable;

    private @BrandedColorScheme int mBrandedColorScheme;
    private int mTabCount;
    private boolean mIsIncognito;
    private boolean mHasOutstandingAnimator;

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
        mBackgroundView = findViewById(R.id.very_sunny_background);
        mForegroundView = findViewById(R.id.fake_tab_switcher_button);
        mForegroundView.setImageDrawable(mTabSwitcherDrawable);
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

    /**
     * Returns the {@link AnimatorSet} for the rotate and fade in of {@link #mBackgroundView} "very
     * sunny" asset.
     *
     * @param incrementCount Whether the tab switcher drawable should increment the count.
     */
    /* package */ AnimatorSet getRotateFadeInAnimator(boolean incrementCount) {
        assert !mHasOutstandingAnimator;

        mHasOutstandingAnimator = true;
        setBackgroundVisibility(true);
        mBackgroundView.setAlpha(0f);
        ObjectAnimator rotateAnimator =
                ObjectAnimator.ofFloat(mBackgroundView, View.ROTATION, -20f, 0f);
        ObjectAnimator scaleXAnimator =
                ObjectAnimator.ofFloat(mBackgroundView, View.SCALE_X, 0.2f, 1f);
        ObjectAnimator scaleYAnimator =
                ObjectAnimator.ofFloat(mBackgroundView, View.SCALE_Y, 0.2f, 1f);
        // TODO(crbug.com/40282469): This might need to do a color blend to the toolbar color
        // instead of an alpha fade.
        ObjectAnimator alphaAnimator = ObjectAnimator.ofFloat(mBackgroundView, View.ALPHA, 0f, 1f);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(rotateAnimator, alphaAnimator, scaleXAnimator, scaleYAnimator);
        animatorSet.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        animatorSet.setDuration(ROTATE_FADE_IN_DURATION_MS);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        mBackgroundView.setAlpha(1f);
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

    /**
     * Returns the {@link AnimatorSet} for the rotate and fade out of {@link #mBackgroundView} "very
     * sunny" asset. In order to work, {@link #getRotateFadeInAnimator} should be run first.
     */
    /* package */ AnimatorSet getRotateFadeOutAnimator() {
        assert mHasOutstandingAnimator : "You should run #getRotateFadeInAnimator first";

        // TODO(crbug.com/40282469): This might need to do a color blend to the toolbar color
        // instead of an alpha fade.
        ObjectAnimator alphaAnimator = ObjectAnimator.ofFloat(mBackgroundView, View.ALPHA, 1f, 0f);
        ObjectAnimator rotateAnimator =
                ObjectAnimator.ofFloat(mBackgroundView, View.ROTATION, 0f, 80f);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(rotateAnimator, alphaAnimator);
        animatorSet.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animatorSet.setDuration(ROTATE_FADE_OUT_DURATION_MS);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationCancel(Animator animator) {
                        resetState();
                    }

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        resetState();
                    }
                });
        return animatorSet;
    }

    /* package */ AnimatorSet getTranslateAnimator(
            @TranslateDirection int direction, boolean incrementCount) {
        assert !mHasOutstandingAnimator;

        mHasOutstandingAnimator = true;
        setBackgroundVisibility(true);
        mBackgroundView.setAlpha(0f);
        float size = getContext().getResources().getDimension(R.dimen.toolbar_height_no_shadow);
        float distance = size * (direction == TranslateDirection.UP ? -1f : 1f);
        ObjectAnimator translateAnimator =
                ObjectAnimator.ofFloat(mInnerContainer, View.TRANSLATION_Y, 0f, distance);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(translateAnimator);
        animatorSet.setDuration(TRANSLATE_DURATION_MS);
        animatorSet.setInterpolator(
                Interpolators.NEW_BACKGROUND_TAB_ANIMATION_TRANSLATE_INTERPOLATOR);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animator) {
                        mBackgroundView.setAlpha(1f);
                        // TODO(crbug.com/40282469): This might need to be posted so a visible
                        // update occurs.
                        if (incrementCount) {
                            incrementTabCount();
                        }
                    }

                    @Override
                    public void onAnimationCancel(Animator animator) {
                        resetState();
                    }

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        resetState();
                    }
                });
        return animatorSet;
    }

    /* package */ boolean hasOutstandingAnimator() {
        return mHasOutstandingAnimator;
    }

    /**
     * Gets the center location for the button.
     *
     * @param location Array to store the x and y position.
     * @param yOffset y-offset to account for the status bar and status indicator (ex: no internet
     *     connection).
     */
    /* package */ void getButtonLocation(int[] location, int yOffset) {
        assert location.length == 2;
        // Difficult for test environment.
        if (!BuildConfig.IS_FOR_TEST) {
            assert mForegroundView.getMeasuredWidth() != 0;
            assert mForegroundView.getMeasuredHeight() != 0;
        }

        mForegroundView.getLocationInWindow(location);
        location[0] += mForegroundView.getMeasuredWidth() / 2;
        location[1] += mForegroundView.getMeasuredHeight() / 2 - yOffset;
    }

    /**
     * Returns the side padding between the fake tab switcher button ImageView and the inner
     * container.
     */
    /* package */ int getInnerSidePadding() {
        FrameLayout.LayoutParams innerContainer =
                (FrameLayout.LayoutParams) mInnerContainer.getLayoutParams();
        FrameLayout.LayoutParams foregroundView =
                (FrameLayout.LayoutParams) mForegroundView.getLayoutParams();

        return (innerContainer.width - foregroundView.width) / 2;
    }

    private void incrementTabCount() {
        setTabCount(mTabCount + 1, mIsIncognito);
    }

    private void resetState() {
        setBackgroundVisibility(false);
        mBackgroundView.setAlpha(0f);
        mBackgroundView.setRotation(0f);
        mInnerContainer.setTranslationY(0f);
        mHasOutstandingAnimator = false;

        mInnerContainer.invalidate();
        mForegroundView.invalidate();
        mBackgroundView.invalidate();
        invalidate();
    }

    private void setBackgroundVisibility(boolean visible) {
        // Use invisible to avoid the need for a measure + layout pass for the background.
        mBackgroundView.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
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
}
