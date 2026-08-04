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
import android.animation.ValueAnimator;
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
    /** A provider of adjacent pane views during a swipe drag gesture. */
    public interface PaneViewProvider {
        /**
         * Prepares the adjacent pane for display (warming resources) and returns its root view, or
         * null if there is no adjacent pane to switch to.
         */
        @Nullable View prepareAndGetAdjacentPaneView(boolean isSwipeLeft);

        /**
         * Notifies that the swipe drag has completed successfully, indicating the Hub should switch
         * focus to the adjacent pane.
         */
        void onSwipeSwitchComplete(boolean isSwipeLeft);

        /** Notifies that the swipe drag was cancelled or settled back to the original pane. */
        default void onSwipeSwitchCancel(boolean isSwipeLeft) {}

        /** Notifies of the active drag displacement progress (fraction from 0.0 to 1.0). */
        default void onSwipeDragProgress(float progress, boolean isSwipeLeft) {}
    }

    private final HubColorMixerRegistrationHelper mColorMixerHelper =
            new HubColorMixerRegistrationHelper();
    private FrameLayout mPaneFrame;
    private ViewGroup mSnackbarContainer;
    private @Nullable View mCurrentViewRoot;
    private final AnimationHandler mSlideAnimatorHandler;
    private @Nullable NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;
    private @Nullable PaneViewProvider mPaneViewProvider;
    private @Nullable InteractiveElementChecker mInteractiveElementChecker;

    private @Nullable View mAdjacentViewRoot;
    private boolean mSwipeDirectionIsLeft;
    private boolean mIsInteractiveSwitchInProgress;

    /** A checker to see if a touch is on an interactive element. */
    public interface InteractiveElementChecker {
        /**
         * Returns whether the touch at (x, y) (relative to the pane's root view) is interactive.
         */
        boolean isTouchOnInteractiveElement(float x, float y);
    }

    // Pane swipe-to-switch specifics.
    private final int mSwipeEdgeGutterWidth;
    private final int mSwipeTouchSlop;
    private final int mMinSwipeFlingVelocity;

    private boolean mIsSwipeBeingDragged;
    private boolean mCanInterceptSwipe;
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

        Context context = getContext();
        mColorMixerHelper.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> getBackgroundColor(context, colorScheme),
                        mPaneFrame::setBackgroundColor));
    }

    public void setPaneViewProvider(@Nullable PaneViewProvider provider) {
        mPaneViewProvider = provider;
    }

    public void setInteractiveElementChecker(@Nullable InteractiveElementChecker checker) {
        mInteractiveElementChecker = checker;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent motionEvent) {
        if (!ChromeFeatureList.sEnableSwipeToSwitchPane.isEnabled()) return false;

        final int action = motionEvent.getActionMasked();

        // Reset drag state on CANCEL or UP.
        if (action == MotionEvent.ACTION_CANCEL || action == MotionEvent.ACTION_UP) {
            cleanupDrag();
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

                // 1. Exclude device edges (reserved for OS System Back).
                boolean isEdgeTouch =
                        mSwipeInitialDownX <= mSwipeEdgeGutterWidth
                                || mSwipeInitialDownX >= getWidth() - mSwipeEdgeGutterWidth;

                // 2. Exclude interactive child elements (reserved for tab swipe-to-close).
                boolean isInteractiveTouch = false;
                if (mInteractiveElementChecker != null && mCurrentViewRoot != null) {
                    int[] paneLocation = new int[2];
                    mCurrentViewRoot.getLocationOnScreen(paneLocation);

                    int[] hostLocation = new int[2];
                    getLocationOnScreen(hostLocation);

                    float rawX = mSwipeInitialDownX + hostLocation[0];
                    float rawY = mSwipeInitialDownY + hostLocation[1];

                    float paneX = rawX - paneLocation[0];
                    float paneY = rawY - paneLocation[1];

                    isInteractiveTouch =
                            mInteractiveElementChecker.isTouchOnInteractiveElement(paneX, paneY);
                }

                mCanInterceptSwipe = !isEdgeTouch && !isInteractiveTouch;
                break;

            case MotionEvent.ACTION_MOVE:
                if (!mCanInterceptSwipe) {
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
        if (!mCanInterceptSwipe) {
            return false;
        }

        if (mVelocityTracker == null) {
            mVelocityTracker = VelocityTracker.obtain();
        }
        mVelocityTracker.addMovement(event);

        final int action = event.getActionMasked();
        final float x = event.getX();

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                mSwipeInitialDownX = event.getX();
                mSwipeInitialDownY = event.getY();
                break;

            case MotionEvent.ACTION_MOVE:
                if (mIsSwipeBeingDragged && mCurrentViewRoot != null) {
                    float dx = x - mSwipeInitialDownX;
                    if (mAdjacentViewRoot == null && mPaneViewProvider != null) {
                        boolean isSwipeLeft = dx < 0;
                        mAdjacentViewRoot =
                                mPaneViewProvider.prepareAndGetAdjacentPaneView(isSwipeLeft);
                        if (mAdjacentViewRoot != null) {
                            mSwipeDirectionIsLeft = isSwipeLeft;
                            mAdjacentViewRoot.setTranslationX(
                                    isSwipeLeft ? getWidth() : -getWidth());
                            tryAddViewToFrame(mAdjacentViewRoot);
                        }
                    }

                    if (mAdjacentViewRoot != null) {
                        if (mSwipeDirectionIsLeft) {
                            dx = Math.min(0, Math.max(-getWidth(), dx));
                        } else {
                            dx = Math.max(0, Math.min(getWidth(), dx));
                        }
                        mCurrentViewRoot.setTranslationX(dx);
                        mAdjacentViewRoot.setTranslationX(
                                dx + (mSwipeDirectionIsLeft ? getWidth() : -getWidth()));

                        int width = getWidth();
                        if (width > 0) {
                            float progress = Math.abs(dx) / width;
                            if (mPaneViewProvider != null) {
                                mPaneViewProvider.onSwipeDragProgress(
                                        progress, mSwipeDirectionIsLeft);
                            }
                        }
                    }
                }
                break;

            case MotionEvent.ACTION_UP:
                performClick();
            // Fallthrough to settle the drag
            case MotionEvent.ACTION_CANCEL:
                if (mIsSwipeBeingDragged && mCurrentViewRoot != null && mAdjacentViewRoot != null) {
                    mVelocityTracker.computeCurrentVelocity(1000);
                    float velocityX = mVelocityTracker.getXVelocity();
                    float velocityY = mVelocityTracker.getYVelocity();
                    float dx = x - mSwipeInitialDownX;

                    boolean isFling =
                            Math.abs(velocityX) > mMinSwipeFlingVelocity
                                    && Math.abs(velocityX) > Math.abs(velocityY);
                    boolean isFlingInCorrectDirection =
                            isFling
                                    && ((mSwipeDirectionIsLeft && velocityX < 0)
                                            || (!mSwipeDirectionIsLeft && velocityX > 0));

                    boolean isDisplacementEnough = Math.abs(dx) > getWidth() / 2f;

                    boolean shouldSwitch = isDisplacementEnough || isFlingInCorrectDirection;
                    if (shouldSwitch) {
                        mIsInteractiveSwitchInProgress = true;
                    }
                    animateSettle(shouldSwitch);
                } else {
                    cleanupDrag();
                }
                break;
        }

        return true;
    }

    private void animateSettle(boolean isSwitch) {
        final View currentView = mCurrentViewRoot;
        final View adjacentView = mAdjacentViewRoot;
        if (currentView == null || adjacentView == null) {
            cleanupDrag();
            return;
        }

        mSlideAnimatorHandler.forceFinishAnimation();
        int width = getWidth();
        if (width <= 0) {
            cleanupDrag();
            return;
        }

        float currentTargetX = isSwitch ? (mSwipeDirectionIsLeft ? -width : width) : 0f;
        float adjacentTargetX = isSwitch ? 0f : (mSwipeDirectionIsLeft ? width : -width);

        Animator currentAnim =
                ObjectAnimator.ofFloat(currentView, View.TRANSLATION_X, currentTargetX);
        Animator adjacentAnim =
                ObjectAnimator.ofFloat(adjacentView, View.TRANSLATION_X, adjacentTargetX);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(currentAnim, adjacentAnim);
        animatorSet.setDuration(PANE_SLIDE_ANIMATION_DURATION_MS);

        ValueAnimator.AnimatorUpdateListener updateListener =
                animation -> {
                    float dx = currentView.getTranslationX();
                    float progress = Math.abs(dx) / width;
                    if (mPaneViewProvider != null) {
                        mPaneViewProvider.onSwipeDragProgress(progress, mSwipeDirectionIsLeft);
                    }
                };
        if (currentAnim instanceof ValueAnimator) {
            ((ValueAnimator) currentAnim).addUpdateListener(updateListener);
        }

        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (isSwitch) {
                            if (mPaneViewProvider != null) {
                                mPaneViewProvider.onSwipeDragProgress(
                                        /* progress= */ 1.0f, mSwipeDirectionIsLeft);
                                mPaneViewProvider.onSwipeSwitchComplete(mSwipeDirectionIsLeft);
                            }
                        } else {
                            if (mPaneViewProvider != null) {
                                mPaneViewProvider.onSwipeDragProgress(
                                        /* progress= */ 0.0f, mSwipeDirectionIsLeft);
                                mPaneViewProvider.onSwipeSwitchCancel(mSwipeDirectionIsLeft);
                            }
                            mPaneFrame.removeView(adjacentView);
                            adjacentView.setTranslationX(0);
                            currentView.setTranslationX(0);
                        }
                        cleanupDrag();
                    }
                });
        mSlideAnimatorHandler.startAnimation(animatorSet);
    }

    private void cleanupDrag() {
        mIsSwipeBeingDragged = false;
        mCanInterceptSwipe = false;
        mAdjacentViewRoot = null;
        if (mVelocityTracker != null) {
            mVelocityTracker.recycle();
            mVelocityTracker = null;
        }
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
        if (mIsInteractiveSwitchInProgress) {
            mCurrentViewRoot = newRootView;
            mPaneFrame.removeAllViews();
            if (newRootView != null) {
                tryAddViewToFrame(newRootView);
                newRootView.setTranslationX(0);
            }
            mIsInteractiveSwitchInProgress = false;
            return;
        }

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
        mColorMixerHelper.setColorMixer(mixer);
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

    public void destroy() {
        mSlideAnimatorHandler.forceFinishAnimation();
        mColorMixerHelper.destroy();
        mPaneFrame.removeAllViews();
        mCurrentViewRoot = null;
    }
}
