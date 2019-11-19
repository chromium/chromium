// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.gesturenav.NavigationBubble.CloseTarget;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles history overscroll navigation controlling the underlying UI widget.
 */
public class NavigationHandler {
    // Width of a rectangluar area in dp on the left/right edge used for navigation.
    // Swipe beginning from a point within these rects triggers the operation.
    @VisibleForTesting
    static final float EDGE_WIDTH_DP = 48;

    // Weighted value to determine when to trigger an edge swipe. Initial scroll
    // vector should form 30 deg or below to initiate swipe action.
    private static final float WEIGTHED_TRIGGER_THRESHOLD = 1.73f;

    // |EDGE_WIDTH_DP| in physical pixel.
    private final float mEdgeWidthPx;

    @IntDef({GestureState.NONE, GestureState.STARTED, GestureState.DRAGGED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface GestureState {
        int NONE = 0;
        int STARTED = 1;
        int DRAGGED = 2;
        int GLOW = 3;
    }

    private final ViewGroup mParentView;
    private final Context mContext;
    private final Supplier<NavigationGlow> mGlowEffectSupplier;

    private final HistoryNavigationDelegate mDelegate;
    private final ActionDelegate mActionDelegate;

    private NavigationGlow mGlowEffect;

    private @GestureState int mState = GestureState.NONE;

    // Frame layout where the main logic turning the gesture into corresponding UI resides.
    private SideSlideLayout mSideSlideLayout;

    private NavigationSheet mNavigationSheet;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopNavigatingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure
    // it does not conflict with pending Android draws.
    private Runnable mDetachLayoutRunnable;

    /**
     * Interface to perform actions for navigating.
     */
    public interface ActionDelegate {
        /**
         * @param forward Direction to navigate. {@code true} if forward.
         * @return {@code true} if navigation toward the given direction is possible.
         */
        boolean canNavigate(boolean forward);

        /**
         * Execute navigation toward the given direction.
         * @param forward Direction to navigate. {@code true} if forward.
         */
        void navigate(boolean forward);

        /**
         * @return {@code true} if back action will close the current tab.
         */
        boolean willBackCloseTab();

        /**
         * @return {@code true} if back action will cause the app to exit.
         */
        boolean willBackExitApp();
    }

    public NavigationHandler(ViewGroup parentView, Context context,
            HistoryNavigationDelegate delegate, Supplier<NavigationGlow> glowEffectSupplier) {
        mParentView = parentView;
        mContext = context;
        mDelegate = delegate;
        mActionDelegate = delegate.createActionDelegate();
        mGlowEffectSupplier = glowEffectSupplier;
        mEdgeWidthPx = EDGE_WIDTH_DP * parentView.getResources().getDisplayMetrics().density;
    }

    private void createLayout() {
        mSideSlideLayout = new SideSlideLayout(mContext);
        mSideSlideLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mSideSlideLayout.setOnNavigationListener((forward) -> {
            mActionDelegate.navigate(forward);
            cancelStopNavigatingRunnable();
            mSideSlideLayout.post(getStopNavigatingRunnable());
        });

        mSideSlideLayout.setOnResetListener(() -> {
            if (mDetachLayoutRunnable != null) return;
            mDetachLayoutRunnable = () -> {
                mDetachLayoutRunnable = null;
                detachLayoutIfNecessary();
            };
            mSideSlideLayout.post(mDetachLayoutRunnable);
        });

        mNavigationSheet = NavigationSheet.isEnabled()
                ? NavigationSheet.create(mParentView, mContext,
                        mDelegate.getBottomSheetController(), mDelegate.createSheetDelegate())
                : NavigationSheet.DUMMY;
    }

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public void onTouchEvent(int action) {
        if (action == MotionEvent.ACTION_UP) {
            if (mState == GestureState.DRAGGED && mSideSlideLayout != null) {
                mSideSlideLayout.release(mNavigationSheet.isHidden());
                mNavigationSheet.release();
            } else if (mState == GestureState.GLOW && mGlowEffect != null) {
                mGlowEffect.release();
            }
        }
    }

    /**
     * @see GestureDetector#SimpleOnGestureListener#onDown(MotionEvent)
     */
    public boolean onDown() {
        mState = GestureState.STARTED;
        return true;
    }

    /**
     * Processes scroll event from {@link SimpleOnGestureListener#onScroll()}.
     * @param startX X coordinate of the position where gesture swipes from.
     * @param distanceX X delta between previous and current motion event.
     * @param distanceX Y delta between previous and current motion event.
     * @param endX X coordinate of the current motion event.
     * @param endY Y coordinate of the current motion event.
     */
    public boolean onScroll(
            float startX, float distanceX, float distanceY, float endX, float endY) {
        // onScroll needs handling only after the state moves away from |NONE|.
        if (mState == GestureState.NONE) return true;

        if (mState == GestureState.STARTED) {
            if (shouldTriggerUi(startX, distanceX, distanceY)) {
                boolean forward = distanceX > 0;
                if (mActionDelegate.canNavigate(forward)) {
                    showArrowWidget(forward);
                } else {
                    // |forward| should be true if we get here, since navigating back
                    // is always possible.
                    assert forward;
                    showGlow(endX, endY);
                }
            }
            if (!isActive()) mState = GestureState.NONE;
        }
        pull(-distanceX);
        return true;
    }

    private boolean shouldTriggerUi(float sX, float dX, float dY) {
        return Math.abs(dX) > Math.abs(dY) * WEIGTHED_TRIGGER_THRESHOLD
                && (sX < mEdgeWidthPx || (mParentView.getWidth() - mEdgeWidthPx) < sX);
    }

    /**
     * Start showing arrow widget for navigation back/forward.
     * @param forward {@code true} if navigating forward.
     */
    public void showArrowWidget(boolean forward) {
        if (mState != GestureState.STARTED) reset();
        if (mSideSlideLayout == null) createLayout();
        mSideSlideLayout.setEnabled(true);
        mSideSlideLayout.setDirection(forward);
        @CloseTarget
        int closeIndicator = getCloseIndicator(forward);
        mSideSlideLayout.setCloseIndicator(closeIndicator);
        attachLayoutIfNecessary();
        mSideSlideLayout.start();
        mNavigationSheet.start(forward, closeIndicator != CloseTarget.NONE);
        mState = GestureState.DRAGGED;
    }

    /**
     * Start showing edge glow effect.
     * @param startX X coordinate of the touch event at the beginning.
     * @param startY Y coordinate of the touch event at the beginning.
     */
    public void showGlow(float startX, float startY) {
        if (mState != GestureState.STARTED) reset();
        if (mGlowEffect == null) mGlowEffect = mGlowEffectSupplier.get();
        mGlowEffect.prepare(startX, startY);
        mState = GestureState.GLOW;
    }

    private boolean shouldShowCloseIndicator(boolean forward) {
        // Some tabs, upon back at the beginning of the history stack, should be just closed
        // than closing the entire app. In such case we do not show the close indicator.
        return !forward && mActionDelegate.willBackExitApp();
    }

    private @CloseTarget int getCloseIndicator(boolean forward) {
        // Some tabs, upon back at the beginning of the history stack, should be just closed
        // than closing the entire app.
        if (!forward && mActionDelegate.willBackCloseTab()) {
            return CloseTarget.TAB;
        } else if (!forward && mActionDelegate.willBackExitApp()) {
            return CloseTarget.APP;
        } else {
            return CloseTarget.NONE;
        }
    }

    /**
     * Signals a pull update.
     * @param delta The change in horizontal pull distance (positive if toward right,
     *         negative if left).
     */
    public void pull(float delta) {
        if (mState == GestureState.DRAGGED && mSideSlideLayout != null) {
            mSideSlideLayout.pull(delta);
            mNavigationSheet.onScroll(
                    delta, mSideSlideLayout.getOverscroll(), mSideSlideLayout.willNavigate());

            mSideSlideLayout.fadeArrow(!mNavigationSheet.isHidden(), /* animate= */ true);
            if (mNavigationSheet.isExpanded()) {
                mSideSlideLayout.hideArrow();
                mState = GestureState.NONE;
            }
        } else if (mState == GestureState.GLOW && mGlowEffect != null) {
            mGlowEffect.onScroll(-delta);
        }
    }

    /**
     * @return {@code true} if navigation was triggered and its UI is in action, or
     *         edge glow effect is visible.
     */
    public boolean isActive() {
        return mState == GestureState.DRAGGED || mState == GestureState.GLOW;
    }

    /**
     * @return {@code true} if navigation is not in operation.
     */
    public boolean isStopped() {
        return mState == GestureState.NONE;
    }

    /**
     * Release the active pull. If no pull has started, the release will be ignored.
     * If the pull was sufficiently large, the navigation sequence will be initiated.
     * @param allowNav Whether to allow a sufficiently large pull to trigger
     *         the navigation action and animation sequence.
     */
    public void release(boolean allowNav) {
        if (mState == GestureState.DRAGGED && mSideSlideLayout != null) {
            cancelStopNavigatingRunnable();
            mSideSlideLayout.release(allowNav && mNavigationSheet.isHidden());
            mNavigationSheet.release();
        } else if (mState == GestureState.GLOW && mGlowEffect != null) {
            mGlowEffect.release();
        }
    }

    /**
     * Reset navigation UI in action.
     */
    public void reset() {
        if (mState == GestureState.DRAGGED && mSideSlideLayout != null) {
            cancelStopNavigatingRunnable();
            mSideSlideLayout.reset();
        } else if (mState == GestureState.GLOW && mGlowEffect != null) {
            mGlowEffect.reset();
        }
        mState = GestureState.NONE;
    }

    /**
     * Performs cleanup upon destruction.
     */
    public void destroy() {
        if (mGlowEffect != null) mGlowEffect.destroy();
    }

    private void cancelStopNavigatingRunnable() {
        if (mStopNavigatingRunnable != null) {
            mSideSlideLayout.removeCallbacks(mStopNavigatingRunnable);
            mStopNavigatingRunnable = null;
        }
    }

    private void cancelDetachLayoutRunnable() {
        if (mDetachLayoutRunnable != null) {
            mSideSlideLayout.removeCallbacks(mDetachLayoutRunnable);
            mDetachLayoutRunnable = null;
        }
    }

    private Runnable getStopNavigatingRunnable() {
        if (mStopNavigatingRunnable == null) {
            mStopNavigatingRunnable = () -> mSideSlideLayout.stopNavigating();
        }
        return mStopNavigatingRunnable;
    }

    private void attachLayoutIfNecessary() {
        // The animation view is attached/detached on-demand to minimize overlap
        // with composited SurfaceView content.
        cancelDetachLayoutRunnable();
        if (mSideSlideLayout.getParent() == null) mParentView.addView(mSideSlideLayout);
    }

    private void detachLayoutIfNecessary() {
        if (mSideSlideLayout == null) return;
        cancelDetachLayoutRunnable();
        if (mSideSlideLayout.getParent() != null) mParentView.removeView(mSideSlideLayout);
    }
}
