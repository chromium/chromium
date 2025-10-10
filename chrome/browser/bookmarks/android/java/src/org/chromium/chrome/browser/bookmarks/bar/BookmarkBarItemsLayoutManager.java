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

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.LocalizationUtils;

import java.util.List;

/**
 * The layout manager used to render the horizontal list of items within the bookmark bar which
 * provides users with bookmark access from top chrome. Note that the layout manager is *not*
 * scrollable and will only render as many items as will fit completely within its viewport.
 */
@NullMarked
class BookmarkBarItemsLayoutManager extends RecyclerView.LayoutManager {

    private static int @Nullable [] sDesiredWidthsForTesting;

    private final int mItemSpacing;
    private final ObservableSupplierImpl<Boolean> mItemsOverflowSupplier;

    private int mItemMinWidth;
    private int mItemMaxWidth;
    private int mEffectiveItemWidth;

    /**
     * Constructor.
     *
     * @param context The context in which to render items.
     */
    public BookmarkBarItemsLayoutManager(Context context) {
        final Resources resources = context.getResources();
        mItemMaxWidth = resources.getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width);
        mItemMinWidth = resources.getDimensionPixelSize(R.dimen.bookmark_bar_item_min_width);
        mItemSpacing = resources.getDimensionPixelSize(R.dimen.bookmark_bar_item_spacing);
        mItemsOverflowSupplier = new ObservableSupplierImpl<>(false);
    }

    @Override
    public RecyclerView.LayoutParams generateDefaultLayoutParams() {
        return new RecyclerView.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
    }

    /**
     * @return The supplier for the current state of items overflow.
     */
    public ObservableSupplier<Boolean> getItemsOverflowSupplier() {
        return mItemsOverflowSupplier;
    }

    @Override
    public boolean isAutoMeasureEnabled() {
        return true;
    }

    @Override
    public void onLayoutChildren(RecyclerView.Recycler recycler, RecyclerView.State state) {
        detachAndScrapAttachedViews(recycler);

        // When the fast-follow feature is enabled, we will use the dynamic width calculation.
        if (ChromeFeatureList.sAndroidBookmarkBarFastFollow.isEnabled()) {
            mEffectiveItemWidth = calculateOptimalItemWidth(recycler, state.getItemCount());
        }

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
    public void onLayoutCompleted(RecyclerView.State state) {
        super.onLayoutCompleted(state);

        // NOTE: Items overflow when there are more items in the adapter than are rendered.
        final boolean itemsOverflow = getChildCount() != state.getItemCount();
        mItemsOverflowSupplier.set(itemsOverflow);
    }

    /**
     * Sets the min/max width constraints for bookmark bar items. Note that the new constraint will
     * not take effect until the next layout pass.
     *
     * @param itemMinWidth The min width constraint.
     * @param itemMaxWidth The max width constraint.
     */
    public void setItemWidthConstraints(int itemMinWidth, int itemMaxWidth) {
        mItemMinWidth = itemMinWidth;
        mItemMaxWidth = itemMaxWidth;
    }

    /**
     * @return The adapter position of the first item that is not visible, or the total item count
     *     if all items are visible.
     */
    public int getFirstHiddenItemPosition() {
        return getChildCount();
    }

    private int getStartOffset() {
        return LocalizationUtils.isLayoutRtl() ? getWidth() : 0;
    }

    private int layoutChild(View child, int startOffset) {
        final boolean isLayoutRtl = LocalizationUtils.isLayoutRtl();

        final int height = getDecoratedMeasuredHeight(child);
        final int width = getDecoratedMeasuredWidth(child);
        final int left = isLayoutRtl ? startOffset - width : startOffset;
        final int right = left + width;

        layoutDecorated(child, left, /* top= */ 0, right, /* bottom= */ height);

        final int layoutDirection = isLayoutRtl ? -1 : 1;
        return startOffset + layoutDirection * (width + mItemSpacing);
    }

    private void measureChild(View child) {
        // NOTE: Max width constraint must be set before measure/layout.
        assert mItemMaxWidth > 0;

        final var lp = child.getLayoutParams();

        // NOTE: Width must be constrained via both layout params and measure spec. Otherwise a
        // child which requests an exact width via layout params will *not* be properly constrained.

        // When the fast-follow feature is enabled, we will use the dynamic width calculation.
        final var width =
                ChromeFeatureList.sAndroidBookmarkBarFastFollow.isEnabled()
                        ? Math.min(mEffectiveItemWidth, lp.width)
                        : Math.min(mItemMaxWidth, lp.width);
        final var widthMeasureSpec =
                ChromeFeatureList.sAndroidBookmarkBarFastFollow.isEnabled()
                        ? MeasureSpec.makeMeasureSpec(mEffectiveItemWidth, AT_MOST)
                        : MeasureSpec.makeMeasureSpec(mItemMaxWidth, AT_MOST);

        child.measure(
                ViewGroup.getChildMeasureSpec(widthMeasureSpec, 0, width),
                ViewGroup.getChildMeasureSpec(UNSPECIFIED, 0, lp.height));
    }

    /**
     * This method calculates the optimal item width for the bookmark bar. The bookmark bar items
     * have both a minimum and maximum width limit. We want to fit as many items as we can on the
     * visible bar within that range, but we also don't want any white space between the last item
     * and the overflow button.
     *
     * <p>This method applies the following rules in order:
     *
     * <p>1. If the bookmarks can all fit at their maximum width, use that width.
     *
     * <p>2. If the bookmarks cannot all fit at their maximum width, but they can all fit at their
     * minimum width, then there must be some width between these two that fills the bar exactly.
     * Use a binary search to find this width.
     *
     * <p>3. If the bookmarks cannot all fit at their minimum width, then we need the overflow. Put
     * as many bookmarks as necessary into the overflow so that the remaining items fit with their
     * minimum width amount, then, do a binary search to find the optimal width for the remaining
     * items.
     *
     * <p>To clarify on step 3 - When we need an overflow, the remaining items on the screen may not
     * be the minimum width exactly (e.g. if the screen is a non-integer multiple of minimum width;
     * or some items have smaller names than the minimum width). In a case like this, it is possible
     * to go from having all the bookmarks on screen with a minimum width, to having one item in the
     * overflow with the visible items having a greater width then before. For example, if all items
     * fit on screen and one item is edited to a longer length, the other items will have slightly
     * more space to expand to when the last item is pushed to the overflow.
     *
     * @param recycler Recycler for the RecyclerView of |this| to recycle views after measuring.
     * @param itemCount The total number of items available for the bookmark bar.
     * @return Returns the optimal width for views in the bookmark bar visible area.
     */
    private int calculateOptimalItemWidth(RecyclerView.Recycler recycler, int itemCount) {
        if (itemCount == 0) {
            return mItemMaxWidth;
        }

        // Measure the desired width for each item (which varies because not all items are long
        // enough that they overflow and need an ellipsis). Then recycle the views.
        int[] desiredWidths = new int[itemCount];
        for (int i = 0; i < itemCount; i++) {
            View v = recycler.getViewForPosition(i);
            v.measure(
                    MeasureSpec.makeMeasureSpec(0, UNSPECIFIED),
                    MeasureSpec.makeMeasureSpec(0, UNSPECIFIED));
            desiredWidths[i] = getDecoratedMeasuredWidth(v);
            recycler.recycleView(v);
        }

        // When testing, we override the desiredWidths because they always return 0 in tests.
        if (sDesiredWidthsForTesting != null && sDesiredWidthsForTesting.length > 0) {
            desiredWidths = sDesiredWidthsForTesting;
        }

        // Calculate the total width of items if limited to the max width (set in dimens.xml). If
        // all the items fit on the screen, return the max width.
        int totalWidthAtMax = 0;
        for (int w : desiredWidths) {
            totalWidthAtMax += Math.min(w, mItemMaxWidth);
        }
        totalWidthAtMax += Math.max(0, itemCount - 1) * mItemSpacing;
        if (totalWidthAtMax <= getWidth()) {
            return mItemMaxWidth;
        }

        // Calculate the total width of items if limited to the min width (set in dimens.xml). If
        // all the items fit on the screen, then there must be some value v, such that
        // minWidth <= v <= maxWidth, that is an optimal width. Find with a binary search.
        int totalWidthAtMin = 0;
        for (int w : desiredWidths) {
            totalWidthAtMin += Math.min(w, mItemMinWidth);
        }
        totalWidthAtMin += Math.max(0, itemCount - 1) * mItemSpacing;
        if (totalWidthAtMin <= getWidth()) {
            return findBestItemWidth(itemCount, desiredWidths);
        }

        // There is no way to fit everything on the screen, so use the overflow menu. Find the
        // maximum number of items that can fit on the visible bar, and then find their optimal
        // width using a binary search.
        int numItemsToDisplay = 0;
        int widthSum = 0;
        for (int i = 0; i < desiredWidths.length; i++) {
            int itemWidth = Math.min(desiredWidths[i], mItemMinWidth);
            if (i > 0) {
                widthSum += mItemSpacing;
            }
            widthSum += itemWidth;
            if (widthSum > getWidth()) {
                break;
            }
            numItemsToDisplay++;
        }
        return findBestItemWidth(numItemsToDisplay, desiredWidths);
    }

    // Simple binary search to find the best item width between the min/max widths.
    private int findBestItemWidth(int numItems, int[] desiredWidths) {
        final int totalSpacing = Math.max(0, numItems - 1) * mItemSpacing;
        int low = mItemMinWidth;
        int high = mItemMaxWidth;
        int bestW = mItemMinWidth;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            int totalItemsWidth = 0;
            for (int i = 0; i < numItems; i++) {
                totalItemsWidth += Math.min(desiredWidths[i], mid);
            }

            if (totalItemsWidth + totalSpacing <= getWidth()) {
                bestW = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        return bestW;
    }

    // ForTesting methods:

    void setDesiredWidthsForTesting(int @Nullable [] desiredWidthsForTesting) {
        sDesiredWidthsForTesting = desiredWidthsForTesting;
    }
}
