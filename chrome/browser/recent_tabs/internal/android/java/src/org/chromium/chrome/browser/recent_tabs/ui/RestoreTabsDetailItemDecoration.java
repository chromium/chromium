// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.recent_tabs.R;

/**
 * The item decoration used on the device and review tabs selection screens that
 * adds horizontal spacing between elements and chooses the correct item background
 * depending on the position of the item in the list.
 */
public class RestoreTabsDetailItemDecoration extends RecyclerView.ItemDecoration {
    private final int mVerticalSpacing;

    /**
     * Creates a RestoreTabsDetailItemDecoration.
     * @param verticalSpacing The spacing between items in pixels.
     */
    public RestoreTabsDetailItemDecoration(@Px int verticalSpacing) {
        mVerticalSpacing = verticalSpacing;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        // The first item does not get additional spacing.
        outRect.top = (parent.getChildAdapterPosition(view) != 0) ? mVerticalSpacing : 0;
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        for (int index = 0; index < parent.getChildCount(); ++index) {
            View child = parent.getChildAt(index);
            int positionInAdapter = parent.getChildAdapterPosition(child);
            child.setBackground(
                    AppCompatResources.getDrawable(
                            parent.getContext(),
                            getBackgroundDrawable(
                                    positionInAdapter, parent.getAdapter().getItemCount())));
        }
    }

    /**
     * Returns the appropriate item background based on the position of the item in the list.
     * The first item has strongly rounded upper corners, the middle item has weakly
     * rounded corners on the top and the bottom, and the last item has strongly rounded
     * bottom corners.
     * @param position The zero-indexed position in the adapter.
     * @param itemCount The number of items in the adapter.
     * @return The resource ID of the item background.
     */
    private static @DrawableRes int getBackgroundDrawable(int position, int itemCount) {
        if (itemCount == 1) {
            return R.drawable.restore_tabs_single_item_background;
        }

        if (position == 0) {
            return R.drawable.restore_tabs_detail_item_background_top;
        }

        if (position == itemCount - 1) {
            return R.drawable.restore_tabs_detail_item_background_bottom;
        }

        return R.drawable.restore_tabs_detail_item_background_middle;
    }
}
