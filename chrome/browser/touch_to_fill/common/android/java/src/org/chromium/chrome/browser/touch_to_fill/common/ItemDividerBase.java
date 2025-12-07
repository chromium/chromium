// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** This is an item decorator for lists of items displayed in Touch to Fill BottomSheets. */
@NullMarked
public abstract class ItemDividerBase extends RecyclerView.ItemDecoration {
    protected final Context mContext;

    private void loadBackgroundDrawable(@Nullable View view, @DrawableRes int backgroundId) {
        if (view == null) {
            // RecyclerView might return a {@code null} view if it's not displayed to the user.
            return;
        }
        GradientDrawable background =
                (GradientDrawable) AppCompatResources.getDrawable(mContext, backgroundId);
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
        assumeNonNull(parent.getAdapter());
        int firstItem = 0;
        int lastItem = parent.getAdapter().getItemCount() - 1;
        // Find the first item to decorate by skipping all header items.
        while (firstItem <= lastItem
                && shouldSkipItemType(parent.getAdapter().getItemViewType(firstItem))) {
            ++firstItem;
        }
        // Find the last item to decorate by skipping items like footer, fill button, additional
        // info, etc.
        while (firstItem <= lastItem
                && shouldSkipItemType(parent.getAdapter().getItemViewType(lastItem))) {
            --lastItem;
        }
        if (firstItem > lastItem) {
            // No items to decorate.
            return;
        }
        if (firstItem == lastItem) {
            // There's only 1 item to decorate - round all corners.
            loadBackgroundDrawable(
                    parent.getChildAt(firstItem),
                    R.drawable.touch_to_fill_credential_background_modern_rounded_all);
            return;
        }
        loadBackgroundDrawable(
                parent.getChildAt(firstItem),
                R.drawable.touch_to_fill_credential_background_modern_rounded_up);
        loadBackgroundDrawable(
                parent.getChildAt(lastItem),
                R.drawable.touch_to_fill_credential_background_modern_rounded_down);
        for (int viewPosition = firstItem + 1; viewPosition < lastItem; viewPosition++) {
            loadBackgroundDrawable(
                    parent.getChildAt(viewPosition),
                    R.drawable.touch_to_fill_credential_background_modern);
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
     *
     * @param type A type of an item in the list on the {@link BottomSheet}.
     * @return True if the item of the said type should be undecorated.
     */
    protected abstract boolean shouldSkipItemType(int type);
}
