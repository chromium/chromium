// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import android.content.Context;
import android.util.AttributeSet;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver.OnGlobalFocusChangeListener;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.base.DeviceInput;

/**
 * A download row in the download tray. With a precision pointer attached, its action buttons show
 * only while the cursor or keyboard focus is inside the row, as on desktop ({@code
 * DownloadBubbleRowView::UpdateRowForHover}, {@code OnWillChangeFocus}), leaving their room to the
 * row's text when hidden. Without a precision pointer, its action buttons stay visible.
 */
@NullMarked
public class DownloadTrayItemView extends LinearLayout {
    private final OnGlobalFocusChangeListener mFocusListener = (_, _) -> updateActionsVisibility();
    private View mActions;
    private boolean mHasActions;
    private boolean mIsCursorInside;

    public DownloadTrayItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActions = findViewById(R.id.download_tray_item_actions);
    }

    /** Sets whether the row has action buttons to show. */
    void setHasActions(boolean hasActions) {
        mHasActions = hasActions;
        updateActionsVisibility();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        getViewTreeObserver().addOnGlobalFocusChangeListener(mFocusListener);
    }

    @Override
    protected void onDetachedFromWindow() {
        getViewTreeObserver().removeOnGlobalFocusChangeListener(mFocusListener);
        // No hover exit follows for a row removed from under the cursor, e.g. to be reused.
        mIsCursorInside = false;
        updateActionsVisibility();
        super.onDetachedFromWindow();
    }

    @Override
    public boolean dispatchHoverEvent(MotionEvent event) {
        // The row sees a hover before its buttons do, so one over a button counts too. Touch
        // exploration's hovers aren't a cursor's.
        if (!event.isFromSource(InputDevice.SOURCE_TOUCHSCREEN)) {
            setCursorInside(event.getActionMasked() != MotionEvent.ACTION_HOVER_EXIT);
        }
        return super.dispatchHoverEvent(event);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        // Android ends the hover just before a mouse press and sends none until its release, so
        // meanwhile the press says where the cursor is. On the row, the buttons show again in time
        // for it to land on one; dragged off it or taken away, e.g. to scroll the list, they hide.
        boolean isMouse = !event.isFromSource(InputDevice.SOURCE_TOUCHSCREEN);
        if (isMouse) {
            float x = event.getX();
            float y = event.getY();
            boolean isOnRow = x >= 0 && x < getWidth() && y >= 0 && y < getHeight();
            setCursorInside(isOnRow && event.getActionMasked() != MotionEvent.ACTION_CANCEL);
        }
        // The row claims a mouse press, to follow it even from where nothing is clickable.
        return super.dispatchTouchEvent(event) || isMouse;
    }

    private void setCursorInside(boolean isCursorInside) {
        if (mIsCursorInside == isCursorInside) return;
        mIsCursorInside = isCursorInside;
        updateActionsVisibility();
    }

    private void updateActionsVisibility() {
        boolean isShown =
                mHasActions
                        && (!DeviceInput.supportsPrecisionPointer()
                                || mIsCursorInside
                                || hasFocus());
        mActions.setVisibility(isShown ? VISIBLE : GONE);
    }
}
