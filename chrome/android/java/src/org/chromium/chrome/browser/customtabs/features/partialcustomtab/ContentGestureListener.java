// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabHandleStrategy.FLING_VELOCITY_PIXELS_PER_MS;

import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.VelocityTracker;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabHandleStrategy.DragEventCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;

/**
 * Class responsible for detecting swipe and scroll events on the partial custom tab's content view,
 * and setting the event target appropriately to the tab window or the content view.
 */
class ContentGestureListener extends GestureDetector.SimpleOnGestureListener {
    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    private static final long BASE_ANIMATION_DURATION_MS = 218;

    static final float MIN_VERTICAL_SCROLL_SLOPE = 2.0f;

    /** The targets that can handle MotionEvents. */
    @IntDef({GestureState.NONE, GestureState.DRAG_TAB, GestureState.SCROLL_CONTENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface GestureState {
        int NONE = 0;
        int DRAG_TAB = 1;
        int SCROLL_CONTENT = 2;
    }

    private @GestureState int mState;

    private VelocityTracker mVelocityTracker;
    private DragEventCallback mCallback;
    private Supplier<Tab> mTab;
    private BooleanSupplier mIsFullyExpanded;
    private int mPrevRawY;

    /**
     * Constructor.
     *
     * @param tab Supplier of the current custom tab.
     * @param callback Callback invoked at each tab resizing phase (start/move/end).
     * @param isFullyExpanded Supplier of the flag whether the tab is in fully expanded state.
     */
    public ContentGestureListener(
            Supplier<Tab> tab, DragEventCallback callback, BooleanSupplier isFullyExpanded) {
        mTab = tab;
        mCallback = callback;
        mIsFullyExpanded = isFullyExpanded;
        mVelocityTracker = VelocityTracker.obtain();
        mState = GestureState.NONE;
    }

    /** Returns the current {@link GestureState} */
    public @GestureState int getState() {
        return mState;
    }

    /** Perform non-fling release. */
    public void doNonFlingRelease() {
        mVelocityTracker.computeCurrentVelocity(FLING_VELOCITY_PIXELS_PER_MS);
        mCallback.onDragEnd(getFlingDistance(mVelocityTracker.getYVelocity()));
        mState = GestureState.NONE;
    }

    @Override
    public boolean onDown(MotionEvent e) {
        mState = GestureState.NONE;
        return e != null;
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        if (e1 == null) return false;

        // Mutable local flags for readability. Needs updating when |mState| changes.
        boolean isScrollingContent = mState == GestureState.SCROLL_CONTENT;
        boolean isMovingTab = mState == GestureState.DRAG_TAB;

        final boolean contentReachedTop = isContentScrolledToTop();
        final boolean draggingUp = distanceY > 0; // == scrolling down
        final boolean draggingDown = distanceY < 0; // == scrolling up

        // Content scrolling should stop when the content reaches the top while dragging down.
        // Reset the state as we may need to switch to the tab dragging instead.
        if (isScrollingContent && draggingDown && contentReachedTop) {
            mState = GestureState.NONE;
            isScrollingContent = false;
            isMovingTab = false;
        }

        // Stop if the scroll is not vertical, except when the tab was already being dragged.
        float slope =
                Math.abs(distanceX) > 0f
                        ? Math.abs(distanceY) / Math.abs(distanceX)
                        : MIN_VERTICAL_SCROLL_SLOPE;
        if (!isMovingTab && slope < MIN_VERTICAL_SCROLL_SLOPE) {
            mVelocityTracker.clear();
            return false;
        }

        mVelocityTracker.addMovement(e2);

        if (mIsFullyExpanded.getAsBoolean()) {
            // 1) Sheet fully expands but keep dragging up -> switch to SCROLL_CONTENT.
            // 2) Start dragging up/down at expanded state -> state gets set to DRAG_TAB first,
            //    and then immediately switched to SCROLL_CONTENT here.
            if ((draggingUp || !contentReachedTop) && isMovingTab) startContentScrolling(e2);
        } else if (draggingDown && mState == GestureState.NONE) {
            // Drag down at peeked state -> SCROLL_CONTENT, but unlike in |startContentScrolling|,
            // no need to inject an additional ACTION_DOWN in this case.
            mState = GestureState.SCROLL_CONTENT;
            // State changed. Update |isScrollingContent/isMovingTab| if they're to be used below.
        }

        switch (mState) {
            case GestureState.NONE:
                startTabDragging(e2); // Start dragging tab if not set to scroll content above.
                break;
            case GestureState.DRAG_TAB:
                mCallback.onDragMove((int) e2.getRawY());
                break;
            case GestureState.SCROLL_CONTENT:
                // Events from now on are routed to content view for scroll in
                // BottomSheetStrategy#onTouchEvent.
                break;
        }
        return true;
    }

    private void startTabDragging(MotionEvent e) {
        mCallback.onDragStart((int) e.getRawY());
        mState = GestureState.DRAG_TAB;
    }

    private void startContentScrolling(MotionEvent e) {
        // Inject an ACTION_DOWN to content view to make it initiate the content scroll.
        MotionEvent down = MotionEvent.obtain(e);
        down.setAction(MotionEvent.ACTION_DOWN);
        mState = GestureState.SCROLL_CONTENT;
        mTab.get().getContentView().onTouchEvent(down);
    }

    private boolean isContentScrolledToTop() {
        WebContents webContents = mTab.get().getWebContents();
        return RenderCoordinates.fromWebContents(webContents).getScrollYPixInt() == 0;
    }

    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
        if (e1 == null || mState != GestureState.DRAG_TAB) return false;
        mCallback.onDragEnd(getFlingDistance(velocityY));
        mState = GestureState.NONE;
        return true;
    }

    @Override
    public boolean onSingleTapUp(MotionEvent e) {
        return false; // Let the content view consume single taps.
    }

    private int getFlingDistance(float velocity) {
        // This includes conversion from seconds to ms.
        return (int) (velocity * BASE_ANIMATION_DURATION_MS / 2000f);
    }

    int getStateForTesting() {
        return mState;
    }
}
