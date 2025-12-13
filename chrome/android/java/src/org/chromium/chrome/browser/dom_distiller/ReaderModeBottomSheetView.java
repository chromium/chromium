// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/** The view for the reader mode bottom sheet. */
@NullMarked
public class ReaderModeBottomSheetView extends LinearLayout {
    private View mDragHandle;
    private View mTitle;

    /**
     * @param context The android context.
     * @param attrs The android attributes.
     */
    public ReaderModeBottomSheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mDragHandle = findViewById(R.id.drag_handle);
        mTitle = findViewById(R.id.title);
    }

    /**
     * @return The height of the bottom sheet when in 'peeking' state. This is the height of the
     *     visible part of the bottom sheet when it is collapsed, which includes the drag handle and
     *     the title.
     */
    public int getPeekHeight() {
        ViewGroup.MarginLayoutParams dragHandleMarginParams =
                (ViewGroup.MarginLayoutParams) mDragHandle.getLayoutParams();
        return dragHandleMarginParams.topMargin
                + mDragHandle.getHeight()
                + dragHandleMarginParams.bottomMargin
                + mTitle.getHeight();
    }
}
