// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewConfiguration;

import org.chromium.third_party.android.swiperefresh.CircleImageView;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;

/**
 * Makes the modified version of SwipeRefreshLayout support layout, measuring and touch handling
 * of direct child.
 */
public class FeedSwipeRefreshLayout extends SwipeRefreshLayout {
    private View mTarget; // the target of the gesture.
    private int mTouchSlop;
    private float mLastMotionY;
    private boolean mIsBeingDragged;

    FeedSwipeRefreshLayout(Context context) {
        super(context);
        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
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
        ensureTarget();
        if (mTarget == null || mTarget.canScrollVertically(-1)) {
            return false;
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
}
