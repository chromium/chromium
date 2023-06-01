// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;

/**
 * Common logic for displaying a view to represent a bookmark folder, and allowing the user to
 * either:
 * 1. Launch an activity to select a new folder.
 * 2. Within that activity select a folder or subfolder.
 */
public class ImprovedBookmarkFolderSelectRow extends FrameLayout {
    private View mRowView;
    private ImprovedBookmarkFolderView mFolderView;
    private TextView mTitleView;
    private View mEndIconView;

    /** Constructor for inflating from XML. */
    public ImprovedBookmarkFolderSelectRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mRowView = findViewById(R.id.improved_folder_row);
        mFolderView = (ImprovedBookmarkFolderView) findViewById(R.id.image);
        mTitleView = (TextView) findViewById(R.id.title);
        mEndIconView = findViewById(R.id.end_icon);
    }

    void setTitle(String title) {
        mTitleView.setText(title);
    }

    void setStartIconDrawable(Drawable drawable) {
        mFolderView.setStartIconDrawable(drawable);
    }

    void setStartIconTint(ColorStateList tint) {
        mFolderView.setStartIconTint(tint);
    }

    void setStartAreaBackgroundColor(@ColorInt int color) {
        mFolderView.setStartAreaBackgroundColor(color);
    }

    void setStartImageDrawables(
            @Nullable Drawable primaryDrawable, @Nullable Drawable secondaryDrawable) {
        mFolderView.setStartImageDrawables(primaryDrawable, secondaryDrawable);
    }

    void setChildCount(int count) {
        mFolderView.setChildCount(count);
    }

    void setEndIconVisible(boolean visible) {
        mEndIconView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setRowClickListener(View.OnClickListener listener) {
        mRowView.setOnClickListener(listener);
    }
}
