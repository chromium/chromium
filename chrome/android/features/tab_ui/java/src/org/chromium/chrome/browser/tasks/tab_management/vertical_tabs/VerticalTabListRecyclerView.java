// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Custom {@link TabListRecyclerView} for the vertical tab layout. */
@NullMarked
public class VerticalTabListRecyclerView extends TabListRecyclerView {
    private static final float SCROLL_OFFSET_DIVISOR = 4f;

    public VerticalTabListRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean onInterceptHoverEvent(MotionEvent event) {
        return false;
    }

    /**
     * Sets up the adapter, layout manager and item animator for the vertical tab list.
     *
     * @param modelList The model list of the vertical tab list.
     */
    void initialize(TabListModel modelList) {
        SimpleRecyclerViewAdapter adapter =
                new SimpleRecyclerViewAdapter(modelList) {
                    @Override
                    public int getItemViewType(int position) {
                        ListItem item = modelList.get(position);
                        if (item.type == UiType.TAB) {
                            if (TabProperties.isPinnedTab(item.model)) {
                                return UiType.PINNED_TAB;
                            } else if (TabProperties.isTabGroupHeader(item.model)) {
                                return UiType.TAB_GROUP;
                            }
                        }
                        return super.getItemViewType(position);
                    }
                };

        adapter.registerType(
                UiType.TAB,
                parent ->
                        (VerticalTabItemLayout)
                                LayoutInflater.from(getContext())
                                        .inflate(
                                                R.layout.vertical_tab_item,
                                                parent,
                                                /* attachToRoot= */ false),
                TabVerticalViewBinder::bindTab);

        // Pinned tabs are rendered in a separate sticky layout. This zero-height hidden layout in
        // the main list preserves the 1:1 index alignment with the TabModel without taking space.
        adapter.registerType(
                UiType.PINNED_TAB,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(getContext())
                                        .inflate(
                                                R.layout.vertical_tab_pinned_item_hidden,
                                                parent,
                                                /* attachToRoot= */ false),
                // The placeholder is never shown, so no property needs to be bound to it. It must
                // stay GONE for the lifetime of the view.
                (model, view, propertyKey) -> {});

        adapter.registerType(
                UiType.TAB_GROUP,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(getContext())
                                        .inflate(
                                                R.layout.vertical_tab_group_header,
                                                parent,
                                                /* attachToRoot= */ false),
                TabVerticalViewBinder::bindTabGroupHeader);

        LinearLayoutManager layoutManager =
                new LinearLayoutManager(getContext(), LinearLayoutManager.VERTICAL, false);
        setLayoutManager(layoutManager);
        setAdapter(adapter);
        setupCustomItemAnimator(/* useClipAnimations= */ true);
        setVisibility(View.VISIBLE);
    }

    /**
     * Scrolls the recycler view to the specified position with an offset if it is not completely
     * visible.
     */
    public void scrollToPositionWithOffset(int position) {
        RecyclerView.LayoutManager layoutManager = getLayoutManager();
        if (layoutManager instanceof LinearLayoutManager linearLayoutManager) {
            post(
                    () -> {
                        int first = linearLayoutManager.findFirstCompletelyVisibleItemPosition();
                        int last = linearLayoutManager.findLastCompletelyVisibleItemPosition();
                        if (position < first || position > last) {
                            int offset = Math.round(getHeight() / SCROLL_OFFSET_DIVISOR);
                            linearLayoutManager.scrollToPositionWithOffset(
                                    position, Math.max(0, offset));
                        }
                    });
        }
    }

    /**
     * Scrolls to {@code position} via {@link #scrollToPositionWithOffset(int)} unless the item at
     * that position is already completely visible.
     */
    void scrollToPositionIfNotCompletelyVisible(int position) {
        RecyclerView.LayoutManager layoutManager = getLayoutManager();
        if (layoutManager instanceof LinearLayoutManager lm) {
            int firstVisible = lm.findFirstCompletelyVisibleItemPosition();
            int lastVisible = lm.findLastCompletelyVisibleItemPosition();
            if (firstVisible != RecyclerView.NO_POSITION
                    && lastVisible != RecyclerView.NO_POSITION
                    && position >= firstVisible
                    && position <= lastVisible) {
                return;
            }
        }
        scrollToPositionWithOffset(position);
    }

    /**
     * Runs {@code callback} with the view holder at {@code position}. If the view holder is not
     * attached, scrolls to {@code position} and retries once after layout; {@code callback} is not
     * run if the view holder is still unavailable.
     */
    void findOrScrollToViewHolder(int position, Callback<RecyclerView.ViewHolder> callback) {
        RecyclerView.ViewHolder holder = findViewHolderForAdapterPosition(position);
        if (holder == null) {
            scrollToPosition(position);
            post(
                    () -> {
                        RecyclerView.ViewHolder retryHolder =
                                findViewHolderForAdapterPosition(position);
                        if (retryHolder != null) {
                            callback.onResult(retryHolder);
                        }
                    });
            return;
        }

        callback.onResult(holder);
    }

