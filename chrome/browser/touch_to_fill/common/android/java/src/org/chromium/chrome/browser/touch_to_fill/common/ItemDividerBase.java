// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

/** This is an item decorator for lists of items displayed in Touch to Fill BottomSheets. */
public abstract class ItemDividerBase extends RecyclerView.ItemDecoration {
    protected final Context mContext;

    /**
     * Returns the proper background for each of the credential items depending on their position.
     *
     * @param position Position of the credential inside the list, including the header and the
     *     button.
     * @param containsFillButton Indicates if the fill button is in the list.
     * @param itemCount Shows how many items are in the list, including the header and the button.
     * @return The ID of the selected background resource.
     */
    protected int selectBackgroundDrawable(
            int position, boolean containsFillButton, int itemCount) {
        if (containsFillButton) { // Round all the corners of the only item.
            return R.drawable.touch_to_fill_credential_background_modern_rounded_all;
        }
        if (position == itemCount - 1) { // Round the bottom of the last suggestion item.
            return R.drawable.touch_to_fill_credential_background_modern_rounded_down;
        }
        if (position == 1) { // Round the top of the first item.
            return R.drawable.touch_to_fill_credential_background_modern_rounded_up;
        }
        // The rest of the items have a background with no rounded edges.
        return R.drawable.touch_to_fill_credential_background_modern;
    }

    private void loadBackgroundDrawable(
            View view, int position, boolean containsFillButton, int itemCount) {
        GradientDrawable background =
                (GradientDrawable)
                        AppCompatResources.getDrawable(
                                mContext,
                                selectBackgroundDrawable(position, containsFillButton, itemCount));
        TouchToFillUtil.addColorAndRippleToBackground(view, background, mContext);
    }

    /**
     * Draws the decorations into the Canvas supplied to the RecyclerView.
     *
     * @param canvas The Canvas to draw into.
     * @param parent The RecyclerView this ItemDecoration is drawing into.
     * @param state The current state of RecyclerView.
     */
    @Override
    public void onDraw(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
        int itemCount = parent.getAdapter().getItemCount();
        // Do not count items at the end like footer, fill button, additional info, etc.
        while (itemCount > 1
                && shouldSkipItemType(parent.getAdapter().getItemViewType(itemCount - 1))) {
            itemCount--;
        }
        for (int posInView = 0; posInView < parent.getChildCount(); posInView++) {
            View child = parent.getChildAt(posInView);
            int posInAdapter = parent.getChildAdapterPosition(child);
            if (shouldSkipItemType(parent.getAdapter().getItemViewType(posInAdapter))) continue;
            loadBackgroundDrawable(child, posInAdapter, containsFillButton(parent), itemCount);
        }
    }

    /**
     * Creates an instance of ItemDividerBase.
     *
     * @param context Is used to get the drawable resources for the item backgrounds.
     */
    protected ItemDividerBase(Context context) {
        this.mContext = context;
    }

    /**
     * Used as helper to determine undecorated items like headers and buttons.
     * @param type A type of an item in the list on the {@link BottomSheet}.
     * @return True if the item of the said type should be undecorated.
     */
    protected abstract boolean shouldSkipItemType(int type);

    /**
     * Used as a helper to determine the appropriate background shape for the decorated items in the
     * list.
     * @param parent The {@link RecyclerView} containing the items on the {@link BottomSheet}.
     * @return True if the last item in the list is a button.
     */
    protected abstract boolean containsFillButton(RecyclerView parent);
}
