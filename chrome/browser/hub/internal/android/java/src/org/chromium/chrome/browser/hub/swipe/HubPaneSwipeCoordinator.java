// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;

import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubPaneHostView.InteractiveElementChecker;
import org.chromium.chrome.browser.hub.HubPaneHostView.PaneViewProvider;
import org.chromium.chrome.browser.hub.swipe.HubPaneSwipeGestureHandler.SwipeGestureDelegate;

import java.util.Objects;

/**
 * Coordinates swipe-to-switch gestures, settle animations, and pane transitions in the Hub pane
 * host.
 */
@NullMarked
public class HubPaneSwipeCoordinator implements SwipeGestureDelegate {
    private final View mHostView;
    private final FrameLayout mPaneFrame;
    private final HubPaneSwipeGestureHandler mGestureHandler;
    private final HubPaneSwipeAnimator mAnimator;

    private @Nullable PaneViewProvider mPaneViewProvider;
    private @Nullable InteractiveElementChecker mInteractiveElementChecker;
    private @Nullable View mCurrentViewRoot;
    private @Nullable View mAdjacentViewRoot;
    private boolean mIsInteractiveSwitchInProgress;
    private boolean mIsSwipeLeft;

    /**
     * Creates a new coordinator instance.
     *
     * @param hostView The host container view.
     * @param paneFrame The inner layout frame holding pane views.
     */
    public HubPaneSwipeCoordinator(View hostView, FrameLayout paneFrame) {
        mHostView = hostView;
        mPaneFrame = paneFrame;
        mGestureHandler = new HubPaneSwipeGestureHandler(hostView.getContext(), this);
        mAnimator = new HubPaneSwipeAnimator();
    }

    /** Sets the provider of adjacent pane views. */
    public void setPaneViewProvider(@Nullable PaneViewProvider provider) {
        mPaneViewProvider = provider;
    }

    /** Sets the checker for interactive child elements. */
    public void setInteractiveElementChecker(@Nullable InteractiveElementChecker checker) {
        mInteractiveElementChecker = checker;
    }

    /**
     * Intercepts touch events for the host view.
     *
     * @param event The motion event.
     * @return Whether the gesture is intercepted.
     */
    public boolean onInterceptTouchEvent(MotionEvent event) {
        return mGestureHandler.onInterceptTouchEvent(event, mHostView.getParent());
    }

    /**
     * Handles touch events for the host view.
     *
     * @param event The motion event.
     * @return Whether the event was handled.
     */
    public boolean onTouchEvent(MotionEvent event) {
        return mGestureHandler.onTouchEvent(event);
    }

