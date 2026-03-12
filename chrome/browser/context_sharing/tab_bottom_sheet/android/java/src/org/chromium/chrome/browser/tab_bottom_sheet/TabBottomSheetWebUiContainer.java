// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.FrameLayout;

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

    public TabBottomSheetWebUiContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /** Sets the touch handler for this container. */
    public void setTouchHandler(@Nullable TouchHandler touchHandler) {
        mTouchHandler = touchHandler;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (mTouchHandler != null && mTouchHandler.handleTouchEvent(this, event)) {
            return true;
        }
        return super.dispatchTouchEvent(event);
    }
}
