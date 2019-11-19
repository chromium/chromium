// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.view.HapticFeedbackConstants;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.AnimationSet;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.ScaleAnimation;
import android.view.animation.Transformation;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.gesturenav.NavigationBubble.CloseTarget;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;

/**
 * The SideSlideLayout can be used whenever the user navigates the contents
 * of a view using horizontal gesture. Shows an arrow widget moving horizontally
 * in reaction to the gesture which, if goes over a threshold, triggers navigation.
 * The caller that instantiates this view should add an {@link #OnNavigateListener}
 * to be notified whenever the gesture is completed.
 * Based on {@link org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout}
 * and modified accordingly to support horizontal gesture.
 */
public class SideSlideLayout extends ViewGroup {
    /**
     * Classes that wish to be notified when the swipe gesture correctly
     * triggers navigation should implement this interface.
     */
    public interface OnNavigateListener { void onNavigate(boolean isForward); }

    /**
     * Classes that wish to be notified when a reset is triggered should
     * implement this interface.
     */
    public interface OnResetListener { void onReset(); }

    // Swipe offset in dips from the border of the view before applying physical tension
    // effect. The actual arrow bubble position is capped at a value three times as this
    // one, which is where navigation gets triggered.
    private static final int RAW_SWIPE_LIMIT_DP = 32;

    // Multiplier to |RAW_SWIPE_LIMIT_DP| to trigger the navigation.
    private static final int THRESHOLD_MULTIPLIER = 3;

    private static final float DECELERATE_INTERPOLATION_FACTOR = 2f;

    private static final int SCALE_DOWN_DURATION_MS = 400;
    private static final int ANIMATE_TO_START_DURATION_MS = 500;

    // Minimum number of pull updates necessary to trigger a side nav.
    private static final int MIN_PULLS_TO_ACTIVATE = 3;

    // Time threshold to detect navigation reversal - i.e. user navigating
    // forward after navigating back (or back after forward) within a short
    // period of time.
    private static final int NAVIGATION_REVERSAL_MS = 3 * 1000;

    private final DecelerateInterpolator mDecelerateInterpolator;
    private final float mTotalDragDistance;
    private final int mMediumAnimationDuration;
    private final int mCircleWidth;

    // Metrics
    private static long sLastCompletedTime;
    private static boolean sLastCompletedForward;

    private OnNavigateListener mListener;
    private OnResetListener mResetListener;

    // Flag indicating that the navigation will be activated.
    private boolean mNavigating;

    private int mCurrentTargetOffset;
    private float mTotalMotion;

    // True while side gesture is in progress.
    private boolean mIsBeingDragged;

    private NavigationBubble mArrowView;
    private int mArrowViewWidth;

    // Start position for animation moving the UI back to original offset.
    private int mFrom;
    private int mOriginalOffset;

    private AnimationSet mHidingAnimation;
    private int mAnimationViewWidth;

    private boolean mIsForward;
    private @CloseTarget int mCloseIndicator;

    // True while swiped to a distance where, if released, the navigation would be triggered.
    private boolean mWillNavigate;

    // Used for metrics. Indicates user swiped over the threshold that turns the arrow blue.
    private boolean mSwipedOverThreshold;

    private final AnimationListener mNavigateListener = new AnimationListener() {
        @Override
        public void onAnimationStart(Animation animation) {}

        @Override
        public void onAnimationRepeat(Animation animation) {}

        @Override
        public void onAnimationEnd(Animation animation) {
            mArrowView.setFaded(false, false);
            mArrowView.setVisibility(View.INVISIBLE);
            if (mNavigating) {
                if (mListener != null) mListener.onNavigate(mIsForward);
            } else {
                reset();
            }
            hideCloseIndicator();
        }
    };

    private final Animation mAnimateToStartPosition = new Animation() {
        @Override
        public void applyTransformation(float interpolatedTime, Transformation t) {
            int targetTop = mFrom + (int) ((mOriginalOffset - mFrom) * interpolatedTime);
            int offset = targetTop - mArrowView.getLeft();
            mTotalMotion += offset;

            float progress = Math.min(1.f, getOverscroll() / mTotalDragDistance);
            setTargetOffsetLeftAndRight(offset);
        }
    };

    public SideSlideLayout(Context context) {
        super(context);

        mMediumAnimationDuration =
                getResources().getInteger(android.R.integer.config_mediumAnimTime);

        setWillNotDraw(false);
        mDecelerateInterpolator = new DecelerateInterpolator(DECELERATE_INTERPOLATION_FACTOR);

        mCircleWidth = (int) getResources().getDimensionPixelSize(R.dimen.navigation_bubble_size);

        LayoutInflater layoutInflater = LayoutInflater.from(getContext());
        mArrowView = (NavigationBubble) layoutInflater.inflate(R.layout.navigation_bubble, null);
        mArrowView.getTextView().setText(
                getResources().getString(R.string.overscroll_navigation_close_chrome,
                        getContext().getString(R.string.app_name)));
        mArrowViewWidth = mCircleWidth;
        addView(mArrowView);

        // The absolute offset has to take into account that the circle starts at an offset
        mTotalDragDistance = RAW_SWIPE_LIMIT_DP * getResources().getDisplayMetrics().density;
    }

