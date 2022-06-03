// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.animation.TimeAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ui.appmenu.internal.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * Handles the drag touch events on AppMenu that start from the menu button.
 *
 * Lint suppression for NewApi is added because we are using TimeAnimator class that was marked
 * hidden in API 16.
 */
@SuppressLint("NewApi")
class AppMenuDragHelper {
    private final Context mContext;
    private final AppMenu mAppMenu;

    // Internally used action constants for dragging.
    @IntDef({ItemAction.HIGHLIGHT, ItemAction.PERFORM, ItemAction.CLEAR_HIGHLIGHT_ALL})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ItemAction {
        int HIGHLIGHT = 0;
        int PERFORM = 1;
        int CLEAR_HIGHLIGHT_ALL = 2;
    }

    private static final float AUTO_SCROLL_AREA_MAX_RATIO = 0.25f;

    // Dragging related variables, i.e., menu showing initiated by touch down and drag to navigate.
    private final float mAutoScrollFullVelocity;
    private final TimeAnimator mDragScrolling = new TimeAnimator();
    private float mDragScrollOffset;
    private int mDragScrollOffsetRounded;
    private volatile float mDragScrollingVelocity;
    private volatile float mLastTouchX;
    private volatile float mLastTouchY;
    private final int mItemRowHeight;
    private boolean mIsSingleTapCanceled;
    private int mMenuButtonScreenCenterY;

    // These are used in a function locally, but defined here to avoid heap allocation on every
    // touch event.
    private final Rect mScreenVisibleRect = new Rect();
    private final int[] mScreenVisiblePoint = new int[2];

    private final int mTapTimeout;
    private final int mScaledTouchSlop;

    AppMenuDragHelper(Context context, AppMenu appMenu, int itemRowHeight) {
        mContext = context;
        mAppMenu = appMenu;
        mItemRowHeight = itemRowHeight;
        Resources res = mContext.getResources();
        mAutoScrollFullVelocity = res.getDimensionPixelSize(R.dimen.auto_scroll_full_velocity);
        // If user is dragging and the popup ListView is too big to display at once,
        // mDragScrolling animator scrolls mPopup.getListView() automatically depending on
        // the user's touch position.
        mDragScrolling.setTimeListener((animation, totalTime, deltaTime) -> {
            if (mAppMenu.getListView() == null) return;

            // We keep both mDragScrollOffset and mDragScrollOffsetRounded because
            // the actual scrolling is by the rounded value but at the same time we also
            // want to keep the precise scroll value in float.
            mDragScrollOffset += (deltaTime * 0.001f) * mDragScrollingVelocity;
            int diff = Math.round(mDragScrollOffset - mDragScrollOffsetRounded);
            mDragScrollOffsetRounded += diff;
            mAppMenu.getListView().smoothScrollBy(diff, 0);

            // Force touch move event to highlight items correctly for the scrolled position.
            if (!Float.isNaN(mLastTouchX) && !Float.isNaN(mLastTouchY)) {
                menuItemAction(
                        Math.round(mLastTouchX), Math.round(mLastTouchY), ItemAction.HIGHLIGHT);
            }
        });

        // We use medium timeout, the average of tap and long press timeouts. This is consistent
        // with ListPopupWindow#ForwardingListener implementation.
        mTapTimeout =
                (ViewConfiguration.getTapTimeout() + ViewConfiguration.getLongPressTimeout()) / 2;
        mScaledTouchSlop = ViewConfiguration.get(mContext).getScaledTouchSlop();
    }

    /**
     * Sets up all the internal state to prepare for menu dragging.
     * @param startDragging      Whether dragging is started. For example, if the app menu
     *                           is showed by tapping on a button, this should be false. If it is
     *                           showed by start dragging down on the menu button, this should be
     *                           true.
     */
    void onShow(boolean startDragging) {
        mLastTouchX = Float.NaN;
        mLastTouchY = Float.NaN;
        mDragScrollOffset = 0.0f;
        mDragScrollOffsetRounded = 0;
        mDragScrollingVelocity = 0.0f;
        mIsSingleTapCanceled = false;

        if (startDragging) mDragScrolling.start();
    }

