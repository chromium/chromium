// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.tab_ui.R;

/**
 * Item decoration used to add vertical spacing between elements and add a background depending on
 * the position of the item in the list.
 */
@NullMarked
public class TabGroupListItemDecoration extends RecyclerView.ItemDecoration {
    private final @Px int mVerticalSpacing;
    private final @Px int mFirstItemTopMargin;
    private final Drawable mTopItemDrawable;
    private final Drawable mBottomItemDrawable;
    private final Drawable mSingleItemDrawable;
    private final Drawable mNormalItemDrawable;

    TabGroupListItemDecoration(Context context) {
        Resources res = context.getResources();
        mVerticalSpacing = res.getDimensionPixelSize(R.dimen.tab_group_list_vertical_item_gap);
        mFirstItemTopMargin =
                res.getDimensionPixelSize(R.dimen.tab_group_list_first_item_top_margin);
        mTopItemDrawable =
                AppCompatResources.getDrawable(
                        context, R.drawable.tab_group_list_top_item_background);
        mBottomItemDrawable =
                AppCompatResources.getDrawable(
                        context, R.drawable.tab_group_list_bottom_item_background);
        mSingleItemDrawable =
                AppCompatResources.getDrawable(
                        context, R.drawable.tab_group_list_single_item_background);
        mNormalItemDrawable =
                AppCompatResources.getDrawable(
                        context, R.drawable.tab_group_list_normal_item_background);
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.top =
                (parent.getChildAdapterPosition(view) != 0)
                        ? mVerticalSpacing
                        : mFirstItemTopMargin;
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        RecyclerView.Adapter adapter = parent.getAdapter();
        if (adapter == null) return;

        int messageCount = 0;
        int index = 0;

        // Skip over and count the messages.
        for (; index < parent.getChildCount(); index++) {
            View child = parent.getChildAt(index);
            if (child instanceof MessageCardView) {
                messageCount++;
                continue;
            }
            break;
        }

        int tabGroupCount = adapter.getItemCount() - messageCount;
        assert tabGroupCount >= 0;

        for (; index < parent.getChildCount(); index++) {
            View child = parent.getChildAt(index);
            // Messages should all come first.
            assert !(child instanceof MessageCardView);

            int position = parent.getChildAdapterPosition(child) - messageCount;
            child.setBackground(getBackgroundDrawable(position, tabGroupCount));
        }
    }

    private Drawable getBackgroundDrawable(int position, int tabGroupCount) {
        if (tabGroupCount == 1) {
            return mSingleItemDrawable;
        }
        if (position == 0) {
            return mTopItemDrawable;
        }
        if (position == tabGroupCount - 1) {
            return mBottomItemDrawable;
        }
        return mNormalItemDrawable;
    }
}
