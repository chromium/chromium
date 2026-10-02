// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Custom {@link TabListRecyclerView} for the pinned tabs in the vertical tab layout. */
@NullMarked
public class VerticalPinnedTabListRecyclerView extends TabListRecyclerView {
    @VisibleForTesting static final int DEFAULT_GRID_SPAN_COUNT = 4;
    @VisibleForTesting static final int MAX_SINGLE_ROW_SPAN_COUNT = 5;
    @VisibleForTesting static final int COLLAPSED_GRID_SPAN_COUNT = 1;
    // Epsilon (5% of a column span) absorbs sub-pixel rounding errors at boundary thresholds.
    private static final float SPAN_CALCULATION_EPSILON = 0.05f;

    private boolean mIsInTransition;

    public VerticalPinnedTabListRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets up the adapter, item animator, grid layout manager and column gap decoration.
     *
     * @param pinnedTabsModelList The model list of the pinned tabs.
     * @param collapseState The current {@link RailCollapseState} of the rail.
     */
    void initialize(TabListModel pinnedTabsModelList, @RailCollapseState int collapseState) {
        SimpleRecyclerViewAdapter adapter =
                new SimpleRecyclerViewAdapter(pinnedTabsModelList) {
                    @Override
                    public int getItemViewType(int position) {
                        ListItem item = pinnedTabsModelList.get(position);
                        if (item.type == UiType.TAB) {
                            return UiType.PINNED_TAB;
                        }
                        return super.getItemViewType(position);
                    }
                };

        adapter.registerType(
                UiType.PINNED_TAB,
                parent -> {
                    VerticalTabItemLayout view =
                            (VerticalTabItemLayout)
                                    LayoutInflater.from(getContext())
                                            .inflate(
                                                    R.layout.vertical_tab_item,
                                                    parent,
                                                    /* attachToRoot= */ false);
                    view.configureAsPinnedTab();
                    return view;
                },
                TabVerticalViewBinder::bindPinnedTab);

        setAdapter(adapter);
        setupCustomItemAnimator(/* useClipAnimations= */ true);
        // The span count depends on the pinned tab count, so the adapter must be set first.
        setLayoutManager(
                new GridLayoutManager(getContext(), calculateSpanCount(collapseState)) {
                    @Override
                    protected void calculateExtraLayoutSpace(
                            RecyclerView.State state, int[] extraLayoutSpace) {
                        super.calculateExtraLayoutSpace(state, extraLayoutSpace);
                        calculatePinnedExtraLayoutSpace(state, extraLayoutSpace);
                    }
                });
        addItemDecoration(createItemDecoration());
    }

    /**
     * Sets whether an animated rail collapse/expand transition is in progress.
     *
     * @param inTransition True if the rail is actively transitioning.
     */
    void setInTransition(boolean inTransition) {
        if (mIsInTransition == inTransition) return;
        mIsInTransition = inTransition;
        // Transition start: SideUiCoordinator captures the transition start values before it
        // changes the container width, so request a layout here. The synchronous measure/layout
        // it runs before TransitionManager#beginDelayedTransition then attaches the offscreen
        // pinned tabs, giving ChangeBounds start values for them.
        // Transition end: request a layout to recycle extra items back to viewport bounds.
        ViewUtils.requestLayout(this, "VerticalPinnedTabListRecyclerView.setInTransition");
    }

    /** Removes and recycles all item views, e.g. when the pinned tab list is hidden. */
    void recycleItemViews() {
        swapAdapter(getAdapter(), /* removeAndRecycleExistingViews= */ true);
    }

    /**
     * Updates the grid column span count for the given rail collapse state, and refreshes the
     * column gap decoration.
     */
    void updateSpanCount(@RailCollapseState int collapseState) {
        if (!(getLayoutManager() instanceof GridLayoutManager gridLayoutManager)) return;
        gridLayoutManager.setSpanCount(calculateSpanCount(collapseState));
        invalidateItemDecorations();
    }

    /**
     * Returns the grid column span count for the Left Rail based on measured width and pinned tab
     * count. The width is taken from the parent vertical tab rail.
     */
    @VisibleForTesting
    int calculateSpanCount(@RailCollapseState int collapseState) {
        if (VerticalTabRailCollapseController.shouldUseCollapsedPositioning(collapseState)) {
            return COLLAPSED_GRID_SPAN_COUNT;
        }

        if (!(getParent() instanceof View container)) return DEFAULT_GRID_SPAN_COUNT;
        int containerWidth = container.getWidth();
        if (containerWidth <= 0) return DEFAULT_GRID_SPAN_COUNT;

        int paddingStart = container.getPaddingStart();
        int paddingEnd = container.getPaddingEnd();
        int availableWidth = containerWidth - paddingStart - paddingEnd;

        Context context = getContext();
        Adapter<?> adapter = getAdapter();
        int pinnedTabCount = adapter != null ? adapter.getItemCount() : 0;
        return calculateBalancedSpanCount(
                availableWidth,
                pinnedTabCount,
                context.getResources(),
                VerticalTabUtils.isTablet(context));
    }

