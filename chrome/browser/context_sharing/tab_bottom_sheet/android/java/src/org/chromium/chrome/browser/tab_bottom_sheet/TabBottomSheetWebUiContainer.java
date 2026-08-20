// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;
import android.content.Context;
import android.util.AttributeSet;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A custom FrameLayout that delegates touch events to an external handler. This is used to
 * arbitrate between scrolling content and dragging the bottom sheet.
 */
@NullMarked
public class TabBottomSheetWebUiContainer extends FrameLayout {
    /** Interface to handle touch events dispatched to this container. */
    public interface TouchHandler {
        /**
         * Handles a touch event.
         *
         * @param v The view.
         * @param event The motion event.
         * @return Whether the event was handled.
         */
        boolean handleTouchEvent(TabBottomSheetWebUiContainer v, MotionEvent event);
    }

    private @Nullable TouchHandler mTouchHandler;
    private boolean mIsDispatchingToHandler;
    private @Nullable DragAndDropPermissions mDragAndDropPermissions;

    public TabBottomSheetWebUiContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /** Sets the touch handler for this container. */
    public void setTouchHandler(@Nullable TouchHandler touchHandler) {
        mTouchHandler = touchHandler;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (!mIsDispatchingToHandler && mTouchHandler != null) {
            mIsDispatchingToHandler = true;
            try {
                if (mTouchHandler.handleTouchEvent(this, event)) {
                    return true;
                }
            } finally {
                mIsDispatchingToHandler = false;
            }
        }
        return super.dispatchTouchEvent(event);
    }

    @Override
    public boolean dispatchDragEvent(DragEvent event) {
        if (event.getAction() == DragEvent.ACTION_DRAG_STARTED) {
            releaseDragAndDropPermissions();
        } else if (event.getAction() == DragEvent.ACTION_DROP) {
            releaseDragAndDropPermissions();
            Activity activity = ContextUtils.activityFromContext(getContext());
            if (activity != null) {
                mDragAndDropPermissions = activity.requestDragAndDropPermissions(event);
            }
        }
        return super.dispatchDragEvent(event);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        releaseDragAndDropPermissions();
    }

    private void releaseDragAndDropPermissions() {
        if (mDragAndDropPermissions != null) {
            mDragAndDropPermissions.release();
            mDragAndDropPermissions = null;
        }
    }
}
