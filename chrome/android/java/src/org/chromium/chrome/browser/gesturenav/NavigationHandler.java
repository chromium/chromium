// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.ACTION;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.ALLOW_NAV;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.BUBBLE_OFFSET;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.CLOSE_INDICATOR;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.DIRECTION;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.GESTURE_POS;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.GLOW_OFFSET;

import android.app.Activity;
import android.content.Context;
import android.gesture.GesturePoint;
import android.os.Build;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate.ActionType;
import org.chromium.chrome.browser.gesturenav.NavigationBubble.CloseTarget;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles history overscroll navigation controlling the underlying UI widget.
 */
class NavigationHandler implements TouchEventObserver {
    // Width of a rectangluar area in dp on the left/right edge used for navigation.
    // Swipe beginning from a point within these rects triggers the operation.
    @VisibleForTesting
    static final int EDGE_WIDTH_DP = 24;

    // Weighted value to determine when to trigger an edge swipe. Initial scroll
    // vector should form 30 deg or below to initiate swipe action.
    private static final float WEIGTHED_TRIGGER_THRESHOLD = 1.73f;

    // |EDGE_WIDTH_DP| in physical pixel.
    private final float mEdgeWidthPx;

    @IntDef({GestureState.NONE, GestureState.STARTED, GestureState.DRAGGED, GestureState.GLOW})
    @Retention(RetentionPolicy.SOURCE)
    @interface GestureState {
        int NONE = 0;
        int STARTED = 1;
        int DRAGGED = 2;
        int GLOW = 3;
    }

    @IntDef({GestureAction.SHOW_ARROW, GestureAction.SHOW_GLOW, GestureAction.RELEASE_BUBBLE,
            GestureAction.RELEASE_GLOW, GestureAction.RESET_BUBBLE, GestureAction.RESET_GLOW})
    @Retention(RetentionPolicy.SOURCE)
    @interface GestureAction {
        int SHOW_ARROW = 1;
        int SHOW_GLOW = 2;
        int RELEASE_BUBBLE = 3;
        int RELEASE_GLOW = 4;
        int RESET_BUBBLE = 5;
        int RESET_GLOW = 6;
    }

    private final ViewGroup mParentView;
    private final Context mContext;
    private final Handler mHandler = new Handler();

    // Frame layout where the main logic turning the gesture into corresponding UI resides.
    private SideSlideLayout mSideSlideLayout;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopNavigatingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure
    // it does not conflict with pending Android draws.
    private Runnable mDetachLayoutRunnable;
    private GestureDetector mDetector;
    private View.OnAttachStateChangeListener mAttachStateListener;
    private final BackActionDelegate mBackActionDelegate;
    @Nullable
    private TabOnBackGestureHandler mTabOnBackGestureHandler;
    private Tab mTab;
    private final Supplier<Boolean> mWillNavigateSupplier;

    private @GestureState int mState;

    private PropertyModel mModel;

    // Total horizontal pull offset for a swipe gesture.
    private float mPullOffsetX;
    // Total vertical pull offset for a swipe gesture.
    private float mPullOffsetY;

    private float mInitialX;
    private float mInitialY;

