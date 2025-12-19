// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

/**
 * Helper class to handle the custom drag and drop interaction logic for {@link
 * ImprovedBookmarkRow}.
 */
@NullMarked
public class BookmarkManagerDragHelper implements View.OnAttachStateChangeListener {
    private static final int DEFAULT_DRAG_START_DELAY_MS = 500;
    private static final int DRAG_START_DELAY_MS = 100;
    private static final int SELECTION_START_DELAY_MS = 500;

    private final Context mContext;
    private final BookmarkId mBookmarkId;
    private final SelectionDelegate<BookmarkId> mSelectionDelegate;
    private final ItemTouchHelper mItemTouchHelper;
    private final RecyclerView mRecyclerView;
    private final RecyclerView.ViewHolder mViewHolder;
    private final boolean mIsDragEnabled;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final int mTouchSlop;
    private float mHandleDownX;
    private float mHandleDownY;
    // Distinguishes between a click and a long-press.
    private boolean mIsLongPressTriggered;

    private final Runnable mSelectRunnable = this::selectItem;
    private final Runnable mStartDragRunnable = this::startDrag;

    /**
     * @param context Context for resources and touch slop.
     * @param bookmarkId The ID of the bookmark bound to the row.
     * @param selectionDelegate Delegate to handle selection toggling.
     * @param itemTouchHelper Helper to start the drag interaction.
     * @param recyclerView The RecyclerView containing the ImprovedBookmarkRow item (to control
     *     scrolling).
     * @param viewHolder The specific ViewHolder object that wraps and manages the
     *     ImprovedBookmarkRow view instance. The RecyclerView creates a ViewHolder for every
     *     visible row to hold its data and state.
     * @param isDragEnabled Whether dragging is allowed for this specific item.
     */
    public BookmarkManagerDragHelper(
            Context context,
            BookmarkId bookmarkId,
            SelectionDelegate<BookmarkId> selectionDelegate,
            ItemTouchHelper itemTouchHelper,
            RecyclerView recyclerView,
            RecyclerView.ViewHolder viewHolder,
            boolean isDragEnabled) {
        mContext = context;
        mBookmarkId = bookmarkId;
        mSelectionDelegate = selectionDelegate;
        assert itemTouchHelper != null;
        mItemTouchHelper = itemTouchHelper;
        mRecyclerView = recyclerView;
        mViewHolder = viewHolder;
        mIsDragEnabled = isDragEnabled;

        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        // Listen for detach events to clean up timers.
        mViewHolder.itemView.addOnAttachStateChangeListener(this);
    }

    /**
     * Handles touch events for the Bookmark Row body. Implements the "Delayed Selection" and
     * "Delayed Drag" logic. Also handles Click/tap interactions if no long-press occurred.
     *
     * @param v The view the touch event has been dispatched to (ImprovedBookmarkRow).
     * @param event The MotionEvent object containing full information about the event.
     * @return True if the listener has consumed the event, false otherwise.
     */
    public boolean onRowBodyTouch(View v, MotionEvent event) {
        // The mobile bookmarks folder, bookmarks bar folder, etc. are not draggable.
        if (!mIsDragEnabled) return false;

        // Prevents double action since the drag handle has its own touch listener.
        View dragHandle = v.findViewById(R.id.drag_handle);
        if (dragHandle != null && isPointInsideView(dragHandle, event.getX(), event.getY())) {
            return false;
        }

        int action = event.getActionMasked();

        if (action == MotionEvent.ACTION_DOWN) {
            // manually trigger press state.
            v.setPressed(true);
            // Capture the start coordinates relative to screen to calculate the delta movement.
            mHandleDownX = event.getRawX();
            mHandleDownY = event.getRawY();

            // Reset flag. New touch, so long-press hasn't happened yet.
            mIsLongPressTriggered = false;

            boolean isSelected = mSelectionDelegate.isItemSelected(mBookmarkId);
            boolean selectionEnabled = mSelectionDelegate.isSelectionEnabled();

            if (isSelected) {
                // Scenario 2: Row is selected. -> 100ms drag timer.
                mHandler.postDelayed(mStartDragRunnable, DRAG_START_DELAY_MS);
            } else if (selectionEnabled) {
                // Scenario 3: Row is unselected but there is another row that is selected. -> 500ms
                // drag timer (no selection toggle).
                mHandler.postDelayed(mStartDragRunnable, DEFAULT_DRAG_START_DELAY_MS);
            } else {
                // Scenario 1: No rows currently selected. -> 500ms select timer -> 100ms drag
                // timer.
                mHandler.postDelayed(mSelectRunnable, SELECTION_START_DELAY_MS);
            }
            // Consume event.
            return true;

        } else if (action == MotionEvent.ACTION_MOVE) {
            // Calculate the movement distance by subtracting the current finger pos from where it
            // was when the user first touched the screen (ACTION_DOWN).
            float deltaX = Math.abs(event.getRawX() - mHandleDownX);
            float deltaY = Math.abs(event.getRawY() - mHandleDownY);

            // If the user has moved their finger/mouse more than the safety buffer, they may be
            // trying to scroll. We cancel our runnables so that the item doesn't accidentally
            // get selected or enter dragging state.
            if (deltaX > mTouchSlop || deltaY > mTouchSlop) {
                cleanupTimers();
                // Release visual pressed state.
                v.setPressed(false);
            }
            // We don't return true here because if the user is trying to scroll, we want the event
            // to pass up to the parent (RecyclerView), which would see the move and scroll the
            // list. If we had returned true, we would have consumed the event and the parent
            // RecyclerView would not have been notified.
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            // Release visual pressed state.
            v.setPressed(false);
            cleanupTimers();

            // Only perform click if we didn't actually drag/select.
            if (!mIsLongPressTriggered && action == MotionEvent.ACTION_UP) {
                // If no timers fired, it's a click (open bookmark/folder).
                v.performClick();
            }

            // Always re-enable interception (unblock scrolling) on release as a safety measure.
            if (mRecyclerView != null) {
                mRecyclerView.requestDisallowInterceptTouchEvent(false);
            }
            // Reset.
            mIsLongPressTriggered = false;
            return true;
        }
        return false;
    }

