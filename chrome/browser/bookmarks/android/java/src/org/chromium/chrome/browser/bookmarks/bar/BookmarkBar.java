// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.ui.util.MotionEventUtils;

/** View for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
class BookmarkBar extends LinearLayout {

    public interface RightClickCallback {
        void onRightClick(float x, float y);
    }

    private FrameLayout mOverflowButton;
    private @Nullable RightClickCallback mRightClickCallback;
    private final GestureDetector mGestureDetector;
    private float mLastTouchX;
    private float mLastTouchY;
    // Index 0 is the x coordinate, index 1 is the y coordinate.
    private final int[] mLocation = new int[2];
    private View mAllBookmarksButton;
    private View mItemsContainer;

    /**
     * Constructor that is called when inflating a bookmark bar from XML.
     *
     * @param context the context the bookmark bar is running in.
     * @param attrs the attributes of the XML tag that is inflating the bookmark bar.
     */
    public BookmarkBar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mGestureDetector =
                new GestureDetector(
                        context,
                        new SimpleOnGestureListener() {
                            @Override
                            public void onLongPress(MotionEvent e) {
                                if (isTouchOnEmptySpace(e)) {
                                    if (mRightClickCallback != null) {
                                        mRightClickCallback.onRightClick(e.getX(), e.getY());
                                    }
                                }
                            }
                        });
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mAllBookmarksButton = findViewById(R.id.bookmark_bar_all_bookmarks_button);
        mOverflowButton = findViewById(R.id.bookmark_bar_overflow_button);
        mItemsContainer = findViewById(R.id.bookmark_bar_items_container);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        mLastTouchX = event.getX();
        mLastTouchY = event.getY();
        mGestureDetector.onTouchEvent(event);
        return super.dispatchTouchEvent(event);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        // Prevent touch events from "falling through" to views below.
        return true;
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (MotionEventUtils.isPointerEvent(event)) {
            mLastTouchX = event.getX();
            mLastTouchY = event.getY();
            if (super.dispatchGenericMotionEvent(event)) {
                return true;
            }
            int action = event.getActionMasked();
            if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
                if (action == MotionEvent.ACTION_BUTTON_RELEASE
                        && event.getActionButton() == MotionEvent.BUTTON_SECONDARY) {
                    if (isTouchOnEmptySpace(event) && mRightClickCallback != null) {
                        mRightClickCallback.onRightClick(mLastTouchX, mLastTouchY);
                        return true;
                    }
                }
            }
            if (action == MotionEvent.ACTION_BUTTON_PRESS
                    || action == MotionEvent.ACTION_BUTTON_RELEASE
                    || action == MotionEvent.ACTION_SCROLL) {
                return true;
            }
            return false;
        }
        return super.dispatchGenericMotionEvent(event);
    }

    /**
     * Checks whether a touch event is on empty space of the bookmark bar. Coordinate hit-testing is
     * necessary because RecyclerView occupies the bulk of the bar and consumes touch events without
     * bubbling empty-space long-clicks to the parent.
     */
    private boolean isTouchOnEmptySpace(MotionEvent e) {
        float rawX = e.getRawX();
        float rawY = e.getRawY();

        if (isPointInsideView(mAllBookmarksButton, rawX, rawY)) {
            return false;
        }

        if (mOverflowButton != null
                && mOverflowButton.getVisibility() == VISIBLE
                && isPointInsideView(mOverflowButton, rawX, rawY)) {
            return false;
        }

        if (mItemsContainer instanceof RecyclerView recyclerView
                && recyclerView.getVisibility() == VISIBLE) {
            recyclerView.getLocationOnScreen(mLocation);
            float xInRecycler = rawX - mLocation[0];
            float yInRecycler = rawY - mLocation[1];
            View child = recyclerView.findChildViewUnder(xInRecycler, yInRecycler);
            if (child != null) {
                return false;
            }
        }

        return true;
    }

    private boolean isPointInsideView(@Nullable View view, float rawX, float rawY) {
        if (view == null || view.getVisibility() != VISIBLE) {
            return false;
        }
        view.getLocationOnScreen(mLocation);
        return rawX >= mLocation[0]
                && rawX < mLocation[0] + view.getWidth()
                && rawY >= mLocation[1]
                && rawY < mLocation[1] + view.getHeight();
    }

    /**
     * Sets the callback to notify when the bookmark bar is right-clicked on empty space.
     *
     * @param callback the callback to notify.
     */
    public void setRightClickCallback(@Nullable RightClickCallback callback) {
        mRightClickCallback = callback;
    }

    /**
     * Sets the callback to notify of bookmark bar overflow button click events.
     *
     * @param callback the callback to notify.
     */
    public void setOverflowButtonClickCallback(@Nullable Runnable callback) {
        mOverflowButton.setOnClickListener(callback != null ? (v) -> callback.run() : null);
    }

    /**
     * Sets the visibility for the bookmark bar overflow button.
     *
     * @param visibility the visibility for the overflow button.
     */
    public void setOverflowButtonVisibility(int visibility) {
        mOverflowButton.setVisibility(visibility);
    }

    /**
     * @return The overflow button view.
     */
    public FrameLayout getOverflowButton() {
        return mOverflowButton;
    }
}