    /**
     * Set the listener to be notified when the navigation is triggered.
     */
    public void setOnNavigationListener(OnNavigateListener listener) {
        mListener = listener;
    }

    /**
     * Set the reset listener to be notified when a reset is triggered.
     */
    public void setOnResetListener(OnResetListener listener) {
        mResetListener = listener;
    }

    /**
     * Stop navigation.
     */
    public void stopNavigating() {
        setNavigating(false);
    }

    private void setNavigating(boolean navigating) {
        if (mNavigating != navigating) {
            mNavigating = navigating;
            if (mNavigating) startHidingAnimation(mNavigateListener);
        }
    }

    /**
     * @return Absolute swipe distance from the starting edge.
     */
    float getOverscroll() {
        return mIsForward ? -Math.min(0, mTotalMotion) : Math.max(0, mTotalMotion);
    }

    private void startHidingAnimation(AnimationListener listener) {
        // ScaleAnimation needs to be created again if the arrow widget width changes over time
        // (due to turning on/off close indicator) to set the right x pivot point.
        if (mHidingAnimation == null || mAnimationViewWidth != mArrowViewWidth) {
            mAnimationViewWidth = mArrowViewWidth;
            ScaleAnimation scalingDown =
                    new ScaleAnimation(1, 0, 1, 0, mArrowViewWidth / 2, mArrowView.getHeight() / 2);
            scalingDown.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
            scalingDown.setDuration(SCALE_DOWN_DURATION_MS);
            Animation fadingOut = new AlphaAnimation(1, 0);
            fadingOut.setInterpolator(mDecelerateInterpolator);
            fadingOut.setDuration(SCALE_DOWN_DURATION_MS);
            mHidingAnimation = new AnimationSet(false);
            mHidingAnimation.addAnimation(fadingOut);
            mHidingAnimation.addAnimation(scalingDown);
        }
        mArrowView.setAnimationListener(listener);
        mArrowView.clearAnimation();
        mArrowView.startAnimation(mHidingAnimation);
    }

    /**
     * Set the direction used for sliding gesture.
     * @param forward {@code true} if direction is forward.
     */
    public void setDirection(boolean forward) {
        mIsForward = forward;
        mArrowView.setIcon(
                forward ? R.drawable.ic_arrow_forward_blue_24dp : R.drawable.ic_arrow_back_24dp);
    }

    public void setCloseIndicator(@CloseTarget int target) {
        mCloseIndicator = target;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        if (getChildCount() == 0) return;

        final int height = getMeasuredHeight();
        final int arrowWidth = mArrowView.getMeasuredWidth();
        final int arrowHeight = mArrowView.getMeasuredHeight();
        mArrowView.layout(mCurrentTargetOffset, height / 2 - arrowHeight / 2,
                mCurrentTargetOffset + arrowWidth, height / 2 + arrowHeight / 2);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mArrowView.measure(MeasureSpec.makeMeasureSpec(mArrowViewWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(mCircleWidth, MeasureSpec.EXACTLY));
    }

    private void initializeOffset() {
        int offset = mIsForward ? ((View) getParent()).getWidth() : -mArrowViewWidth;
        mCurrentTargetOffset = mOriginalOffset = offset;
    }

    /**
     * Start the pull effect. If the effect is disabled or a navigation animation
     * is currently active, the request will be ignored.
     * @return whether a new pull sequence has started.
     */
    public boolean start() {
        if (!isEnabled() || mNavigating || mListener == null) return false;
        mTotalMotion = 0;
        mIsBeingDragged = true;
        mWillNavigate = false;
        initializeOffset();
        mArrowView.setFaded(false, false);
        return true;
    }

    /**
     * Apply a pull impulse to the effect. If the effect is disabled or has yet
     * to start, the pull will be ignored.
     * @param delta the magnitude of the pull.
     */
    public void pull(float delta) {
        if (!isEnabled() || !mIsBeingDragged) return;

        float maxDelta = mTotalDragDistance / MIN_PULLS_TO_ACTIVATE;
        delta = Math.max(-maxDelta, Math.min(maxDelta, delta));
        mTotalMotion += delta;

        float overscroll = getOverscroll();
        float extraOs = overscroll - mTotalDragDistance;
        float slingshotDist = mTotalDragDistance;
        float tensionSlingshotPercent =
                Math.max(0, Math.min(extraOs, slingshotDist * 2) / slingshotDist);
        float tensionPercent =
                (float) ((tensionSlingshotPercent / 4) - Math.pow((tensionSlingshotPercent / 4), 2))
                * 2f;

        if (mArrowView.getVisibility() != View.VISIBLE) mArrowView.setVisibility(View.VISIBLE);

        float originalDragPercent = overscroll / mTotalDragDistance;
        float dragPercent = Math.min(1f, Math.abs(originalDragPercent));

        // Tint the arrow blue when swiped enough to initiate navigation if released.
        boolean navigating = willNavigate();
        if (navigating != mWillNavigate) {
            mArrowView.setImageTint(navigating);
            if (navigating) {
                performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP);
                mSwipedOverThreshold = true;
            }
        }
        mWillNavigate = navigating;

        if (mCloseIndicator != CloseTarget.NONE) {
            if (mWillNavigate) {
                mArrowView.showCaption(mCloseIndicator);
                mArrowViewWidth = mArrowView.getMeasuredWidth();
            } else {
                hideCloseIndicator();
            }
        }

        float extraMove = slingshotDist * tensionPercent * 2;
        int targetDiff = (int) (slingshotDist * dragPercent + extraMove);
        int targetX = mOriginalOffset + (mIsForward ? -targetDiff : targetDiff);
        setTargetOffsetLeftAndRight(targetX - mCurrentTargetOffset);
    }

