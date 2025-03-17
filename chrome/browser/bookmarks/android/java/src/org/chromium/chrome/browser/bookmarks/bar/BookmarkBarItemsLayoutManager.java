// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.view.View.MeasureSpec.AT_MOST;
import static android.view.View.MeasureSpec.UNSPECIFIED;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.ui.base.LocalizationUtils;

import java.util.List;

/**
 * The layout manager used to render the horizontal list of items within the bookmark bar which
 * provides users with bookmark access from top chrome. Note that the layout manager is *not*
 * scrollable and will only render as many items as will fit completely within its viewport.
 */
class BookmarkBarItemsLayoutManager extends RecyclerView.LayoutManager {

    private final int mItemMaxWidth;
    private final int mItemSpacing;
    private final ObservableSupplierImpl<Boolean> mItemsOverflowSupplier;

    /**
     * Constructor.
     *
     * @param context the context in which to render items.
     */
    public BookmarkBarItemsLayoutManager(@NonNull Context context) {
        final Resources resources = context.getResources();
        mItemMaxWidth = resources.getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width);
        mItemSpacing = resources.getDimensionPixelSize(R.dimen.bookmark_bar_item_spacing);
        mItemsOverflowSupplier = new ObservableSupplierImpl<>(false);
    }

    @Override
    public RecyclerView.LayoutParams generateDefaultLayoutParams() {
        return new RecyclerView.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
    }

    /**
     * @return the supplier for the current state of items overflow.
     */
    public @NonNull ObservableSupplier<Boolean> getItemsOverflowSupplier() {
        return mItemsOverflowSupplier;
    }

    @Override
    public boolean isAutoMeasureEnabled() {
        return true;
    }

    @Override
    public void onLayoutChildren(
            @NonNull RecyclerView.Recycler recycler, @NonNull RecyclerView.State state) {
        detachAndScrapAttachedViews(recycler);

        final var visibleBounds = new Rect(0, 0, getWidth(), getHeight());

        for (int i = 0, startOffset = getStartOffset(); i < state.getItemCount(); i++) {
            final View itemView = recycler.getViewForPosition(i);

            addView(itemView);
            measureChild(itemView);
            startOffset = layoutChild(itemView, startOffset);

            final var itemViewBounds =
                    new Rect(
                            itemView.getLeft(),
                            itemView.getTop(),
                            itemView.getRight(),
                            itemView.getBottom());

            // NOTE: Lay out only as many items as will fit completely within the viewport.
            if (!visibleBounds.contains(itemViewBounds)) {
                detachAndScrapView(itemView, recycler);
                break;
            }
        }

        final List<RecyclerView.ViewHolder> scrapList = recycler.getScrapList();
        for (int i = scrapList.size() - 1; i >= 0; i--) {
            recycler.recycleView(scrapList.get(i).itemView);
        }
    }

    @Override
    public void onLayoutCompleted(@NonNull RecyclerView.State state) {
        super.onLayoutCompleted(state);

        // NOTE: Items overflow when there are more items in the adapter than are rendered.
        final boolean itemsOverflow = getChildCount() != state.getItemCount();
        mItemsOverflowSupplier.set(itemsOverflow);
    }

    private int getStartOffset() {
        return LocalizationUtils.isLayoutRtl() ? getWidth() : 0;
    }

    private int layoutChild(@NonNull View child, int startOffset) {
        final boolean isLayoutRtl = LocalizationUtils.isLayoutRtl();

        final int height = getDecoratedMeasuredHeight(child);
        final int width = getDecoratedMeasuredWidth(child);
        final int left = isLayoutRtl ? startOffset - width : startOffset;
        final int right = left + width;

        layoutDecorated(child, left, /* top= */ 0, right, /* bottom= */ height);

        final int layoutDirection = isLayoutRtl ? -1 : 1;
        return startOffset + layoutDirection * (width + mItemSpacing);
    }

    private void measureChild(@NonNull View child) {
        final var lp = child.getLayoutParams();

        // NOTE: Width must be constrained via both layout params and measure spec. Otherwise a
        // child which requests an exact width via layout params will *not* be properly constrained.
        final var width = Math.min(mItemMaxWidth, lp.width);
        final var widthMeasureSpec = MeasureSpec.makeMeasureSpec(mItemMaxWidth, AT_MOST);

        child.measure(
                ViewGroup.getChildMeasureSpec(widthMeasureSpec, 0, width),
                ViewGroup.getChildMeasureSpec(UNSPECIFIED, 0, lp.height));
    }
}
