// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_SLIDE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.animation.AnimationHandler;

import java.util.Objects;

/** Holds the current pane's {@link View}. */
@NullMarked
public class HubPaneHostView extends FrameLayout {
    /** A listener for swipe gestures on the pane host view. */
    public interface OnPaneSwipeListener {
        /**
         * Called when a swipe gesture is completed.
         *
         * @param isSwipeLeft Whether the swipe was to the left.
         */
        void onPaneSwipe(boolean isSwipeLeft);
    }

    private FrameLayout mPaneFrame;
    private ViewGroup mSnackbarContainer;
    private @Nullable View mCurrentViewRoot;
    private final AnimationHandler mSlideAnimatorHandler;
    private @Nullable NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private @Nullable OnPaneSwipeListener mOnPaneSwipeListener;

    // Pane swipe-to-switch specifics.
    private final int mSwipeEdgeGutterWidth;
    private final int mSwipeTouchSlop;
    private final int mMinSwipeFlingVelocity;

    private boolean mIsSwipeBeingDragged;
    private float mSwipeInitialDownX;
    private float mSwipeInitialDownY;
    private @Nullable VelocityTracker mVelocityTracker;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        mSlideAnimatorHandler = new AnimationHandler();

        ViewConfiguration vc = ViewConfiguration.get(context);
        mSwipeEdgeGutterWidth =
                context.getResources().getDimensionPixelSize(R.dimen.hub_edge_swipe_gutter_width);
        mSwipeTouchSlop = vc.getScaledTouchSlop();
        mMinSwipeFlingVelocity = vc.getScaledMinimumFlingVelocity();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPaneFrame = findViewById(R.id.pane_frame);
        mSnackbarContainer = findViewById(R.id.pane_host_view_snackbar_container);
    }

    public void setOnPaneSwipeListener(OnPaneSwipeListener listener) {
        mOnPaneSwipeListener = listener;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent motionEvent) {
        if (!ChromeFeatureList.sEnableSwipeToSwitchPane.isEnabled()) return false;

        final int action = motionEvent.getActionMasked();

        // Reset drag state on CANCEL or UP.
        if (action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_UP) {
            mIsSwipeBeingDragged = false;
            return false;
        }

        // If we're already intercepting, continue to do so.
        if (action != MotionEvent.ACTION_DOWN && mIsSwipeBeingDragged) {
            return true;
        }

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                // Record the start of the gesture.
                mSwipeInitialDownX = motionEvent.getX();
                mSwipeInitialDownY = motionEvent.getY();
                mIsSwipeBeingDragged = false;
                break;

            case MotionEvent.ACTION_MOVE:
                // Only consider swipes that start at the edge.
                if (mSwipeInitialDownX > mSwipeEdgeGutterWidth
                        && mSwipeInitialDownX < getWidth() - mSwipeEdgeGutterWidth) {
                    return false;
                }

                final float x = motionEvent.getX();
                final float y = motionEvent.getY();
                final float dx = x - mSwipeInitialDownX;
                final float dy = y - mSwipeInitialDownY;

                // Check for a clear horizontal swipe past the touch slop.
                if (Math.abs(dx) > mSwipeTouchSlop && Math.abs(dx) > Math.abs(dy)) {
                    mIsSwipeBeingDragged = true;
                    // Prevent the parent from stealing our gesture.
                    getParent().requestDisallowInterceptTouchEvent(true);
                }
                break;
        }

        return mIsSwipeBeingDragged;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        }
        mVelocityTracker.addMovement(event);

        if (event.getActionMasked() == MotionEvent.ACTION_UP) {
            // This is the up, we are NOT handling this as a click.
            performClick();

            // Calculate the velocity of the swipe.
            mVelocityTracker.computeCurrentVelocity(1000);
            float velocityX = mVelocityTracker.getXVelocity();
            float velocityY = mVelocityTracker.getYVelocity();

            // Check if the swipe was a horizontal fling.
            if (Math.abs(velocityX) > mMinSwipeFlingVelocity
                    && Math.abs(velocityX) > Math.abs(velocityY)) {
                if (mOnPaneSwipeListener != null) {
                    mOnPaneSwipeListener.onPaneSwipe(velocityX < 0);
                }
            }

            mIsSwipeBeingDragged = false;
            if (mVelocityTracker != null) {
                mVelocityTracker.recycle();
                mVelocityTracker = null;
            }
        }
        return true;
    }

    @Override
    public boolean performClick() {
        // This is a no-op, but we need to override it for accessibility.
        super.performClick();
        return true;
    }

    /**
     * Sets the root view for the pane host, animating the transition if both old and new views are
     * non-null.
     *
     * @param newRootView The new root view to display.
     * @param isSlideAnimationLeftToRight Whether the animation should slide from left-to-right
     *     (true) or right-to-left (false), only when slide animation is enabled.
     */
    void setRootView(@Nullable View newRootView, boolean isSlideAnimationLeftToRight) {

        final View oldRootView = mCurrentViewRoot;
        mCurrentViewRoot = newRootView;

        if (oldRootView != null && newRootView != null) {
            // If width is not available, just swap views without animation.
            if (mPaneFrame.getWidth() == 0) {
                mPaneFrame.removeAllViews();
                tryAddViewToFrame(newRootView);
            } else {
                animateSlideTransition(oldRootView, newRootView, isSlideAnimationLeftToRight);
            }
        } else if (newRootView == null) {
            mPaneFrame.removeAllViews();
        } else { // oldRootView == null
            tryAddViewToFrame(newRootView);
        }
    }

    private void animateSlideTransition(View oldRootView, View newRootView, boolean isLeftToRight) {
        mSlideAnimatorHandler.forceFinishAnimation();
        int containerWidth = mPaneFrame.getWidth();

        // Determine start and end positions based on direction.
        float oldViewEndTranslation = isLeftToRight ? containerWidth : -containerWidth;
        float newViewStartTranslation = isLeftToRight ? -containerWidth : containerWidth;

        // Ensure old view is at its starting position.
        oldRootView.setTranslationX(0);
        // Position new view off-screen.
        newRootView.setTranslationX(newViewStartTranslation);

        // Ensure new view is added before animation starts.
        tryAddViewToFrame(newRootView);

        Animator slideOut =
                ObjectAnimator.ofFloat(oldRootView, View.TRANSLATION_X, 0, oldViewEndTranslation);
        slideOut.setDuration(PANE_SLIDE_ANIMATION_DURATION_MS);

        Animator slideIn =
                ObjectAnimator.ofFloat(newRootView, View.TRANSLATION_X, newViewStartTranslation, 0);
        slideIn.setDuration(PANE_SLIDE_ANIMATION_DURATION_MS);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(slideOut, slideIn);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mPaneFrame.removeView(oldRootView);
                        oldRootView.setTranslationX(0);
                        newRootView.setTranslationX(0);
                    }
                });
        mSlideAnimatorHandler.startAnimation(animatorSet);
    }

    void setColorMixer(HubColorMixer mixer) {
        registerColorBlends(mixer);
    }

    private void registerColorBlends(HubColorMixer mixer) {
        Context context = getContext();
        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> getBackgroundColor(context, colorScheme),
                        mPaneFrame::setBackgroundColor));
    }

    void setSnackbarContainerConsumer(Callback<ViewGroup> consumer) {
        consumer.onResult(mSnackbarContainer);
    }

    private void tryAddViewToFrame(View rootView) {
        ViewParent parent = rootView.getParent();
        if (!Objects.equals(parent, mPaneFrame)) {
            if (parent instanceof ViewGroup viewGroup) {
                viewGroup.removeView(rootView);
            }
            mPaneFrame.addView(rootView);
        }
    }

    private @ColorInt int getBackgroundColor(Context context, @HubColorScheme int colorScheme) {
        boolean isXrFullSpaceMode =
                mXrSpaceModeObservableSupplier != null && mXrSpaceModeObservableSupplier.get();
        return HubColors.getBackgroundColor(context, colorScheme, isXrFullSpaceMode);
    }

    public void setXrSpaceModeObservableSupplier(
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
    }

    int getSwipeEdgeGutterWidthForTesting() {
        return mSwipeEdgeGutterWidth;
    }

    void setVelocityTrackerForTesting(VelocityTracker tracker) {
        mVelocityTracker = tracker;
    }
}
