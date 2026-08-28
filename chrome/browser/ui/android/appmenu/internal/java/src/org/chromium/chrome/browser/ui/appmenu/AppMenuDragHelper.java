// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import android.widget.PopupWindow;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
@NullMarked
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
    private boolean mMoved;
    private boolean mHasMovedOutOfButton;
    private int mMenuButtonScreenCenterY;

    // These are used in a function locally, but defined here to avoid heap allocation on every
    // touch event.
    private final Rect mScreenVisibleRect = new Rect();
    private final int[] mScreenVisiblePoint = new int[2];

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
        mDragScrolling.setTimeListener(
                (animation, totalTime, deltaTime) -> {
                    if (mAppMenu.getListView() == null) return;

                    // We keep both mDragScrollOffset and mDragScrollOffsetRounded because
                    // the actual scrolling is by the rounded value but at the same time we also
                    // want to keep the precise scroll value in float.
                    mDragScrollOffset += (deltaTime * 0.001f) * mDragScrollingVelocity;
                    int diff = Math.round(mDragScrollOffset - mDragScrollOffsetRounded);
                    mDragScrollOffsetRounded += diff;
                    mAppMenu.getListView().smoothScrollBy(diff, 0);

                    // Force touch move event to highlight items correctly for the scrolled
                    // position.
                    if (!Float.isNaN(mLastTouchX) && !Float.isNaN(mLastTouchY)) {
                        menuItemAction(
                                Math.round(mLastTouchX),
                                Math.round(mLastTouchY),
                                ItemAction.HIGHLIGHT);
                    }
                });

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
        mMoved = false;
        mHasMovedOutOfButton = false;

        if (startDragging) mDragScrolling.start();
    }

    /**
     * Dragging mode will be stopped by calling this function. Note that it will fall back to normal
     * non-dragging mode.
     */
    void finishDragging() {
        // If the menu is being dismissed, we cannot access mAppMenu.getPopup().getListView()
        // needed to by menuItemAction. Only clear highlighting if the menu is still showing.
        // See crbug.com/41241151.
        @Nullable PopupWindow popupWindow = mAppMenu.getPopup();
        if (popupWindow != null && popupWindow.isShowing()) {
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
        final ListView listView = mAppMenu.getListView();
        final float deltaY = Float.isNaN(mLastTouchY) ? 0.0f : (rawY - mLastTouchY);

        mLastTouchX = rawX;
        mLastTouchY = rawY;
        mMenuButtonScreenCenterY = getScreenVisibleRect(button).centerY();

        if (eventActionMasked == MotionEvent.ACTION_CANCEL) {
            mAppMenu.dismiss();
            return true;
        }

        if (eventActionMasked == MotionEvent.ACTION_MOVE) {
            mMoved = true;
        }

        boolean isInsideButton = pointInView(button, event.getX(), event.getY(), mScaledTouchSlop);
        if (!isInsideButton) {
            mHasMovedOutOfButton = true;
        }

        if (eventActionMasked == MotionEvent.ACTION_UP && (!mMoved || !mHasMovedOutOfButton)) {
            RecordUserAction.record("MobileUsingMenuBySwButtonTap");
            finishDragging();
            return true;
        }

        // Do not highlight or perform any item action until the user drags out of the anchor
        // button into the menu. If the finger is still on the button (!mHasMovedOutOfButton),
        // keep itemAction as CLEAR_HIGHLIGHT_ALL so adjacent menu items are not highlighted.
        @ItemAction int itemAction = ItemAction.CLEAR_HIGHLIGHT_ALL;
        if (mHasMovedOutOfButton) {
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
        }
        boolean didPerformClick = menuItemAction(roundedRawX, roundedRawY, itemAction);

        if (eventActionMasked == MotionEvent.ACTION_UP) {
            // When drag gesture ends on ACTION_UP, if the release did not perform an action on
            // a menu item (e.g. released on empty background space), dismiss the menu.
            if (!didPerformClick) {
                RecordUserAction.record("MobileUsingMenuBySwButtonDragging");
                mAppMenu.dismiss();
            }
            return true;
        }

        // Auto-scrolling velocity calculation for ongoing gestures:
        if (!mHasMovedOutOfButton) {
            // Do not auto-scroll while the user's touch has not moved outside the anchor button.
            mDragScrollingVelocity = 0.0f;
        } else if (eventActionMasked == MotionEvent.ACTION_MOVE) {
            // Auto scrolling on the top or the bottom of the listView.
            assumeNonNull(listView);
            if (listView.getHeight() > 0) {
                float autoScrollAreaRatio =
                        Math.min(
                                AUTO_SCROLL_AREA_MAX_RATIO,
                                mItemRowHeight * 1.2f / listView.getHeight());
                float normalizedY =
                        (rawY - getScreenVisibleRect(listView).top) / listView.getHeight();
                boolean isButtonAtTop =
                        mMenuButtonScreenCenterY <= getScreenVisibleRect(listView).centerY();

                if (normalizedY < autoScrollAreaRatio) {
                    // Top auto-scroll zone: auto-scroll up.
                    mDragScrollingVelocity =
                            (normalizedY / autoScrollAreaRatio - 1.0f) * mAutoScrollFullVelocity;
                } else if (normalizedY > 1.0f - autoScrollAreaRatio) {
                    // Bottom auto-scroll zone: auto-scroll down.
                    // For a bottom-anchored menu, suppress downward auto-scrolling if the user is
                    // dragging upward out of the bottom button (deltaY < 0). Only auto-scroll down
                    // when the user moves their finger downward (deltaY > 0).
                    if (!isButtonAtTop && deltaY < 0) {
                        mDragScrollingVelocity = 0.0f;
                    } else if (isButtonAtTop || deltaY > 0) {
                        mDragScrollingVelocity =
                                ((normalizedY - 1.0f) / autoScrollAreaRatio + 1.0f)
                                        * mAutoScrollFullVelocity;
                    }
                } else {
                    // Middle or not scrollable.
                    mDragScrollingVelocity = 0.0f;
                }
            }
        }

        return true;
    }

    private boolean pointInView(View view, float x, float y, float slop) {
        return x >= -slop
                && y >= -slop
                && x < (view.getWidth() + slop)
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
        assumeNonNull(listView);

        ArrayList<View> itemViews = new ArrayList<>();
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

            boolean shouldPerform =
                    itemView.isEnabled()
                            && itemView.isShown()
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
        assumeNonNull(listView);

        // When the menu button is at the top, the popup menu has an entrance animation that slides
        // down from the top. If we process dragging events while it is sliding down, it touches
        // multiple views passing under the user's finger. Thus, we wait until the first item slides
        // below the top menu button.
        //
        // However, for a bottom-anchored menu, the button center Y is at the bottom of the screen,
        // so `firstRow.bottom <= mMenuButtonScreenCenterY` is always true at scroll position 0 and
        // would permanently block all drag actions. Therefore, only apply this slide-down heuristic
        // when the button is at the top of the menu (`isButtonAtTop`).
        boolean isButtonAtTop =
                mMenuButtonScreenCenterY <= getScreenVisibleRect(listView).centerY();
        final View firstRow = listView.getChildAt(0);
        if (isButtonAtTop
                && listView.getFirstVisiblePosition() == 0
                && firstRow != null
                && firstRow.getTop() == 0
                && getScreenVisibleRect(firstRow).bottom <= mMenuButtonScreenCenterY) {
            return false;
        }

        return true;
    }
}
