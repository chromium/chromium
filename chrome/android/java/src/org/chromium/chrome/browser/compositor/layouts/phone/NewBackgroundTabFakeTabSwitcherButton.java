// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

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
    @VisibleForTesting protected static final long ANIMATION_DURATION_MS = 100L;

    @IntDef({
        TranslateDirection.UP,
        TranslateDirection.DOWN,
    })
    @Retention(RetentionPolicy.SOURCE)
    /*package*/ @interface TranslateDirection {
        int UP = 0;
        int DOWN = 1;
    }

    private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;

    private FrameLayout mInnerContainer;
    private ImageView mBackgroundView;
    private ImageView mForegroundView;
    private TabSwitcherDrawable mTabSwitcherDrawable;

    private int mTabCount;
    private boolean mIsIncognito;
    private boolean mHasOutstandingAnimator;

    /** Default constructor for inflation. */
    public NewBackgroundTabFakeTabSwitcherButton(Context context, AttributeSet atts) {
        super(context, atts);
        mRunOnNextLayoutDelegate = new RunOnNextLayoutDelegate(this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTabSwitcherDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        getContext(),
                        BrandedColorScheme.LIGHT_BRANDED_THEME,
                        TabSwitcherDrawableLocation.TAB_TOOLBAR);
        setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        setTabCount(0, false);
        setNotificationIconStatus(false);

        mInnerContainer = findViewById(R.id.new_tab_indicator_inner_container);
        mBackgroundView = findViewById(R.id.very_sunny_background);
        mForegroundView = findViewById(R.id.fake_tab_switcher_button);
        mForegroundView.setImageDrawable(mTabSwitcherDrawable);
    }

    /* package */ void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
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

    /* package */ AnimatorSet getRotateAnimator(boolean incrementCount) {
        assert !mHasOutstandingAnimator;

        mHasOutstandingAnimator = true;
        setBackgroundVisibility(true);
        mBackgroundView.setAlpha(0f);
        ObjectAnimator rotateAnimator =
                ObjectAnimator.ofFloat(mBackgroundView, View.ROTATION, 0f, 30f);
        // TODO(crbug.com/40282469): This might need to do a color blend to the toolbar color
        // instead of an alpha fade.
        ObjectAnimator alphaAnimator = ObjectAnimator.ofFloat(mBackgroundView, View.ALPHA, 1f, 0f);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(rotateAnimator, alphaAnimator);
        animatorSet.setDuration(ANIMATION_DURATION_MS);
        animatorSet.setInterpolator(Interpolators.EMPHASIZED);
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
        animatorSet.setDuration(ANIMATION_DURATION_MS);
        animatorSet.setInterpolator(Interpolators.EMPHASIZED);
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

    private void incrementTabCount() {
        setTabCount(mTabCount + 1, mIsIncognito);
    }

    private void resetState() {
        // TODO(crbug.com/40282469): when positioned over the toolbar we might need the background
        // to be the color of the toolbar rather than invisible to mask the real tab switcher
        // button.
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
}
