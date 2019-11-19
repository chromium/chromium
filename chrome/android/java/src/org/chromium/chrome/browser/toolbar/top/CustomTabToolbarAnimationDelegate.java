// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * A delegate class to handle the title animation and security icon fading animation in
 * {@link CustomTabToolbar}.
 * <p>
 * How does the title animation work?
 * <p>
 * 1. The title bar is set from {@link View#GONE} to {@link View#VISIBLE}, which triggers a relayout
 * of the location bar. 2. On relayout, the newly positioned urlbar will be moved&scaled to look
 * exactly the same as before the relayout. (Note the scale factor is calculated based on
 * {@link TextView#getTextSize()}, not height or width.) 3. Finally the urlbar will be animated to
 * its new position.
 */
class CustomTabToolbarAnimationDelegate {
    private static final int CUSTOM_TAB_TOOLBAR_SLIDE_DURATION_MS = 200;
    private static final int CUSTOM_TAB_TOOLBAR_FADE_DURATION_MS = 150;

    private final View mSecurityButton;
    private final View mTitleUrlContainer;
    private final AnimatorSet mSecurityButtonShowAnimator;
    private final AnimatorSet mSecurityButtonHideAnimator;

    private TextView mUrlBar;
    private TextView mTitleBar;
    private int mSecurityButtonWidth;
    // A flag controlling whether the animation has run before.
    private boolean mShouldRunTitleAnimation;

    /**
     * Constructs an instance of {@link CustomTabToolbarAnimationDelegate}.
     */
    CustomTabToolbarAnimationDelegate(View securityButton, final View titleUrlContainer) {
        mSecurityButton = securityButton;
        mTitleUrlContainer = titleUrlContainer;
        mSecurityButtonWidth = securityButton.getResources().getDimensionPixelSize(
                R.dimen.location_bar_icon_width);

        titleUrlContainer.setTranslationX(-mSecurityButtonWidth);

        mSecurityButtonShowAnimator = new AnimatorSet();
        Animator translateRight = ObjectAnimator.ofFloat(titleUrlContainer, View.TRANSLATION_X, 0);
        translateRight.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        translateRight.setDuration(CUSTOM_TAB_TOOLBAR_SLIDE_DURATION_MS);

        Animator fadeIn = ObjectAnimator.ofFloat(mSecurityButton, View.ALPHA, 1);
        fadeIn.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        fadeIn.setDuration(CUSTOM_TAB_TOOLBAR_FADE_DURATION_MS);
        fadeIn.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                mSecurityButton.setVisibility(View.VISIBLE);
            }
        });
        mSecurityButtonShowAnimator.playSequentially(translateRight, fadeIn);

        mSecurityButtonHideAnimator = new AnimatorSet();
        Animator fadeOut = ObjectAnimator.ofFloat(mSecurityButton, View.ALPHA, 0);
        fadeOut.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        fadeOut.setDuration(CUSTOM_TAB_TOOLBAR_FADE_DURATION_MS);
        fadeOut.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onEnd(Animator animation) {
                mSecurityButton.setVisibility(View.INVISIBLE);
            }
        });

        Animator translateLeft = ObjectAnimator.ofFloat(
                titleUrlContainer, View.TRANSLATION_X, -mSecurityButtonWidth);
        translateLeft.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        translateLeft.setDuration(CUSTOM_TAB_TOOLBAR_SLIDE_DURATION_MS);
        mSecurityButtonHideAnimator.playSequentially(fadeOut, translateLeft);
    }

    /**
     * Sets whether the title scaling animation is enabled.
     */
    void setTitleAnimationEnabled(boolean enabled) {
        mShouldRunTitleAnimation = enabled;
    }

    void prepareTitleAnim(TextView urlBar, TextView titleBar) {
        mTitleBar = titleBar;
        mUrlBar = urlBar;
        mUrlBar.setPivotX(0f);
        mUrlBar.setPivotY(0f);
        mShouldRunTitleAnimation = true;
    }

    /**
     * Starts animation for urlbar scaling and title fading-in. If this animation has already run
     * once, does nothing.
     */
    void startTitleAnimation(Context context) {
        if (!mShouldRunTitleAnimation) return;
        mShouldRunTitleAnimation = false;

        mTitleBar.setVisibility(View.VISIBLE);
        mTitleBar.setAlpha(0f);

        float newSizeSp = context.getResources().getDimension(R.dimen.custom_tabs_url_text_size);

        float oldSizePx = mUrlBar.getTextSize();
        mUrlBar.setTextSize(TypedValue.COMPLEX_UNIT_PX, newSizeSp);
        float newSizePx = mUrlBar.getTextSize();
        final float scale = oldSizePx / newSizePx;

        // View#getY() cannot be used because the boundary of the parent will change after relayout.
        final int[] oldLoc = new int[2];
        mUrlBar.getLocationInWindow(oldLoc);

        mUrlBar.requestLayout();

        mUrlBar.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                mUrlBar.removeOnLayoutChangeListener(this);

                int[] newLoc = new int[2];
                mUrlBar.getLocationInWindow(newLoc);

                mUrlBar.setScaleX(scale);
                mUrlBar.setScaleY(scale);
                mUrlBar.setTranslationX(oldLoc[0] - newLoc[0]);
                mUrlBar.setTranslationY(oldLoc[1] - newLoc[1]);

                mUrlBar.animate()
                        .scaleX(1f)
                        .scaleY(1f)
                        .translationX(0)
                        .translationY(0)
                        .setDuration(CUSTOM_TAB_TOOLBAR_SLIDE_DURATION_MS)
                        .setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE)
                        .setListener(new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(Animator animation) {
                                mTitleBar.animate()
                                        .alpha(1f)
                                        .setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE)
                                        .setDuration(CUSTOM_TAB_TOOLBAR_FADE_DURATION_MS)
                                        .start();
                            }
                        })
                        .start();
            }
        });
    }

    /**
     * Starts the animation to show the security button.
     */
    void showSecurityButton() {
        if (mSecurityButtonHideAnimator.isStarted()) mSecurityButtonHideAnimator.cancel();
        if (mSecurityButtonShowAnimator.isStarted()
                || mSecurityButton.getVisibility() == View.VISIBLE) {
            return;
        }
        mSecurityButtonShowAnimator.start();
    }

    /**
     * Starts the animation to hide the security button.
     */
    void hideSecurityButton() {
        if (mSecurityButtonShowAnimator.isStarted()) mSecurityButtonShowAnimator.cancel();
        if (mSecurityButtonHideAnimator.isStarted()
                || mTitleUrlContainer.getTranslationX() == -mSecurityButtonWidth) {
            return;
        }
        mSecurityButtonHideAnimator.start();
    }
}
