// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.bookmarks.BookmarkRow;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;

/**
 * Represents a folder row in bookmark bottom sheet.
 */
class BookmarkBottomSheetFolderRow extends BookmarkRow {
    private Runnable mOnClickListener;

    /**
     * Constructor for inflating from XML.
     */
    public BookmarkBottomSheetFolderRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    void setTitle(@NonNull String title) {
        mTitleView.setText(title);
    }

    void setSubtitle(@Nullable String subtitle) {
        mDescriptionView.setText(subtitle == null ? "" : subtitle);
    }

    void setOnClickListener(@NonNull Runnable listener) {
        mOnClickListener = listener;
    }

    // SelectableItemViewBase overrides.
    @Override
    public void onClick() {}

    // View overrides.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMoreIcon.setVisibility(GONE);

        // TODO(xingliu): Load the correct icon.
        setStartIconDrawable(BookmarkUtils.getFolderIcon(getContext()));
    }

    // BookmarkRow overrides.
    @Override
    public void onClick(View view) {
        assert mOnClickListener != null;
        mOnClickListener.run();
    }

    @Override
    public boolean onLongClick(View view) {
        return false;
    }
}