    /**
     * Calculates and applies extra layout space for pinned tabs during transitions so that
     * boundary/trailing pinned tabs remain attached for ChangeBounds transitions.
     */
    @VisibleForTesting
    void calculatePinnedExtraLayoutSpace(RecyclerView.State state, int[] extraLayoutSpace) {
        if (!mIsInTransition) return;
        int railHeight = getParent() instanceof View container ? container.getHeight() : 0;
        int height = Math.max(railHeight, getResources().getDisplayMetrics().heightPixels);
        int itemCount = state.getItemCount();
        if (itemCount > 0) {
            Context context = getContext();
            int itemHeight =
                    TabVerticalViewBinder.getPinnedItemHeight(context)
                            + context.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.vertical_tab_pinned_item_margin_bottom);
            int padding = getPaddingTop() + getPaddingBottom();
            int totalContentHeight = itemCount * itemHeight + padding;
            // Cap to the maximum items that can physically fit in the expanded
            // viewport across all columns (at most MAX_SINGLE_ROW_SPAN_COUNT = 5),
            // avoiding layout overhead for items that remain offscreen.
            int maxExpandedHeight = height * MAX_SINGLE_ROW_SPAN_COUNT + padding;
            height = Math.clamp(totalContentHeight, height, maxExpandedHeight);
        }
        extraLayoutSpace[0] = Math.max(extraLayoutSpace[0], height);
        extraLayoutSpace[1] = Math.max(extraLayoutSpace[1], height);
    }

    /**
     * Calculates the grid column span count for pinned tabs based on available width and tab count.
     */
    @VisibleForTesting
    static int calculateBalancedSpanCount(
            int availableWidth, int pinnedTabCount, Resources res, boolean isTablet) {
        int minItemWidth =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tab_pinned_item_min_width_tablet
                                : R.dimen.vertical_tab_pinned_item_min_width);
        int minHorizontalGap = res.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        if (minItemWidth <= 0) return DEFAULT_GRID_SPAN_COUNT;

        float spansFittingWidth =
                (float) (availableWidth + minHorizontalGap) / (minItemWidth + minHorizontalGap)
                        + SPAN_CALCULATION_EPSILON;
        int maxFitSpans =
                Math.clamp((int) Math.floor(spansFittingWidth), 1, MAX_SINGLE_ROW_SPAN_COUNT);

        if (pinnedTabCount <= 0) {
            return maxFitSpans;
        }

        // Uses integer ceiling division (A + B - 1) / B instead of A / B (which truncates and
        // would yield 0 rows when pinnedTabCount < maxFitSpans) to calculate the full number of
        // rows needed, balancing tabs evenly across rows.
        int rows = (pinnedTabCount + maxFitSpans - 1) / maxFitSpans;
        int columns = (pinnedTabCount + rows - 1) / rows;
        return Math.clamp(columns, 1, maxFitSpans);
    }

    /** Returns an item decoration that distributes the gaps between grid columns. */
    @VisibleForTesting
    static RecyclerView.ItemDecoration createItemDecoration() {
        return new RecyclerView.ItemDecoration() {
            @Override
            public void getItemOffsets(
                    Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
                calculateItemOffsets(outRect, view, parent);
            }
        };
    }

    /**
     * Distributes inter-item horizontal gaps evenly across grid columns without outer margins,
     * ensuring identical visual item widths since RecyclerView does not support layout_weight.
     */
    private static void calculateItemOffsets(Rect outRect, View view, RecyclerView parent) {
        int position = parent.getChildAdapterPosition(view);
        if (position == RecyclerView.NO_POSITION) {
            position = parent.indexOfChild(view);
        }
        if (position == RecyclerView.NO_POSITION) return;
        if (!(parent.getLayoutManager() instanceof GridLayoutManager gridLayoutManager)) return;
        int spanCount = gridLayoutManager.getSpanCount();
        if (spanCount <= 1) {
            outRect.left = 0;
            outRect.right = 0;
            return;
        }
        int minHorizontalGap =
                parent.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        int column = position % spanCount;
        int left = column * minHorizontalGap / spanCount;
        int right = minHorizontalGap - (column + 1) * minHorizontalGap / spanCount;
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        outRect.left = isRtl ? right : left;
        outRect.right = isRtl ? left : right;
    }
}
