// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.ACTION;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.ALLOW_NAV;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.BUBBLE_OFFSET;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.CLOSE_INDICATOR;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.DIRECTION;
import static org.chromium.chrome.browser.gesturenav.GestureNavigationProperties.EDGE;

import android.app.Activity;
import android.content.Context;
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

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.back_press.BackPressMetrics;
import org.chromium.chrome.browser.gesturenav.BackActionDelegate.ActionType;
import org.chromium.chrome.browser.gesturenav.NavigationBubble.CloseTarget;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles history overscroll navigation controlling the underlying UI widget. Note: used only from
 * 3-button navigation mode. For gestural navigation mode, see {@link
 * ToolbarManager#OnBackPressHandler}
 */
class NavigationHandler implements TouchEventObserver {
    // Width of a rectangluar area in dp on the left/right edge used for navigation.
    // Swipe beginning from a point within these rects triggers the operation.
    @VisibleForTesting static final int EDGE_WIDTH_DP = 24;

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

    @IntDef({GestureAction.SHOW_ARROW, GestureAction.RELEASE_BUBBLE, GestureAction.RESET_BUBBLE})
    @Retention(RetentionPolicy.SOURCE)
    @interface GestureAction {
        int SHOW_ARROW = 1;
        int RELEASE_BUBBLE = 2;
        int RESET_BUBBLE = 3;
    }

    @IntDef({
        TriggerUiCallSource.NO_TRIGGER,
        TriggerUiCallSource.ON_SCROLL,
        TriggerUiCallSource.WEBPAGE_OVERSCROLL
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TriggerUiCallSource {
        int NO_TRIGGER = 0;
        int ON_SCROLL = 1;
        int WEBPAGE_OVERSCROLL = 2;
    }

    private final ViewGroup mParentView;
    private final Context mContext;
    private final Handler mHandler = new Handler();

    private GestureDetector mDetector;
    private View.OnAttachStateChangeListener mAttachStateListener;
    private final BackActionDelegate mBackActionDelegate;
    @Nullable private TabOnBackGestureHandler mTabOnBackGestureHandler;
    private Tab mTab;
    private final Supplier<Boolean> mWillNavigateSupplier;

    private @GestureState int mState;

    private PropertyModel mModel;

    // Total horizontal pull offset for a swipe gesture.
    private float mPullOffsetX;
    // Total vertical pull offset for a swipe gesture.
    private float mPullOffsetY;

    private @BackGestureEventSwipeEdge int mInitiatingEdge;

    private @TriggerUiCallSource int mTriggerUiCallSource;

    private boolean mBackGestureForTabHistoryInProgress;
    private boolean mStartNavDuringOngoingGesture;
    private TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onDidStartNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigationHandle) {
                    if (tab != mTab) return;
                    mStartNavDuringOngoingGesture |= mBackGestureForTabHistoryInProgress;
                }
            };

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

    public NavigationHandler(
            PropertyModel model,
            ViewGroup parentView,
            BackActionDelegate backActionDelegate,
            Supplier<Boolean> supplier) {
        mModel = model;
        mParentView = parentView;
        mContext = parentView.getContext();
        mBackActionDelegate = backActionDelegate;
        mWillNavigateSupplier = supplier;
        mState = GestureState.NONE;

        mTriggerUiCallSource = TriggerUiCallSource.NO_TRIGGER;

        mEdgeWidthPx = EDGE_WIDTH_DP * parentView.getResources().getDisplayMetrics().density;
        mDetector = new GestureDetector(mContext, new SideNavGestureListener());
        mAttachStateListener =
                new View.OnAttachStateChangeListener() {
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
        if (mTab != null) mTab.removeObserver(mTabObserver);
        mBackGestureForTabHistoryInProgress = false;
        mTab = tab;
        if (tab != null) tab.addObserver(mTabObserver);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // Forward gesture events only for native pages. Rendered pages receive events
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
        return mTab != null && mTab.isNativePage();
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
                triggerUi(
                        distanceX > 0
                                ? BackGestureEventSwipeEdge.RIGHT
                                : BackGestureEventSwipeEdge.LEFT,
                        TriggerUiCallSource.ON_SCROLL);
            }
            if (!isActive()) mState = GestureState.NONE;
        }
        pull(-distanceX, -distanceY);
        return true;
    }

    private boolean isValidState() {
        // We are in a valid state for UI process if the underlying tab is alive.
        return mTab != null && !mTab.isDestroyed();
    }

    private boolean shouldTriggerUi(float sX, float dX, float dY) {
        return Math.abs(dX) > Math.abs(dY) * WEIGTHED_TRIGGER_THRESHOLD
                && (sX < mEdgeWidthPx || (mParentView.getWidth() - mEdgeWidthPx) < sX);
    }

    /**
     * @see {@link HistoryNavigationCoordinator#triggerUi(int)}
     */
    boolean triggerUi(
            @BackGestureEventSwipeEdge int initiatingEdge,
            @TriggerUiCallSource int triggerUiCallSource) {
        if (!isValidState()) return false;

        if (mTriggerUiCallSource != TriggerUiCallSource.NO_TRIGGER) {
            assert false
                    : "triggerUi has been already called. mInitiatingEdge: "
                            + String.valueOf(mInitiatingEdge)
                            + ". initiatingEdge passed to the function: "
                            + String.valueOf(initiatingEdge)
                            + ". Previous triggerUi call source: "
                            + String.valueOf(mTriggerUiCallSource)
                            + ". Current triggerUi call source: "
                            + String.valueOf(triggerUiCallSource);
            Log.i(
                    NavigationHandler.class.getSimpleName(),
                    "triggerUi has been already called. mInitiatingEdge: "
                            + String.valueOf(mInitiatingEdge)
                            + ". initiatingEdge passed to the function: "
                            + String.valueOf(initiatingEdge)
                            + ". Previous triggerUi call source: "
                            + String.valueOf(mTriggerUiCallSource)
                            + ". Current triggerUi call source: "
                            + String.valueOf(triggerUiCallSource));
        }
        mTriggerUiCallSource = triggerUiCallSource;

        mInitiatingEdge = initiatingEdge;

        boolean forward = mInitiatingEdge == BackGestureEventSwipeEdge.RIGHT;

        // If the UI uses an RTL layout, it may be necessary to flip the meaning of each edge so
        // that the left edge goes forward and the right goes back.
        if (LocalizationUtils.shouldMirrorBackForwardGestures()) {
            forward = !forward;
        }

        mModel.set(DIRECTION, forward);
        mModel.set(EDGE, mInitiatingEdge);
        if (canNavigate(forward)) {
            if (mState != GestureState.STARTED) mModel.set(ACTION, GestureAction.RESET_BUBBLE);
            mModel.set(CLOSE_INDICATOR, getCloseIndicator(forward));
            mModel.set(ACTION, GestureAction.SHOW_ARROW);
            mState = GestureState.DRAGGED;

            if (willUpdateTabHistory(forward)) {
                if (GestureNavigationUtils.allowTransition(mTab, forward)) {
                    if (TabOnBackGestureHandler.shouldAnimateNavigationTransition(
                            forward, initiatingEdge)) {
                        // Always force to show the top control at the start of the gesture.
                        TabBrowserControlsConstraintsHelper.update(
                                mTab, BrowserControlsState.SHOWN, /* animate= */ true);
                    }
                    mTabOnBackGestureHandler = TabOnBackGestureHandler.from(mTab);
                    mTabOnBackGestureHandler.onBackStarted(getProgress(), mInitiatingEdge, forward);
                }
                BackPressMetrics.recordNavStatusOnGestureStart(
                        mTab.getWebContents().hasUncommittedNavigationInPrimaryMainFrame(),
                        mTab.getWindowAndroid().getActivity().get().getWindow());
                mStartNavDuringOngoingGesture = false;
                mBackGestureForTabHistoryInProgress = true;
            }
            mBackActionDelegate.onGestureHandled();
        } else {
            mBackActionDelegate.onGestureUnhandled();
        }

        return true;
    }

    /**
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
        return !forward || (mTab != null && mTab.canGoForward());
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

        @ActionType int type = mBackActionDelegate.getBackActionType(mTab);
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
        // If the back gesture will update history, record the metrics.
        if (mBackGestureForTabHistoryInProgress) {
            BackPressMetrics.recordNavStatusDuringGesture(
                    mStartNavDuringOngoingGesture,
                    mTab.getWindowAndroid().getActivity().get().getWindow());
        }
        mBackGestureForTabHistoryInProgress = false;
        mStartNavDuringOngoingGesture = false;
        mModel.set(ALLOW_NAV, allowNav);
        if (mState == GestureState.DRAGGED) {
            mModel.set(ACTION, GestureAction.RELEASE_BUBBLE);
        }
        mPullOffsetX = 0.f;
        mPullOffsetY = 0.f;
        if (mTabOnBackGestureHandler != null) {
            if (allowNav && mWillNavigateSupplier.get()) {
                mTabOnBackGestureHandler.onBackInvoked();
            } else {
                mTabOnBackGestureHandler.onBackCancelled();
            }
            mTabOnBackGestureHandler = null;
        }
        mTriggerUiCallSource = TriggerUiCallSource.NO_TRIGGER;
    }

    /**
     * @see {@link HistoryNavigationCoordinator#reset()}
     */
    void reset() {
        if (mState == GestureState.DRAGGED) {
            mModel.set(ACTION, GestureAction.RESET_BUBBLE);
        }
        mState = GestureState.NONE;
        mTriggerUiCallSource = TriggerUiCallSource.NO_TRIGGER;
        mPullOffsetX = 0.f;
        mPullOffsetY = 0.f;
    }

    /**
     * @see {@link HistoryNavigationCoordinator#pull(float, float)}
     */
    void pull(float xDelta, float yDelta) {
        mPullOffsetX += xDelta;
        mPullOffsetY += yDelta;
        if (mState == GestureState.DRAGGED) {
            mModel.set(BUBBLE_OFFSET, mPullOffsetX);
        }
        if (mTabOnBackGestureHandler != null) {
            mTabOnBackGestureHandler.onBackProgressed(getProgress(), mInitiatingEdge);
        }
    }

    /**
     * @return {@code true} if navigation was triggered and its UI is in action, or edge glow effect
     *     is visible.
     */
    boolean isActive() {
        return mState == GestureState.DRAGGED || mState == GestureState.GLOW;
    }

    /**
     * @return Which edge the current gesture was initiated from.
     */
    @BackGestureEventSwipeEdge
    int getInitiatingEdge() {
        return mInitiatingEdge;
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

        // Progress runs from 0 to 1 even when pulling from the right edge.
        float offset =
                mInitiatingEdge == BackGestureEventSwipeEdge.LEFT ? mPullOffsetX : -mPullOffsetX;

        return Math.min(Math.max(0, offset / width), 1);
    }

    /** Performs cleanup upon destruction. */
    void destroy() {
        if (mTab != null) {
            assert mTabObserver != null : "Always has a tab observer";
            mTab.removeObserver(mTabObserver);
        }
        mParentView.removeOnAttachStateChangeListener(mAttachStateListener);
        mDetector = null;
    }

    TabOnBackGestureHandler getTabOnBackGestureHandlerForTesting() {
        return mTabOnBackGestureHandler;
    }
}