    /**
     * Updates the root view displayed in the pane frame.
     *
     * @param newRootView The new root view, or null to clear.
     * @param isSlideAnimationLeftToRight Whether programmatic transition slides left-to-right.
     */
    public void setRootView(@Nullable View newRootView, boolean isSlideAnimationLeftToRight) {
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
            if (mPaneFrame.getWidth() == 0) {
                mPaneFrame.removeAllViews();
                tryAddViewToFrame(newRootView);
            } else {
                mAnimator.animateSlideTransition(
                        mPaneFrame,
                        oldRootView,
                        newRootView,
                        isSlideAnimationLeftToRight,
                        CallbackUtils.emptyRunnable());
            }
        } else if (newRootView == null) {
            mPaneFrame.removeAllViews();
        } else {
            tryAddViewToFrame(newRootView);
        }
    }

    @Override
    public int getContainerWidth() {
        return mPaneFrame.getWidth() > 0 ? mPaneFrame.getWidth() : mHostView.getWidth();
    }

    @Override
    public boolean isTouchOnInteractiveElement(float hostX, float hostY) {
        if (mInteractiveElementChecker == null || mCurrentViewRoot == null) {
            return false;
        }

        int[] paneLocation = new int[2];
        mCurrentViewRoot.getLocationOnScreen(paneLocation);

        int[] hostLocation = new int[2];
        mHostView.getLocationOnScreen(hostLocation);

        float rawX = hostX + hostLocation[0];
        float rawY = hostY + hostLocation[1];

        float paneX = rawX - paneLocation[0];
        float paneY = rawY - paneLocation[1];

        return mInteractiveElementChecker.isTouchOnInteractiveElement(paneX, paneY);
    }

    @Override
    public @Nullable View onSwipeStarted(boolean isSwipeLeft) {
        if (mPaneViewProvider == null) return null;
        mIsSwipeLeft = isSwipeLeft;
        mAdjacentViewRoot = mPaneViewProvider.prepareAndGetAdjacentPaneView(isSwipeLeft);
        if (mAdjacentViewRoot != null) {
            int width = getContainerWidth();
            mAdjacentViewRoot.setTranslationX(isSwipeLeft ? width : -width);
            tryAddViewToFrame(mAdjacentViewRoot);
        }
        return mAdjacentViewRoot;
    }

    @Override
    public void onSwipeProgress(float dx, float progress, boolean isSwipeLeft) {
        if (mCurrentViewRoot != null && mAdjacentViewRoot != null) {
            int width = getContainerWidth();
            mCurrentViewRoot.setTranslationX(dx);
            mAdjacentViewRoot.setTranslationX(dx + (isSwipeLeft ? width : -width));
            if (mPaneViewProvider != null) {
                mPaneViewProvider.onSwipeDragProgress(progress, isSwipeLeft);
            }
        }
    }

    @Override
    public void onSwipeSettled(boolean shouldSwitch, boolean isSwipeLeft) {
        mIsSwipeLeft = isSwipeLeft;
        final View currentView = mCurrentViewRoot;
        final View adjacentView = mAdjacentViewRoot;
        if (currentView == null || adjacentView == null) {
            onSwipeCancelled();
            return;
        }

        if (shouldSwitch) {
            mIsInteractiveSwitchInProgress = true;
        }

        mAnimator.animateSettle(
                currentView,
                adjacentView,
                shouldSwitch,
                isSwipeLeft,
                getContainerWidth(),
                (progress, left) -> {
                    if (mPaneViewProvider != null) {
                        mPaneViewProvider.onSwipeDragProgress(progress, left);
                    }
                },
                () -> {
                    if (shouldSwitch) {
                        // Settle completed: notify provider to switch to the adjacent pane.
                        if (mPaneViewProvider != null) {
                            mPaneViewProvider.onSwipeDragProgress(
                                    /* progress= */ 1.0f, isSwipeLeft);
                            mPaneViewProvider.onSwipeSwitchComplete(isSwipeLeft);
                        }
                    } else {
                        // Settle cancelled: restore previous pane and clean up adjacent view.
                        resetSwipeViewsAndNotifyCancel(isSwipeLeft);
                    }
                    mAdjacentViewRoot = null;
                });
    }

    @Override
    public void onSwipeCancelled() {
        resetSwipeViewsAndNotifyCancel(mIsSwipeLeft);
    }

    /**
     * Resets swipe translations, removes the adjacent view, and notifies the provider of
     * cancellation.
     */
    private void resetSwipeViewsAndNotifyCancel(boolean isSwipeLeft) {
        if (mAdjacentViewRoot != null) {
            mPaneFrame.removeView(mAdjacentViewRoot);
            mAdjacentViewRoot.setTranslationX(0);
            mAdjacentViewRoot = null;
        }
        if (mCurrentViewRoot != null) {
            mCurrentViewRoot.setTranslationX(0);
        }
        if (mPaneViewProvider != null) {
            mPaneViewProvider.onSwipeDragProgress(/* progress= */ 0.0f, isSwipeLeft);
            mPaneViewProvider.onSwipeSwitchCancel(isSwipeLeft);
        }
        mIsInteractiveSwitchInProgress = false;
        mIsSwipeLeft = false;
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

    /** Destroys the coordinator and cleans up running animations. */
    public void destroy() {
        mAnimator.forceFinishAnimation();
        mGestureHandler.cleanupDrag();
        mPaneFrame.removeAllViews();
        mCurrentViewRoot = null;
        mAdjacentViewRoot = null;
        mIsInteractiveSwitchInProgress = false;
        mIsSwipeLeft = false;
    }
}