    @Override
    public void onViewAttachedToWindow(View v) {
        // No-op.
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        // If the view is detached (recycled) while a timer is running, we must cancel everything to
        // prevent a drag starting on a view that is no longer on screen.
        cleanupTimers();
        // Reset the internal state since this view will be recycled.
        mIsLongPressTriggered = false;
        v.setPressed(false);
        // Remove listener to prevent the recycled view from accumulating old DragHelper objects.
        v.removeOnAttachStateChangeListener(this);
    }

    private void cleanupTimers() {
        mHandler.removeCallbacks(mSelectRunnable);
        mHandler.removeCallbacks(mStartDragRunnable);
    }

    private boolean isPointInsideView(View view, float x, float y) {
        return x >= view.getLeft()
                && x <= view.getRight()
                && y >= view.getTop()
                && y <= view.getBottom();
    }

    // Logic for Runnable 1: Selection (500ms). Runs when the user touches an unselected row body
    // and
    // holds for 500ms (and doesn't move enough to cancel it).
    private void selectItem() {
        mIsLongPressTriggered = true;
        if (mSelectionDelegate != null && mBookmarkId != null) {
            // This protects the 100ms gap after selection (500ms) but before drag start
            // (600ms). We call the parent (RecyclerView) to stop watching touches to
            // prevent accidental scrolling.
            if (mRecyclerView != null) {
                mRecyclerView.requestDisallowInterceptTouchEvent(true);
            }

            // Perform Action. Item is selected, updateView() is called.
            mSelectionDelegate.toggleSelectionForItem(mBookmarkId);
        }

        // Chain the second timer (100ms) for drag. Once this is called, ItemTouchHelper
        // steals the touch stream and our ImprovedBookmarkRow.onTouch stops receiving
        // events immediately.
        mHandler.postDelayed(mStartDragRunnable, DRAG_START_DELAY_MS);
    }

    // Logic for Runnable 2: Drag Start. Runs when A) The grab handle is touched or B) 100ms after
    // mSelectRunnable finishes.
    private void startDrag() {
        if (mViewHolder == null) return;
        // We previously blocked interception in the mSelectRunnable to stop
        // scrolling. We must unblock it now so ItemTouchHelper (attached to the
        // parent, which is the RecyclerView) can intercept the stream and handle
        // the drag.
        if (mRecyclerView != null) {
            mRecyclerView.requestDisallowInterceptTouchEvent(false);
        }

        // Set grabbing (closed hand) on ImprovedBookmarkRow.
        if (mContext != null) {
            mViewHolder.itemView.setPointerIcon(
                    PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRABBING));
        }

        // Set grabbing (closed hand) on ImprovedBookmarkRow.
        mViewHolder.itemView.setPointerIcon(
                PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRABBING));

        // Manually call startDrag, which we disabled in the adapter.
        mItemTouchHelper.startDrag(mViewHolder);
    }
}
