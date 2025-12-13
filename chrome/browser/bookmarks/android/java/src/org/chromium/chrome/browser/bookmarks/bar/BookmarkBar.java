// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.ui.util.MotionEventUtils;

/** View for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
class BookmarkBar extends LinearLayout {

    private FrameLayout mOverflowButton;

    /**
     * Constructor that is called when inflating a bookmark bar from XML.
     *
     * @param context the context the bookmark bar is running in.
     * @param attrs the attributes of the XML tag that is inflating the bookmark bar.
     */
    public BookmarkBar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mOverflowButton = findViewById(R.id.bookmark_bar_overflow_button);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        // Prevent touch events from "falling through" to views below.
        return true;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (MotionEventUtils.isPointerEvent(event)) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_BUTTON_PRESS
                    || action == MotionEvent.ACTION_BUTTON_RELEASE
                    || action == MotionEvent.ACTION_SCROLL) {
                return true;
            }
        }
        return super.onGenericMotionEvent(event);
    }

    /**
     * Sets the callback to notify of bookmark bar overflow button click events.
     *
     * @param callback the callback to notify.
     */
    public void setOverflowButtonClickCallback(@Nullable Runnable callback) {
        mOverflowButton.setOnClickListener(callback != null ? (v) -> callback.run() : null);
    }

    /**
     * Sets the visibility for the bookmark bar overflow button.
     *
     * @param visibility the visibility for the overflow button.
     */
    public void setOverflowButtonVisibility(int visibility) {
        mOverflowButton.setVisibility(visibility);
    }

    /**
     * @return The overflow button view.
     */
    public FrameLayout getOverflowButton() {
        return mOverflowButton;
    }
}