    /**
     * Dragging mode will be stopped by calling this function. Note that it will fall back to normal
     * non-dragging mode.
     */
    void finishDragging() {
        // If the menu is being dismissed, we cannot access mAppMenu.getPopup().getListView()
        // needed to by menuItemAction. Only clear highlighting if the menu is still showing.
        // See crbug.com/589805.
        if (mAppMenu.getPopup().isShowing()) {
            menuItemAction(0, 0, ItemAction.CLEAR_HIGHLIGHT_ALL);
        }
        mDragScrolling.cancel();
    }

    /**
     * Gets all the touch events and updates dragging related logic. Note that if this app menu
     * is initiated by software UI control, then the control should set onTouchListener and forward
     * all the events to this method because the initial UI control that processed ACTION_DOWN will
     * continue to get all the subsequent events.
     *
     * @param event Touch event to be processed.
     * @param button Button that received the touch event.
     * @return Whether the event is handled.
     */
    boolean handleDragging(MotionEvent event, View button) {
        if (!mAppMenu.isShowing() || !mDragScrolling.isRunning()) return false;

        // We will only use the screen space coordinate (rawX, rawY) to reduce confusion.
        // This code works across many different controls, so using local coordinates will be
        // a disaster.

        final float rawX = event.getRawX();
        final float rawY = event.getRawY();
        final int roundedRawX = Math.round(rawX);
        final int roundedRawY = Math.round(rawY);
        final int eventActionMasked = event.getActionMasked();
        final long timeSinceDown = event.getEventTime() - event.getDownTime();
        final ListView listView = mAppMenu.getListView();

        mLastTouchX = rawX;
        mLastTouchY = rawY;
        mMenuButtonScreenCenterY = getScreenVisibleRect(button).centerY();

        if (eventActionMasked == MotionEvent.ACTION_CANCEL) {
            mAppMenu.dismiss();
            return true;
        } else if (eventActionMasked == MotionEvent.ACTION_UP) {
            RecordHistogram.recordTimesHistogram("WrenchMenu.TouchDuration", timeSinceDown);
        }

        mIsSingleTapCanceled |= timeSinceDown > mTapTimeout;
        mIsSingleTapCanceled |= !pointInView(button, event.getX(), event.getY(), mScaledTouchSlop);
        if (!mIsSingleTapCanceled && eventActionMasked == MotionEvent.ACTION_UP) {
            RecordUserAction.record("MobileUsingMenuBySwButtonTap");
            finishDragging();
        }

        // After this line, drag scrolling is happening.
        if (!mDragScrolling.isRunning()) return false;

        boolean didPerformClick = false;
        @ItemAction
        int itemAction = ItemAction.CLEAR_HIGHLIGHT_ALL;
        switch (eventActionMasked) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
                itemAction = ItemAction.HIGHLIGHT;
                break;
            case MotionEvent.ACTION_UP:
                itemAction = ItemAction.PERFORM;
                break;
            default:
                break;
        }
        didPerformClick = menuItemAction(roundedRawX, roundedRawY, itemAction);

        if (eventActionMasked == MotionEvent.ACTION_UP && !didPerformClick) {
            RecordUserAction.record("MobileUsingMenuBySwButtonDragging");
            mAppMenu.dismiss();
        } else if (eventActionMasked == MotionEvent.ACTION_MOVE) {
            // Auto scrolling on the top or the bottom of the listView.
            if (listView.getHeight() > 0) {
                float autoScrollAreaRatio = Math.min(
                        AUTO_SCROLL_AREA_MAX_RATIO, mItemRowHeight * 1.2f / listView.getHeight());
                float normalizedY =
                        (rawY - getScreenVisibleRect(listView).top) / listView.getHeight();
                if (normalizedY < autoScrollAreaRatio) {
                    // Top
                    mDragScrollingVelocity =
                            (normalizedY / autoScrollAreaRatio - 1.0f) * mAutoScrollFullVelocity;
                } else if (normalizedY > 1.0f - autoScrollAreaRatio) {
                    // Bottom
                    mDragScrollingVelocity = ((normalizedY - 1.0f) / autoScrollAreaRatio + 1.0f)
                            * mAutoScrollFullVelocity;
                } else {
                    // Middle or not scrollable.
                    mDragScrollingVelocity = 0.0f;
                }
            }
        }