    private class SideNavGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent event) {
            return NavigationHandler.this.onDown();
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            // |onScroll| needs handling only after the state moved away from |NONE|.
            if (isStopped()) return true;
            return NavigationHandler.this.onScroll(
                    e1.getX(), distanceX, distanceY, e2.getX(), e2.getY());
        }
    }

    public NavigationHandler(PropertyModel model, ViewGroup parentView,
            BackActionDelegate backActionDelegate, Supplier<Boolean> supplier) {
        mModel = model;
        mParentView = parentView;
        mContext = parentView.getContext();
        mBackActionDelegate = backActionDelegate;
        mWillNavigateSupplier = supplier;
        mState = GestureState.NONE;

        mEdgeWidthPx = EDGE_WIDTH_DP * parentView.getResources().getDisplayMetrics().density;
        mDetector = new GestureDetector(mContext, new SideNavGestureListener());
        mAttachStateListener = new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {}

            @Override
            public void onViewDetachedFromWindow(View v) {
                reset();
            }
        };
        parentView.addOnAttachStateChangeListener(mAttachStateListener);
    }

    void setTab(Tab tab) {
        mTab = tab;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // Forward gesture events only for native pages/start surface. Rendered pages receive events
        // from SwipeRefreshHandler.
        if (!shouldProcessTouchEvents()) return false;
        return isActive();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        assert e != null : "The motion event in NavigationHandler shouldn't be null!";
        if (e == null || !shouldProcessTouchEvents()) return false;
        mDetector.onTouchEvent(e);
        if (e.getAction() == MotionEvent.ACTION_UP) release(true);
        return false;
    }

    private boolean shouldProcessTouchEvents() {
        return mTab != null && mTab.isNativePage() || mBackActionDelegate.isNavigable();
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
    @VisibleForTesting
    boolean onScroll(float startX, float distanceX, float distanceY, float endX, float endY) {
        // onScroll needs handling only after the state moves away from |NONE|.
        if (mState == GestureState.NONE || !isValidState()) return true;

        if (mState == GestureState.STARTED) {
            if (shouldTriggerUi(startX, distanceX, distanceY)) {
                triggerUi(distanceX > 0, endX, endY);
            }
            if (!isActive()) mState = GestureState.NONE;
        }
        pull(-distanceX, -distanceY);
        return true;
    }

    private boolean isValidState() {
        // We are in a valid state for UI process if the underlying tab is alive, or
        // start surface is showing.
        return mTab != null && !mTab.isDestroyed() || mBackActionDelegate.isNavigable();
    }

    private boolean shouldTriggerUi(float sX, float dX, float dY) {
        return Math.abs(dX) > Math.abs(dY) * WEIGTHED_TRIGGER_THRESHOLD
                && (sX < mEdgeWidthPx || (mParentView.getWidth() - mEdgeWidthPx) < sX);
    }

    /**
     * @see {@link HistoryNavigationCoordinator#triggerUi(boolean, float, float)}
     */
    boolean triggerUi(boolean forward, float x, float y) {
        if (!isValidState()) return false;

        mModel.set(DIRECTION, forward);
        boolean navigable = canNavigate(forward);
        if (navigable) {
            if (mState != GestureState.STARTED) mModel.set(ACTION, GestureAction.RESET_BUBBLE);
            mModel.set(CLOSE_INDICATOR, getCloseIndicator(forward));
            mModel.set(ACTION, GestureAction.SHOW_ARROW);
            mState = GestureState.DRAGGED;
        } else {
            if (mState != GestureState.STARTED) mModel.set(ACTION, GestureAction.RESET_GLOW);
            mModel.set(GESTURE_POS, new GesturePoint(x, y, 0L));
            mModel.set(ACTION, GestureAction.SHOW_GLOW);
            mState = GestureState.GLOW;
        }
        mInitialX = x;
        mInitialY = y;
        if (navigable && willUpdateTabHistory(forward)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_TRANSITIONS)) {
            mTabOnBackGestureHandler = TabOnBackGestureHandler.from(mTab);
            mTabOnBackGestureHandler.onBackStarted(
                    x, y, getProgress(), getBackDirection(), forward);
        }
        return navigable;
    }

    /**
     *
     * @param forward {@code true} for forward navigation, or {@code false} for back.
     * @return True if the gesture is going to navigate page rather than closing tab or exiting app.
     */
    boolean willUpdateTabHistory(boolean forward) {
        if (mTab == null) return false;
        return forward ? mTab.canGoForward() : mTab.canGoBack();
    }

    private boolean canNavigate(boolean forward) {
        // Navigating back is considered always possible (actual navigation, closing
        // tab, or exiting app).
        return !forward || mTab != null && mTab.canGoForward();
    }

    /**
     * Perform navigation back or forward.
     * @param forward {@code true} for forward navigation, or {@code false} for back.
     */
    void navigate(boolean forward) {
        if (!isValidState()) return;
        if (mTabOnBackGestureHandler != null) {
            // Delegate navigation to native side: supposed to be triggered after animation.
            return;
        }
        if (forward) {
            // Session history may have changed since the beginning of the gesture such that it's no
            // longer possible to go forward.
            if (mTab.canGoForward()) {
                mTab.goForward();
            }
        } else {
            // Perform back action at the next UI thread execution. The back action can
            // potentially close the tab we're running on, which causes use-after-destroy
            // exception if the closing operation is performed synchronously.
            mHandler.post(mBackActionDelegate::onBackGesture);
        }
    }

    /**
     * @return The type of target to close when left swipe is performed. Could be
     *         the current tab, Chrome app, or none as defined in {@link CloseTarget}.
     * @param forward {@code true} for forward navigation, or {@code false} for back.
     */
    private @CloseTarget int getCloseIndicator(boolean forward) {
        if (forward) return CloseTarget.NONE;

        @ActionType
        int type = mBackActionDelegate.getBackActionType(mTab);
        if (type == ActionType.CLOSE_TAB) {
            return CloseTarget.TAB;
        } else if (type == ActionType.EXIT_APP) {
            return CloseTarget.APP;
        } else {
            return CloseTarget.NONE;
        }
    }

    /**
     * @see {@link HistoryNavigationCoordinator#release(boolean)}
     */
    void release(boolean allowNav) {
        mModel.set(ALLOW_NAV, allowNav);
        if (mState == GestureState.DRAGGED) {
            mModel.set(ACTION, GestureAction.RELEASE_BUBBLE);
        } else if (mState == GestureState.GLOW) {
            mModel.set(ACTION, GestureAction.RELEASE_GLOW);
        }
        mPullOffsetX = mPullOffsetY = 0.f;
        if (mTabOnBackGestureHandler != null) {
            if (allowNav && mWillNavigateSupplier.get()) {
                mTabOnBackGestureHandler.onBackInvoked();
            } else {
                mTabOnBackGestureHandler.onBackCancelled();
            }
            mTabOnBackGestureHandler = null;
        }
    }

    /**
     * @see {@link HistoryNavigationCoordinator#reset()}
     */
    void reset() {
        if (mState == GestureState.DRAGGED) {
            mModel.set(ACTION, GestureAction.RESET_BUBBLE);
        } else if (mState == GestureState.GLOW) {
            mModel.set(ACTION, GestureAction.RESET_GLOW);
        }
        mState = GestureState.NONE;
        mPullOffsetX = mPullOffsetY = 0.f;
    }

    /**
     * @see {@link HistoryNavigationCoordinator#pull(float, float)}
     */
    void pull(float xDelta, float yDelta) {
        mPullOffsetX += xDelta;
        mPullOffsetY += yDelta;
        if (mState == GestureState.DRAGGED) {
            mModel.set(BUBBLE_OFFSET, mPullOffsetX);
        } else if (mState == GestureState.GLOW) {
            mModel.set(GLOW_OFFSET, mPullOffsetX);
        }
        if (mTabOnBackGestureHandler != null) {
            mTabOnBackGestureHandler.onBackProgressed(
                    getTouchX(), getTouchY(), getProgress(), getBackDirection());
        }
    }

    /**
     * @return Absolute X location of the touch point of the current gesture.
     */
    float getTouchX() {
        return mInitialX + mPullOffsetX;
    }

    /**
     * @return Absolute Y location of the touch point of the current gesture.
     */
    float getTouchY() {
        return mInitialY + mPullOffsetY;
    }

    /**
     * @return {@code true} if navigation was triggered and its UI is in action, or
     *         edge glow effect is visible.
     */
    boolean isActive() {
        return mState == GestureState.DRAGGED || mState == GestureState.GLOW;
    }

    /**
     * @return Whether the current gesture origins from the left side.
     */
    boolean fromLeftSide() {
        return mInitialX < mEdgeWidthPx;
    }

    @BackGestureEventSwipeEdge
    int getBackDirection() {
        return fromLeftSide() ? BackGestureEventSwipeEdge.LEFT : BackGestureEventSwipeEdge.RIGHT;
    }

    /**
     * @return {@code true} if navigation is not in operation.
     */
    private boolean isStopped() {
        return mState == GestureState.NONE;
    }

    /**
     * Get progress of back gesture. This is a mock of
     * {@link android.window.BackEvent#getProgress()}.
     */
    private float getProgress() {
        assert mTab != null;
        Activity activity = mTab.getWindowAndroid().getActivity().get();
        assert activity != null;
        int width;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            width = activity.getWindowManager().getCurrentWindowMetrics().getBounds().width();
        } else {
            DisplayMetrics displayMetrics = new DisplayMetrics();
            activity.getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
            width = displayMetrics.heightPixels;
        }
        return Math.max(0, getTouchX() / width);
    }

    /**
     * Performs cleanup upon destruction.
     */
    void destroy() {
        mParentView.removeOnAttachStateChangeListener(mAttachStateListener);
        mDetector = null;
    }

    TabOnBackGestureHandler getTabOnBackGestureHandlerForTesting() {
        return mTabOnBackGestureHandler;
    }
}