    /**
     * Update arrow bubble transparency as navigation sheet state changes.
     * @param faded {@code true} if arrow bubble should fade out.
     * @param animate {@code true} if animation is needed.
     */
    void fadeArrow(boolean faded, boolean animate) {
        mArrowView.setFaded(faded, animate);
    }

    /**
     * Hide arrow bubble by making it fade away at the current position.
     */
    void hideArrow() {
        mNavigating = false;
        startHidingAnimation(mNavigateListener);
    }

    /**
     * @return {@code true} if swiped long enough to trigger navigation upon release.
     */
    boolean willNavigate() {
        return getOverscroll() > mTotalDragDistance * THRESHOLD_MULTIPLIER;
    }

    private void hideCloseIndicator() {
        mArrowView.showCaption(CloseTarget.NONE);
        // The width when indicator text view is hidden is slightly bigger than the height.
        // Set the width to circle's diameter for the widget to be of completely round shape.
        mArrowViewWidth = mCircleWidth;
    }

    private void setTargetOffsetLeftAndRight(int offset) {
        mArrowView.offsetLeftAndRight(offset);
        mCurrentTargetOffset = mArrowView.getLeft();
    }

    /**
     * Release the active pull. If no pull has started, the release will be ignored.
     * If the pull was sufficiently large, the navigation sequence will be initiated.
     * @param allowNav whether to allow a sufficiently large pull to trigger
     *                     the navigation action and animation sequence.
     */
    public void release(boolean allowNav) {
        if (!mIsBeingDragged) return;

        // See ACTION_UP handling in {@link #onTouchEvent(...)}.
        mIsBeingDragged = false;

        GestureNavMetrics.recordHistogram("GestureNavigation.Triggered", mIsForward);
        if (mSwipedOverThreshold) {
            GestureNavMetrics.recordHistogram("GestureNavigation.SwipedOverThreshold", mIsForward);
            mSwipedOverThreshold = false;
        }

        if (isEnabled() && willNavigate()) {
            if (allowNav) {
                setNavigating(true);
                GestureNavMetrics.recordHistogram("GestureNavigation.Completed", mIsForward);
                long time = System.currentTimeMillis();
                if (sLastCompletedTime > 0 && time - sLastCompletedTime < NAVIGATION_REVERSAL_MS
                        && mIsForward != sLastCompletedForward) {
                    GestureNavMetrics.recordHistogram("GestureNavigation.Reversed", mIsForward);
                }
                sLastCompletedTime = time;
                sLastCompletedForward = mIsForward;
            } else {
                // Show navigation instead of triggering navigation. Just hide the arrow
                // by fading it away.
                mNavigating = false;
                startHidingAnimation(mNavigateListener);
            }
            return;
        }
        // Cancel navigation
        mNavigating = false;
        mFrom = mCurrentTargetOffset;
        mAnimateToStartPosition.reset();
        mAnimateToStartPosition.setDuration(ANIMATE_TO_START_DURATION_MS);
        mAnimateToStartPosition.setInterpolator(mDecelerateInterpolator);
        mArrowView.clearAnimation();
        mArrowView.startAnimation(mAnimateToStartPosition);
        GestureNavMetrics.recordHistogram("GestureNavigation.Abandoned", mIsForward);
    }

    /**
     * Reset the effect, clearing any active animations.
     */
    public void reset() {
        mIsBeingDragged = false;
        setNavigating(false);
        hideCloseIndicator();

        // Return the circle to its start position
        setTargetOffsetLeftAndRight(mOriginalOffset - mCurrentTargetOffset);
        mCurrentTargetOffset = mArrowView.getLeft();
        if (mResetListener != null) mResetListener.onReset();
    }
}