        return true;
    }

    private boolean pointInView(View view, float x, float y, float slop) {
        return x >= -slop && y >= -slop && x < (view.getWidth() + slop)
                && y < (view.getHeight() + slop);
    }

    /**
     * Performs the specified action on the menu item specified by the screen coordinate position.
     * @param screenX X in screen space coordinate.
     * @param screenY Y in screen space coordinate.
     * @param action  Action type to perform, it should be one of ITEM_ACTION_* constants.
     * @return true whether or not a menu item is performed (executed).
     */
    private boolean menuItemAction(int screenX, int screenY, @ItemAction int action) {
        if (!isReadyForMenuItemAction()) return false;

        ListView listView = mAppMenu.getListView();

        ArrayList<View> itemViews = new ArrayList<View>();
        for (int i = 0; i < listView.getChildCount(); ++i) {
            boolean hasImageButtons = false;
            if (listView.getChildAt(i) instanceof LinearLayout) {
                LinearLayout layout = (LinearLayout) listView.getChildAt(i);
                for (int j = 0; j < layout.getChildCount(); ++j) {
                    itemViews.add(layout.getChildAt(j));
                    if (layout.getChildAt(j) instanceof ImageButton) hasImageButtons = true;
                }
            }
            if (!hasImageButtons) itemViews.add(listView.getChildAt(i));
        }

        boolean didPerformClick = false;
        for (int i = 0; i < itemViews.size(); ++i) {
            View itemView = itemViews.get(i);

            boolean shouldPerform = itemView.isEnabled() && itemView.isShown()
                    && getScreenVisibleRect(itemView).contains(screenX, screenY);

            switch (action) {
                case ItemAction.HIGHLIGHT:
                    itemView.setPressed(shouldPerform);
                    break;
                case ItemAction.PERFORM:
                    if (shouldPerform) {
                        RecordUserAction.record("MobileUsingMenuBySwButtonDragging");
                        itemView.performClick();
                        didPerformClick = true;
                    }
                    break;
                case ItemAction.CLEAR_HIGHLIGHT_ALL:
                    itemView.setPressed(false);
                    break;
                default:
                    assert false;
                    break;
            }
        }
        return didPerformClick;
    }

    /**
     * @return Visible rect in screen coordinates for the given View.
     */
    @VisibleForTesting
    Rect getScreenVisibleRect(View view) {
        view.getLocalVisibleRect(mScreenVisibleRect);
        view.getLocationOnScreen(mScreenVisiblePoint);
        mScreenVisibleRect.offset(mScreenVisiblePoint[0], mScreenVisiblePoint[1]);
        return mScreenVisibleRect;
    }

    @VisibleForTesting
    boolean isReadyForMenuItemAction() {
        ListView listView = mAppMenu.getListView();

        // Starting M, we have a popup menu animation that slides down. If we process dragging
        // events while it's sliding, it will touch many views that are passing by user's finger,
        // which is not desirable. So we only process when the first item is below the menu button.
        // Unfortunately, there is no available listener for sliding animation finished. Thus the
        // following nasty heuristics.
        final View firstRow = listView.getChildAt(0);
        if (listView.getFirstVisiblePosition() == 0 && firstRow != null && firstRow.getTop() == 0
                && getScreenVisibleRect(firstRow).bottom <= mMenuButtonScreenCenterY) {
            return false;
        }

        return true;
    }
}
