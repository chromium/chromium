// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;

/** A LinearLayout that can notify when its size changes. */
public class SizeListenableLinearLayout extends LinearLayout {
    @Nullable
    private BottomSheetContent.ContentSizeListener mListener;

    public SizeListenableLinearLayout(Context context) {
        super(context);
    }

    public SizeListenableLinearLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SizeListenableLinearLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (mListener != null) {
            // TODO(crbug.com/806868): In some edge cases, #onLayout is called and #onSizeChanged is
            // not. This call with invalid values ensures that the BottomSheet is resized correctly
            // when that happens. A correct fix is to make the BottomSheet always listen for layout
            // changes (it currently does that only if there is no ContentSizeListener attached to
            // the BottomSheetContent).
            mListener.onSizeChanged(-1, bottom - top, -1, -1);
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (mListener != null) {
            mListener.onSizeChanged(w, h, oldw, oldh);
        }
    }

    void setContentSizeListener(@Nullable BottomSheetContent.ContentSizeListener listener) {
        mListener = listener;
    }
}
