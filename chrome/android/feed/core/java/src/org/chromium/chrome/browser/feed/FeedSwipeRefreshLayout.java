// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewConfiguration;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.ScrollListener;
import org.chromium.chrome.browser.ntp.ScrollableContainerDelegate;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.third_party.android.swiperefresh.CircleImageView;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;

/**
 * Makes the modified version of SwipeRefreshLayout support layout, measuring and touch handling
 * of direct child.
 */
public class FeedSwipeRefreshLayout extends SwipeRefreshLayout implements ScrollListener {
    public static final int IPH_WAIT_TIME_MS = 5 * 1000;
    // Offset in dips from the top of the view to where the progress spinner should start.
    private static final int SPINNER_START_OFFSET = 16;
    // Offset in dips from the top of the view to where the progress spinner should stop.
    private static final int SPINNER_END_OFFSET = 80;

    private final Activity mActivity;
    private View mTarget; // the target of the gesture.
    private final int mTouchSlop;
    private final ObserverList<SwipeRefreshLayout.OnRefreshListener> mRefreshListeners =
            new ObserverList<>();
    private float mLastMotionY;
    private boolean mIsBeingDragged;
    private ScrollableContainerDelegate mScrollableContainerDelegate;
    private int mHeaderOffset;

    /**
     * Creates and returns an instance of {@link FeedSwipeRefreshLayout}.
     * @param activity The current {@link Activity}.
     */
    public static FeedSwipeRefreshLayout create(@NonNull Activity activity) {
        if (!ChromeFeatureList.isInitialized()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_INTERACTIVE_REFRESH)) {
            return null;
        }
        FeedSwipeRefreshLayout instance = new FeedSwipeRefreshLayout(activity);
        instance.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        instance.setProgressBackgroundColorSchemeResource(
                org.chromium.ui.R.color.default_bg_color_elev_2);
        instance.setColorSchemeResources(org.chromium.ui.R.color.default_control_color_active);
        instance.setEnabled(false);
        final DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
        instance.setProgressViewOffset(false, (int) (SPINNER_START_OFFSET * metrics.density),
                (int) (SPINNER_END_OFFSET * metrics.density));
        instance.addOnRefreshListener(new SwipeRefreshLayout.OnRefreshListener() {
            @Override
            public void onRefresh() {
                String accessibilityRefreshString = activity.getResources().getString(
                        org.chromium.chrome.R.string.accessibility_swipe_refresh);
                instance.announceForAccessibility(accessibilityRefreshString);
                RecordUserAction.record("MobilePullGestureReloadNTP");
            }
        });
        return instance;
    }

    private FeedSwipeRefreshLayout(@NonNull Activity activity) {
        super(activity);
        mActivity = activity;
        mTouchSlop = ViewConfiguration.get(activity).getScaledTouchSlop();

        setOnRefreshListener(new OnRefreshListener() {
            @Override
            public void onRefresh() {
                for (OnRefreshListener listener : mRefreshListeners) {
                    listener.onRefresh();
                }
            }
        });
    }

    /** Shows an IPH. */
    public void showIPH(UserEducationHelper helper) {
        ViewGroup contentContainer = mActivity.findViewById(android.R.id.content);
        if (contentContainer == null) return;
        View toolbarView = contentContainer.findViewById(org.chromium.chrome.R.id.toolbar);
        if (toolbarView == null) return;
        helper.requestShowIPH(new IPHCommandBuilder(getContext().getResources(),
                FeatureConstants.FEED_SWIPE_REFRESH_FEATURE, R.string.feed_swipe_refresh_iph,
                R.string.accessibility_feed_swipe_refresh_iph)
                                      .setAnchorView(toolbarView)
                                      .setHighlightParams(
                                              new HighlightParams(HighlightShape.CIRCLE))
                                      .setDismissOnTouch(true)
                                      .setAutoDismissTimeout(IPH_WAIT_TIME_MS)
                                      .build());
    }

    /**
     * Enables the swipe gesture.
     * @param scrollableContainerDelegate Delegate for the scrollable container.
     */
    public void enableSwipe(ScrollableContainerDelegate scrollableContainerDelegate) {
        if (isEnabled()) return;
        setEnabled(true);

        if (scrollableContainerDelegate != null) {
            mScrollableContainerDelegate = scrollableContainerDelegate;
            mScrollableContainerDelegate.addScrollListener(this);
        }
    }

    /**
     * Disables the swipe gesture.
     */
    public void disableSwipe() {
        if (!isEnabled()) return;
        setEnabled(false);

        if (mScrollableContainerDelegate != null) {
            mScrollableContainerDelegate.removeScrollListener(this);
            mScrollableContainerDelegate = null;
        }
    }

    /**
     * Adds the listener to be notified when a refresh is triggered via the swipe gesture.
     * @param listener Listener to add.
     */
    public void addOnRefreshListener(SwipeRefreshLayout.OnRefreshListener listener) {
        mRefreshListeners.addObserver(listener);
    }

    /**
     * Removes the listener to be notified when a refresh is triggered via the swipe gesture.
     * @param listener Listener to remove.
     */
    public void removeOnRefreshListener(SwipeRefreshLayout.OnRefreshListener listener) {
        mRefreshListeners.removeObserver(listener);
    }

    private void ensureTarget() {
        if (mTarget == null) {
            for (int i = 0; i < getChildCount(); i++) {
                View child = getChildAt(i);
                if (!(child instanceof CircleImageView)) {
                    mTarget = child;
                    break;
                }
            }
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        ensureTarget();
        if (mTarget == null) {
            return;
        }
        final int width = getMeasuredWidth();
        final int height = getMeasuredHeight();
        final View child = mTarget;
        final int childLeft = getPaddingLeft();
        final int childTop = getPaddingTop();
        final int childWidth = width - getPaddingLeft() - getPaddingRight();
        final int childHeight = height - getPaddingTop() - getPaddingBottom();
        child.layout(childLeft, childTop, childLeft + childWidth, childTop + childHeight);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        ensureTarget();
        if (mTarget == null) {
            return;
        }
        mTarget.measure(MeasureSpec.makeMeasureSpec(
                                getMeasuredWidth() - getPaddingLeft() - getPaddingRight(),
                                MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(
                        getMeasuredHeight() - getPaddingTop() - getPaddingBottom(),
                        MeasureSpec.EXACTLY));
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (!isEnabled()) return false;
        if (mScrollableContainerDelegate != null) {
            // For start surface, the target view is task view which doesn't move. We need to rely
            // on onHeaderOffsetChanged event that is fired with header offset.
            if (mHeaderOffset != 0) return false;
        } else {
            // Otherwise for pure New Tab Page, the target view is RecyclerView and we can check it
            // directly to determine if it can scroll up.
            ensureTarget();
            if (mTarget == null || mTarget.canScrollVertically(-1)) return false;
        }

        final int action = event.getAction();
        switch (action) {
            case MotionEvent.ACTION_DOWN: {
                mIsBeingDragged = false;
                final float y = event.getY();
                if (y == -1) {
                    return false;
                }
                mLastMotionY = y;
                break;
            }

            case MotionEvent.ACTION_MOVE: {
                final float y = event.getY();
                if (y == -1) {
                    return false;
                }
                final float yDiff = y - mLastMotionY;
                if (yDiff > mTouchSlop && !mIsBeingDragged) {
                    mIsBeingDragged = true;
                    start();
                }
                break;
            }

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                mIsBeingDragged = false;
                break;
        }

        return mIsBeingDragged;
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) return false;
        final int action = event.getAction();
        switch (action) {
            case MotionEvent.ACTION_DOWN:
                mIsBeingDragged = false;
                break;

            case MotionEvent.ACTION_MOVE:
                pull(event.getY() - mLastMotionY);
                mLastMotionY = event.getY();
                break;

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                release(true);
                mIsBeingDragged = false;
                return false;
        }

        return true;
    }

    @Override
    public void onScrollStateChanged(@ScrollState int state) {}

    @Override
    public void onScrolled(int dx, int dy) {}

    @Override
    public void onHeaderOffsetChanged(int headerOffset) {
        mHeaderOffset = headerOffset;
    }
}
