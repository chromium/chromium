// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.util.Pair;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkRow;

/**
 * Represents a folder row in bookmark bottom sheet.
 */
class BookmarkBottomSheetFolderRow extends BookmarkRow {
    private Runnable mOnClickListener;
    private @ColorRes int mIconColor = R.color.default_icon_color_tint_list;

    /**
     * Constructor for inflating from XML.
     */
    public BookmarkBottomSheetFolderRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    void setTitle(@NonNull CharSequence title) {
        mTitleView.setText(title);
    }

    void setSubtitle(@Nullable CharSequence subtitle) {
        mDescriptionView.setText(subtitle == null ? "" : subtitle);
    }

    void setIcon(Pair<Drawable, Integer> drawableAndColor) {
        mIconColor = drawableAndColor.second;
        setStartIconDrawable(drawableAndColor.first);
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

    @Override
    protected ColorStateList getDefaultStartIconTint() {
        return AppCompatResources.getColorStateList(getContext(), mIconColor);
    }
}
