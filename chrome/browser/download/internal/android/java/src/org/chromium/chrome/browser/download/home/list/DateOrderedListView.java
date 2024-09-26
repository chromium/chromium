// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.Recycler;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator.DateOrderedListObserver;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.holder.ListItemViewHolder;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * The View component of a DateOrderedList.  This takes the DateOrderedListModel and creates the
 * glue to display it on the screen.
 */
class DateOrderedListView {
    private final DownloadManagerUiConfig mConfig;
    private final DecoratedListItemModel mModel;

    private final int mIdealImageWidthPx;
    private final int mInterImagePaddingPx;
    private final int mPrefetchVerticalPaddingPx;
    private final int mHorizontalPaddingPx;
    private final int mVerticalPaddingPx;
    private final int mMaxWidthImageItemPx;

    private final RecyclerView mView;
    private final GridLayoutManager mGridLayoutManager;
    private final UiConfig mUiConfig;
    private Runnable mOnConfigurationChangedCallback;

    /** Creates an instance of a {@link DateOrderedListView} representing {@code model}. */
    public DateOrderedListView(
            Context context,
            DownloadManagerUiConfig config,
            DecoratedListItemModel model,
            DateOrderedListObserver dateOrderedListObserver,
            Runnable onConfigurationChangedCallback) {
        mConfig = config;
        mModel = model;

        mIdealImageWidthPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.download_manager_ideal_image_width);
        mInterImagePaddingPx =
                context.getResources()
                        .getDimensionPixelOffset(R.dimen.download_manager_image_padding);
        mHorizontalPaddingPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.download_manager_horizontal_margin);
        mPrefetchVerticalPaddingPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.download_manager_prefetch_vertical_margin);
        mVerticalPaddingPx =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.download_manager_vertical_margin_between_download_types);
        mMaxWidthImageItemPx =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.download_manager_max_image_item_width_wide_screen);

        mView =
                new RecyclerView(context) {
                    private int mScreenOrientation = Configuration.ORIENTATION_UNDEFINED;

                    @Override
                    protected void onConfigurationChanged(Configuration newConfig) {
                        super.onConfigurationChanged(newConfig);
                        mUiConfig.updateDisplayStyle();
                        if (newConfig.orientation == mScreenOrientation) return;

                        mScreenOrientation = newConfig.orientation;
                        mView.invalidateItemDecorations();
                        mOnConfigurationChangedCallback.run();
                    }
                };
        mView.setId(R.id.download_home_recycler_view);
        mView.setHasFixedSize(true);
        ((DefaultItemAnimator) mView.getItemAnimator()).setSupportsChangeAnimations(false);
        mView.getItemAnimator().setMoveDuration(0);

        mGridLayoutManager = new GridLayoutManagerImpl(context);
        mView.setLayoutManager(mGridLayoutManager);
        mView.addItemDecoration(new ItemDecorationImpl());
        mView.setClipToPadding(false);

        PropertyModelChangeProcessor.create(
                mModel.getProperties(), mView, new ListPropertyViewBinder());

        // Do the final hook up to the underlying data adapter.
        DateOrderedListViewAdapter adapter =
                new DateOrderedListViewAdapter(
                        mModel, new ModelChangeProcessor(mModel), ListItemViewHolder::create);
        mView.setAdapter(adapter);
        mView.post(adapter::notifyDataSetChanged);
        mView.addOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView view, int dx, int dy) {
                        dateOrderedListObserver.onListScroll(mView.canScrollVertically(-1));
                    }
                });

        mUiConfig = new UiConfig(mView);
        mUiConfig.addObserver(
                (newDisplayStyle) -> {
                    int padding =
                            getPaddingForDisplayStyle(newDisplayStyle, context.getResources());
                    mView.setPaddingRelative(
                            padding, mView.getPaddingTop(), padding, mView.getPaddingBottom());
                });
        mOnConfigurationChangedCallback = onConfigurationChangedCallback;
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }

    /**
     * @return The start and end padding of the recycler view for the given display style.
     */
    private static int getPaddingForDisplayStyle(
            UiConfig.DisplayStyle displayStyle, Resources resources) {
        int padding = 0;
        if (displayStyle.horizontal == HorizontalDisplayStyle.WIDE) {
            int screenWidthDp = resources.getConfiguration().screenWidthDp;
            padding =
                    (int)
                            (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f)
                                    * resources.getDisplayMetrics().density);
            padding =
                    (int)
                            Math.max(
                                    resources.getDimensionPixelSize(
                                            R.dimen
                                                    .download_manager_recycler_view_min_padding_wide_screen),
                                    padding);
        }
        return padding;
    }

    /** @return The view width available after start and end padding. */
    private int getAvailableViewWidth() {
        return mView.getWidth()
                - ViewCompat.getPaddingStart(mView)
                - ViewCompat.getPaddingEnd(mView);
    }

    private class GridLayoutManagerImpl extends GridLayoutManager {
        /** Creates an instance of a {@link GridLayoutManagerImpl}. */
        public GridLayoutManagerImpl(Context context) {
            super(context, /* spanCount= */ 1, VERTICAL, /* reverseLayout= */ false);
            setSpanSizeLookup(new SpanSizeLookupImpl());
        }

        // GridLayoutManager implementation.
        @Override
        public void onLayoutChildren(Recycler recycler, State state) {
            assert getOrientation() == VERTICAL;

            int availableWidth = getAvailableViewWidth() - 2 * mHorizontalPaddingPx;
            int columnWidth = mIdealImageWidthPx - mInterImagePaddingPx;

            int easyFitSpan = availableWidth / columnWidth;
            double remaining =
                    ((double) (availableWidth - easyFitSpan * columnWidth)) / columnWidth;
            if (remaining > 0.5) easyFitSpan++;
            setSpanCount(Math.max(1, easyFitSpan));

            super.onLayoutChildren(recycler, state);
        }

        @Override
        public boolean supportsPredictiveItemAnimations() {
            return false;
        }

        private class SpanSizeLookupImpl extends SpanSizeLookup {
            // SpanSizeLookup implementation.
            @Override
            public int getSpanSize(int position) {
                return ListUtils.getSpanSize(mModel.get(position), mConfig, getSpanCount());
            }
        }
    }

    private class ItemDecorationImpl extends ItemDecoration {
        // ItemDecoration implementation.
        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
            int position = parent.getChildAdapterPosition(view);
            if (position < 0 || position >= mModel.size()) return;

            ListItem item = mModel.get(position);
            boolean isFullWidthMedia = false;
            @ListUtils.ViewType
            int viewType = ListUtils.getViewTypeForItem(mModel.get(position), mConfig);
            switch (viewType) {
                case ListUtils.ViewType.IMAGE: // fall through
                case ListUtils.ViewType.IMAGE_FULL_WIDTH: // fall through
                case ListUtils.ViewType.IN_PROGRESS_IMAGE:
                    isFullWidthMedia = ((ListItem.OfflineItemListItem) item).spanFullWidth;
                    if (isFullWidthMedia || mGridLayoutManager.getSpanCount() == 1) {
                        outRect.left = mHorizontalPaddingPx;
                        outRect.right = mHorizontalPaddingPx;
                    } else {
                        computeItemDecoration(position, outRect);
                    }

                    outRect.top = mInterImagePaddingPx / 2;
                    outRect.bottom = mInterImagePaddingPx / 2;
                    break;
                case ListUtils.ViewType.VIDEO: // Intentional fallthrough.
                case ListUtils.ViewType.IN_PROGRESS_VIDEO:
                    outRect.left = mHorizontalPaddingPx;
                    outRect.right = mHorizontalPaddingPx;
                    outRect.top = mPrefetchVerticalPaddingPx / 2;
                    outRect.bottom = mPrefetchVerticalPaddingPx / 2;
                    isFullWidthMedia = true;
                    break;
                case ListUtils.ViewType.PREFETCH_ARTICLE: // fall through
                case ListUtils.ViewType.AUDIO:
                    outRect.left = mHorizontalPaddingPx;
                    outRect.right = mHorizontalPaddingPx;
                    outRect.top = mPrefetchVerticalPaddingPx / 2;
                    outRect.bottom = mPrefetchVerticalPaddingPx / 2;
                    break;
                case ListUtils.ViewType.GROUP_CARD_HEADER: // fall through
                case ListUtils.ViewType.GROUP_CARD_FOOTER: // fall through
                case ListUtils.ViewType.GROUP_CARD_ITEM: // fall through
                case ListUtils.ViewType.GROUP_CARD_DIVIDER_MIDDLE:
                    outRect.left = mHorizontalPaddingPx;
                    outRect.right = mHorizontalPaddingPx;
                    break;
                case ListUtils.ViewType.GROUP_CARD_DIVIDER_TOP:
                    outRect.left = mHorizontalPaddingPx;
                    outRect.right = mHorizontalPaddingPx;
                    outRect.top = mPrefetchVerticalPaddingPx / 2;
                    break;
                case ListUtils.ViewType.GROUP_CARD_DIVIDER_BOTTOM:
                    outRect.left = mHorizontalPaddingPx;
                    outRect.right = mHorizontalPaddingPx;
                    outRect.bottom = mPrefetchVerticalPaddingPx / 2;
                    break;
            }

            if (isFullWidthMedia
                    && mUiConfig.getCurrentDisplayStyle().horizontal
                            == HorizontalDisplayStyle.WIDE) {
                outRect.right += Math.max(getAvailableViewWidth() - mMaxWidthImageItemPx, 0);
            }

            // If the current item is the last of its download type in a given section and not
            // displayed in a grid, add padding below. Grid items are handled differently as
            // described in the next section.
            if (isLastOfDownloadTypeInSection(position) && !isGridItem(position)) {
                outRect.bottom += mVerticalPaddingPx;
            }

            // If the previous item was a grid item, and current one is not, add padding above to
            // differentiate between sections.
            if (position > 0
                    && isLastOfDownloadTypeInSection(position - 1)
                    && isGridItem(position - 1)) {
                outRect.top += mVerticalPaddingPx;
            }
        }

        private void computeItemDecoration(int position, Rect outRect) {
            GridLayoutManager.SpanSizeLookup spanLookup = mGridLayoutManager.getSpanSizeLookup();
            int spanCount = mGridLayoutManager.getSpanCount();
            int columnIndex = spanLookup.getSpanIndex(position, spanCount);

            horizontallyRepositionGridItem(
                    columnIndex,
                    spanCount,
                    mHorizontalPaddingPx,
                    mHorizontalPaddingPx,
                    mInterImagePaddingPx,
                    outRect);
        }
    }

    /**
     * Given the column index for an image in a grid view, computes the left and right edge
     * offsets.
     * @param columnIndex Column index for the item
     * @param spanCount Span count of the grid
     * @param leftMargin Leftmost margin in the row
     * @param rightMargin Rightmost margin in the row
     * @param padding Spacing between two items
     * @param outRect The output rect that contains the computed offsets
     */
    private static void horizontallyRepositionGridItem(
            int columnIndex,
            int spanCount,
            int leftMargin,
            int rightMargin,
            int padding,
            Rect outRect) {
        assert spanCount > 1;

        // Margin here refers to the leftmost or rightmost margin in the row, and padding here
        // refers to the inter-image spacing.

        // Calculate how much each image should be shrunk compared to the ideal image size if no
        // margin or padding were present.
        int shrink = (leftMargin + rightMargin + (spanCount - 1) * padding) / spanCount;

        // Starting from left, calculate how much the image is shifted from ideal position
        // due to the leftmost margin and padding between previous images. Subtract the
        // total shrink for the previous images from this value.
        outRect.left = leftMargin + columnIndex * padding - columnIndex * shrink;

        // For right edge, the calculation is exactly same as left, except we have one extra
        // shrink. Negate the final value.
        outRect.right = -(leftMargin + columnIndex * padding - (columnIndex + 1) * shrink);
    }

    /**
     * Determines whether or not the item at position is an OfflineItemListItem (representing a
     * downloaded item) and is the last of its type in a given section. Does so by comparing the
     * current item to the following item.
     * @param position Index of the item we are checking
     * @return Whether or not the current item is the last of its download type in a given section.
     */
    private boolean isLastOfDownloadTypeInSection(int position) {
        // If the current item is not an OfflineItemListItem, it cannot have a download type, and
        // thus can't be the last of its download type.
        ListItem currentItem = mModel.get(position);
        if (!(currentItem instanceof OfflineItemListItem)) return false;

        // If the next item is not an OfflineItemListItem, it cannot have a download type. This
        // means the next item can't be the same type as the current item and the current item is
        // therefore the last of its download type.
        ListItem nextItem = position >= mModel.size() - 1 ? null : mModel.get(position + 1);
        if (!(nextItem instanceof OfflineItemListItem)) return true;

        // If both items are OfflineItemListItems, but are of different type, then the current item
        // is the last of its type.
        boolean nextItemIsDifferentType =
                ((OfflineItemListItem) currentItem).item.filter
                        != ((OfflineItemListItem) nextItem).item.filter;
        if (nextItemIsDifferentType) return true;

        // If this point is reached, both items are OfflineListItems and the same type, meaning the
        // current item is not the last of its type.
        return false;
    }

    /**
     * Determines whether or not the item at position is displayed in a grid (e.g. multiple images).
     * Does so by checking if the item's span size is less than the span count of a row.
     * @param position Index of the item we are checking
     * @return Whether or not the item at position is displayed in a grid.
     */
    private boolean isGridItem(int position) {
        GridLayoutManager.SpanSizeLookup spanLookup = mGridLayoutManager.getSpanSizeLookup();
        int spanCount = mGridLayoutManager.getSpanCount();
        int spanSize = spanLookup.getSpanSize(position);

        return spanSize < spanCount;
    }

    private class ModelChangeProcessor extends ForwardingListObservable<Void>
            implements RecyclerViewAdapter.Delegate<ListItemViewHolder, Void> {
        private final DecoratedListItemModel mModel;

        public ModelChangeProcessor(DecoratedListItemModel model) {
            mModel = model;
            model.addObserver(this);
        }

        @Override
        public int getItemCount() {
            return mModel.size();
        }

        @Override
        public int getItemViewType(int position) {
            return ListUtils.getViewTypeForItem(mModel.get(position), mConfig);
        }

        @Override
        public void onBindViewHolder(
                ListItemViewHolder viewHolder, int position, @Nullable Void payload) {
            viewHolder.bind(mModel.getProperties(), mModel.get(position));
        }

        @Override
        public void onViewRecycled(ListItemViewHolder viewHolder) {
            viewHolder.recycle();
        }
    }
}