    /**
     * For vertical tab layout, the non-pinned tab list shares its TabListModel with the pinned tab
     * strip to preserve 1:1 index alignment with TabModel, inflating pinned tabs as 0-height hidden
     * placeholders (vertical_tab_pinned_item_hidden).
     *
     * <p>Default RecyclerView/LinearLayoutManager super functions calculate scroll offset and range
     * by multiplying item positions and total item count by an average row height (avgSizePerRow).
     * Because default super logic assumes all items have uniform visible height, it treats the
     * 0-height pinned placeholders as full-height items, causing the scrollbar thumb to offset
     * downward prematurely and misalign at the bottom of the track.
     *
     * <p>computeVerticalScrollOffset() and computeVerticalScrollRange() override this behavior by
     * excluding pinnedCount from position/count calculations and dynamically computing
     * avgSizePerRow strictly across visible regular tabs to ensure accurate scrollbar positioning
     * and sizing.
     */
    @Override
    public int computeVerticalScrollOffset() {
        if (getLayoutManager() instanceof LinearLayoutManager linearLayoutManager
                && getAdapter() != null) {
            // Count 0-height pinned tabs at the adapter start.
            int pinnedCount = getPinnedTabsCount();
            if (pinnedCount > 0) {
                int firstPos = linearLayoutManager.findFirstVisibleItemPosition();
                int lastPos = linearLayoutManager.findLastVisibleItemPosition();
                int startPos = Math.max(firstPos, pinnedCount);
                View startView = linearLayoutManager.findViewByPosition(startPos);
                View lastView = linearLayoutManager.findViewByPosition(lastPos);

                if (startView != null && lastView != null && lastPos >= startPos) {
                    int realFirstPos = startPos - pinnedCount;
                    int laidOutArea = lastView.getBottom() - startView.getTop();
                    int itemRange = lastPos - startPos + 1;
                    float avgSizePerRow = (float) laidOutArea / itemRange;
                    int topOffset = getPaddingTop() - startView.getTop();
                    return Math.max(0, Math.round(realFirstPos * avgSizePerRow + topOffset));
                }
            }
        }
        return super.computeVerticalScrollOffset();
    }

    @Override
    public int computeVerticalScrollRange() {
        if (getLayoutManager() instanceof LinearLayoutManager linearLayoutManager
                && getAdapter() != null) {
            Adapter<?> adapter = getAdapter();
            if (adapter.getItemCount() > 0) {
                // Count 0-height pinned tabs at the adapter start.
                int pinnedCount = getPinnedTabsCount();
                if (pinnedCount > 0) {
                    // Calculate regular tab count (excluding pinned items).
                    int realItemCount = adapter.getItemCount() - pinnedCount;
                    int firstPos = linearLayoutManager.findFirstVisibleItemPosition();
                    int lastPos = linearLayoutManager.findLastVisibleItemPosition();
                    int startPos = Math.max(firstPos, pinnedCount);
                    View regularFirstView = linearLayoutManager.findViewByPosition(pinnedCount);
                    View startView = linearLayoutManager.findViewByPosition(startPos);
                    View lastView = linearLayoutManager.findViewByPosition(lastPos);

                    boolean fitsCompletely =
                            regularFirstView != null
                                    && regularFirstView.getTop() >= getPaddingTop()
                                    && lastPos == adapter.getItemCount() - 1
                                    && lastView != null
                                    && lastView.getBottom() <= getHeight() - getPaddingBottom();
                    // Hide scrollbar if all regular tabs fit without clipping.
                    if (fitsCompletely) {
                        return computeVerticalScrollExtent();
                    }

                    if (startView != null && lastView != null && lastPos >= startPos) {
                        int laidOutArea = lastView.getBottom() - startView.getTop();
                        int laidOutRange = lastPos - startPos + 1;
                        int totalRange =
                                Math.round(((float) laidOutArea / laidOutRange) * realItemCount)
                                        + getPaddingTop()
                                        + getPaddingBottom();
                        return Math.max(computeVerticalScrollExtent(), totalRange);
                    }
                }
            }
        }
        return super.computeVerticalScrollRange();
    }

    private int getPinnedTabsCount() {
        Adapter<?> adapter = getAdapter();
        if (adapter == null) return 0;
        int pinnedCount = 0;
        for (int i = 0; i < adapter.getItemCount(); i++) {
            if (adapter.getItemViewType(i) == TabProperties.UiType.PINNED_TAB) {
                pinnedCount++;
            } else {
                break;
            }
        }
        return pinnedCount;
    }
}
