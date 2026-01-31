// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
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
    private static final int HIDE_HANDLE_DELAY_MS = 50;

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
    // Tracks if the drag handle is currently being pressed (via touch or mouse down).
    private boolean mIsHandleTouched;

    // Tracks if a drag interaction was just successfully initiated. Used to prevent the handle from
    // hiding when the system (the ItemTouchHelper working with RecyclerView) sends ACTION_CANCEL to
    // the grab handle’s onTouch listener on drag start.
    private boolean mDragInteractionActive;

    private final Runnable mSelectRunnable = this::selectItem;
    private final Runnable mStartDragRunnable = this::startDrag;
    private final Runnable mHideHandleRunnable = this::hideDragHandle;

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

    /**
     * Handles touch events for the Drag Handle. Bound to {@link
     * ImprovedBookmarkRowProperties#DRAG_HANDLE_TOUCH_LISTENER}.
     *
     * @param v The view the touch event has been dispatched to (The Drag Handle).
     * @param event The MotionEvent object containing full information about the event.
     * @return True if the listener has consumed the event, false otherwise.
     */
    public boolean onDragHandleTouch(View v, MotionEvent event) {
        if (!mIsDragEnabled) return false;

        int action = event.getActionMasked();
        boolean isMouse = event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE;

        if (action == MotionEvent.ACTION_DOWN) {
            mHandleDownX = event.getRawX();
            mHandleDownY = event.getRawY();

            // While dragging, the recycler view should ignore other movements to
            // prevent accidental scrolling.
            if (mRecyclerView != null) {
                mRecyclerView.requestDisallowInterceptTouchEvent(true);
            }

            // Ensure the handle is visible.
            mIsHandleTouched = true;
            v.setVisibility(View.VISIBLE);

            if (isMouse) {
                // Closed hand (grabbing) when mouse down over grab handle.
                PointerIcon grabbing =
                        PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRABBING);
                v.setPointerIcon(grabbing);

            } else {
                // Almost-instant (100ms to distinguish tap vs long-press) drag.
                mHandler.postDelayed(mStartDragRunnable, DRAG_START_DELAY_MS);
            }
            // Ensure the visual state updates (pressed state).
            v.setPressed(true);
            return true;

        } else if (action == MotionEvent.ACTION_MOVE) {
            // This else-if block is only for mouse down on grab handle. This is a
            // unique case where we want the dragging UI to only appear on movement to
            // avoid the dragging UI flashing when the user clicks on the grab handle.
            if (!isMouse || !mIsHandleTouched) return true;

            float deltaX = Math.abs(event.getRawX() - mHandleDownX);
            float deltaY = Math.abs(event.getRawY() - mHandleDownY);

            // Check if the mouse has moved far enough to count as a "drag".
            if (deltaX > mTouchSlop || deltaY > mTouchSlop) {
                // Main command.
                startDrag();

                // Dispatch a fake ACTION_MOVE to the parent RecyclerView. This is because when
                // startDrag() is called, the ItemTouchHelper attached to the RecyclerView
                // intercepts the touch stream. This causes the framework to automatically send an
                // ACTION_CANCEL to this child view (the drag handle). The drag handle's onTouch
                // listener responds to ACTION_CANCEL by resetting the cursor to an "open hand"
                // (thus reversing our "closed hand" on drag). This fake event forces the
                // RecyclerView to immediately process a move event while dragging is active,
                // overriding that reset and ensuring the "closed hand" cursor persists correctly
                // during the drag.
                long now = SystemClock.uptimeMillis();
                MotionEvent fakeMove =
                        MotionEvent.obtain(
                                now,
                                now,
                                MotionEvent.ACTION_MOVE,
                                event.getRawX(),
                                event.getRawY(),
                                0);

                if (mRecyclerView != null) {
                    mRecyclerView.dispatchTouchEvent(fakeMove);
                }
                fakeMove.recycle();

                // Reset flag since dragging has started.
                mIsHandleTouched = false;
                v.setPressed(false);
            }
            return true;

        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            // If the user has let go too early (before 100ms).
            cleanupTimers();
            v.setPressed(false);
            mIsHandleTouched = false;

            if (isMouse) {
                // Revert the Handle's icon from closed hand to open hand.
                PointerIcon grab = PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRAB);
                v.setPointerIcon(grab);
            }

            boolean isSelected = mSelectionDelegate.isItemSelected(mBookmarkId);

            // If mDragInteractionActive = true, it means this ACTION_CANCEL signal came from the
            // ItemTouchHelper taking over when the drag started, so we want the handle to stay
            // visible.
            if (!isSelected && !mDragInteractionActive) {
                mHandler.postDelayed(mHideHandleRunnable, HIDE_HANDLE_DELAY_MS);
            }

            // Reset the flag for the next interaction.
            mDragInteractionActive = false;

            if (mRecyclerView != null) {
                mRecyclerView.requestDisallowInterceptTouchEvent(false);
            }
            return true;
        }
        return false;
    }

    /**
     * Handles hover events to manage the visibility of the grab handle. Shows the handle when the
     * pointer enters the row and hides it after small delay upon exit. Returns false to allow the
     * default hover visual state (e.g. background highlight) to render.
     *
     * @param v The view receiving the hover event (ImprovedBookmarkRow).
     * @param event The MotionEvent describing the hover action.
     * @return False to allow the event to propagate to default handlers (for visual feedback).
     */
    public boolean onRowBodyHover(View v, MotionEvent event) {
        // The mobile bookmarks folder, bookmarks bar folder, etc. are not draggable.
        if (!mIsDragEnabled) return false;

        int action = event.getActionMasked();
        View dragHandle = mViewHolder.itemView.findViewById(R.id.drag_handle);
        if (dragHandle == null) return false;

        // The mouse is inside the row boundaries.
        if (action == MotionEvent.ACTION_HOVER_ENTER || action == MotionEvent.ACTION_HOVER_MOVE) {
            // The handle should always be visible.
            mHandler.removeCallbacks(mHideHandleRunnable);
            dragHandle.setVisibility(View.VISIBLE);

            // Return false to let the event "bubble up" to the View's default handler, which shows
            // the grey highlight background on hover.
            return false;

        } else if (action == MotionEvent.ACTION_HOVER_EXIT) {
            // The mouse has now left the row boundaries.
            boolean isSelected = mSelectionDelegate.isItemSelected(mBookmarkId);

            // If selected, the grab handle should always visible. If not, hide the handle.
            if (!isSelected) {
                // 50ms delay to prevent flickering.
                mHandler.postDelayed(mHideHandleRunnable, HIDE_HANDLE_DELAY_MS);
            }
            // This allows the system to remove the grey background highlight on hover.
            return false;
        }
        return false;
    }

    /** Handles hover events for the Drag Handle itself. */
    public boolean onDragHandleHover(View v, MotionEvent event) {
        if (!mIsDragEnabled) return false;

        // Set the cursor to be an open hand.
        v.setPointerIcon(PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRAB));

        // Call the ImprovedBookmarkRow onHoverListener, which shows the handle on
        // hover. This line is needed because it avoids flickering by forcing the parent
        // (Row) to cancel the hide handle logic.
        onRowBodyHover(mViewHolder.itemView, event);

        // This tells the system to allow the Handle's default behavior to run.
        // The default behavior is what draws the circular grey background on the
        // handle on hover.
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
        mHandler.removeCallbacks(mHideHandleRunnable);

        // When the view (ImprovedBookmarkRow) is detached, it will be recycled, so we need to reset
        // v.setPressed, v.setVisibility, etc. But the BookmarkManagerDragHelper object gets garbage
        // collected and a new helper is created every time coordinator#bindDragProperties is
        // called. We therefore don't need to reset internal variables inside this class.
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
        // We previously blocked interception in the mSelectRunnable to stop
        // scrolling. We must unblock it now so ItemTouchHelper (attached to the
        // parent, which is the RecyclerView) can intercept the stream and handle
        // the drag.
        if (mRecyclerView != null) {
            mRecyclerView.requestDisallowInterceptTouchEvent(false);
        }

        // Set grabbing (closed hand) on ImprovedBookmarkRow.
        mViewHolder.itemView.setPointerIcon(
                PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRABBING));

        mDragInteractionActive = true;

        // Manually call startDrag, which we disabled in the adapter.
        mItemTouchHelper.startDrag(mViewHolder);
    }

    // Logic for Runnable 3: Hides the handle. Runs when the user moves the mouse out of the row
    // (HOVER_EXIT).
    private void hideDragHandle() {
        View dragHandle = mViewHolder.itemView.findViewById(R.id.drag_handle);
        if (dragHandle == null) return;

        boolean isSelected = mSelectionDelegate.isItemSelected(mBookmarkId);

        // If the item is selected, the handle is always visible. mIsHandleTouched is
        // needed to keep the handle visible when mouse down.
        if (!isSelected && !mIsHandleTouched) {
            dragHandle.setVisibility(View.GONE);
        }
    }
}
