// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.bookmarks.R;

/** View for the bookmark bar which provides users with bookmark access from top chrome. */
class BookmarkBar extends LinearLayout implements View.OnLayoutChangeListener {

    private Callback<Integer> mHeightChangeCallback;
    private ImageButton mOverflowButton;

    /**
     * Constructor that is called when inflating a bookmark bar from XML.
     *
     * @param context the context the bookmark bar is running in.
     * @param attrs the attributes of the XML tag that is inflating the bookmark bar.
     */
    public BookmarkBar(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        addOnLayoutChangeListener(this);
    }

    /** Destroys the bookmark bar. */
    public void destroy() {
        removeOnLayoutChangeListener(this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mOverflowButton = findViewById(R.id.bookmark_bar_overflow_button);
    }

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        if (mHeightChangeCallback != null) {
            final int oldHeight = oldBottom - oldTop;
            final int newHeight = bottom - top;
            if (newHeight != oldHeight) {
                mHeightChangeCallback.onResult(newHeight);
            }
        }
    }

    /**
     * Sets the callback to notify of bookmark bar height change events. Note that the callback will
     * be immediately notified of the current bookmark bar height.
     *
     * @param heightChangeCallback the callback to notify.
     */
    public void setHeightChangeCallback(@Nullable Callback<Integer> heightChangeCallback) {
        mHeightChangeCallback = heightChangeCallback;
        if (mHeightChangeCallback != null) {
            mHeightChangeCallback.onResult(getHeight());
        }
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
}
