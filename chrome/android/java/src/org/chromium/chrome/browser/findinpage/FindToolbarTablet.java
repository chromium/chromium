// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;

/**
 * A tablet specific version of the {@link FindToolbar}.
 */
public class FindToolbarTablet extends FindToolbar {
    private static final int ENTER_EXIT_ANIMATION_DURATION_MS = 200;
    private static final int MAKE_ROOM_ANIMATION_DURATION_MS = 200;

    private static final float Y_INSET_DP = 8.f;

    private ObjectAnimator mCurrentAnimation;

    private ObjectAnimator mAnimationEnter;
    private ObjectAnimator mAnimationLeave;

    private final int mYInsetPx;

    /**
     * Creates an instance of a {@link FindToolbarTablet}.
     * @param context The Context to create the {@link FindToolbarTablet} under.
     * @param attrs The AttributeSet used to create the {@link FindToolbarTablet}.
     */
    public FindToolbarTablet(Context context, AttributeSet attrs) {
        super(context, attrs);

        mYInsetPx = (int) (context.getResources().getDisplayMetrics().density * Y_INSET_DP);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        setVisibility(View.GONE);

        Resources resources = getContext().getResources();
        int width = resources.getDimensionPixelSize(R.dimen.find_in_page_popup_width);
        int endMargin = resources.getDimensionPixelOffset(R.dimen.find_in_page_popup_margin_end);
        int translateWidth = width + endMargin;

        mAnimationEnter = ObjectAnimator.ofFloat(this, View.TRANSLATION_X, translateWidth, 0);
        mAnimationEnter.setDuration(ENTER_EXIT_ANIMATION_DURATION_MS);
        mAnimationEnter.setInterpolator(Interpolators.DECELERATE_INTERPOLATOR);
        mAnimationEnter.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animation) {
                setVisibility(View.VISIBLE);
                postInvalidateOnAnimation();
                FindToolbarTablet.super.handleActivate();
            }

            @Override
            public void onEnd(Animator animation) {
                mCurrentAnimation = null;
            }
        });

        mAnimationLeave = ObjectAnimator.ofFloat(this, View.TRANSLATION_X, 0, translateWidth);
        mAnimationLeave.setDuration(ENTER_EXIT_ANIMATION_DURATION_MS);
        mAnimationLeave.setInterpolator(Interpolators.DECELERATE_INTERPOLATOR);
        mAnimationLeave.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onStart(Animator animator) {
                setVisibility(View.VISIBLE);
                postInvalidateOnAnimation();
            }

            @Override
            public void onEnd(Animator animator) {
                setVisibility(View.GONE);
                mCurrentAnimation = null;
            }
        });
    }

    @Override
    protected void handleActivate() {
        if (mCurrentAnimation == mAnimationEnter) return;
        assert isWebContentAvailable();
        setShowState(true);
    }

    @Override
    protected void handleDeactivation(boolean clearSelection) {
        if (mCurrentAnimation != mAnimationLeave) setShowState(false);
        super.handleDeactivation(clearSelection);
    }

    @Override
    public boolean isAnimating() {
        return mCurrentAnimation != null;
    }

    @Override
    public void findResultSelected(Rect rect) {
        super.findResultSelected(rect);

        boolean makeRoom = false;
        float density = getContext().getResources().getDisplayMetrics().density;

        if (rect != null
                && rect.intersects((int) (getLeft() / density), 0, (int) (getRight() / density),
                        (int) (getHeight() / density))) {
            makeRoom = true;
        }

        setMakeRoomForResults(makeRoom);
    }

    @Override
    protected void clearResults() {
        super.clearResults();
        setMakeRoomForResults(false);
    }

    private void setMakeRoomForResults(boolean makeRoom) {
        float translationY = makeRoom ? -(getHeight() - mYInsetPx) : 0.f;

        if (translationY == getTranslationY()) return;

        if (mCurrentAnimation != null) {
            if (mCurrentAnimation == mAnimationEnter || mCurrentAnimation == mAnimationLeave) {
                mCurrentAnimation.end();
            } else {
                mCurrentAnimation.cancel();
            }
        }

        mCurrentAnimation = ObjectAnimator.ofFloat(this, View.TRANSLATION_Y, translationY);
        mCurrentAnimation.setDuration(MAKE_ROOM_ANIMATION_DURATION_MS);
        mCurrentAnimation.setInterpolator(Interpolators.DECELERATE_INTERPOLATOR);
        mCurrentAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                postInvalidateOnAnimation();
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mCurrentAnimation = null;
            }
        });
        startAnimationOverContent(mCurrentAnimation);
    }

    private void setShowState(boolean show) {
        ObjectAnimator nextAnimator = null;

        if (show && getVisibility() != View.VISIBLE && mCurrentAnimation != mAnimationEnter) {
            View anchorView = getRootView().findViewById(R.id.toolbar);
            FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) getLayoutParams();
            lp.topMargin = anchorView.getBottom() - mYInsetPx;
            setLayoutParams(lp);
            nextAnimator = mAnimationEnter;
        } else if (!show && getVisibility() != View.GONE && mCurrentAnimation != mAnimationLeave) {
            nextAnimator = mAnimationLeave;
            onHideAnimationStart();
        }

        if (nextAnimator != null) {
            if (mCurrentAnimation != null) mCurrentAnimation.cancel();

            mCurrentAnimation = nextAnimator;
            startAnimationOverContent(nextAnimator);
            postInvalidateOnAnimation();
        }
    }
}
