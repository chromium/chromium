// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_FOCUSED;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BLOCK_TOUCH_INPUT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_CLIP_TO_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for TabListRecyclerView. */
class TabListContainerViewBinder {
    /**
     * Bind the given model to the given view, updating the payload in propertyKey.
     *
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, TabListRecyclerView view, PropertyKey propertyKey) {
        if (BLOCK_TOUCH_INPUT == propertyKey) {
            view.setBlockTouchInput(model.get(BLOCK_TOUCH_INPUT));
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            int index = model.get(INITIAL_SCROLL_INDEX);
            int offset = computeOffset(view, model);
            // RecyclerView#scrollToPosition(int) behaves incorrectly first time after cold start.
            ((LinearLayoutManager) view.getLayoutManager())
                    .scrollToPositionWithOffset(index, offset);
        } else if (FOCUS_TAB_INDEX_FOR_ACCESSIBILITY == propertyKey) {
            int index = model.get(FOCUS_TAB_INDEX_FOR_ACCESSIBILITY);
            RecyclerView.ViewHolder selectedViewHolder =
                    view.findViewHolderForAdapterPosition(index);
            if (selectedViewHolder == null) return;
            View focusView = selectedViewHolder.itemView;
            focusView.requestFocus();
            focusView.sendAccessibilityEvent(TYPE_VIEW_FOCUSED);
        } else if (BOTTOM_PADDING == propertyKey) {
            int left = view.getPaddingLeft();
            int top = view.getPaddingTop();
            int right = view.getPaddingRight();
            int bottom = model.get(BOTTOM_PADDING);
            view.setPadding(left, top, right, bottom);
        } else if (IS_CLIP_TO_PADDING == propertyKey) {
            view.setClipToPadding(model.get(IS_CLIP_TO_PADDING));
        } else if (FETCH_VIEW_BY_INDEX_CALLBACK == propertyKey) {
            Callback<Function<Integer, View>> callback = model.get(FETCH_VIEW_BY_INDEX_CALLBACK);
            callback.onResult(
                    (Integer index) -> {
                        RecyclerView.ViewHolder viewHolder =
                                view.findViewHolderForAdapterPosition(index);
                        return viewHolder == null ? null : viewHolder.itemView;
                    });
        } else if (GET_VISIBLE_RANGE_CALLBACK == propertyKey) {
            Callback<Supplier<Pair<Integer, Integer>>> callback =
                    model.get(GET_VISIBLE_RANGE_CALLBACK);
            callback.onResult(
                    () -> {
                        LinearLayoutManager layoutManager =
                                (LinearLayoutManager) view.getLayoutManager();
                        int start = layoutManager.findFirstCompletelyVisibleItemPosition();
                        int end = layoutManager.findLastCompletelyVisibleItemPosition();
                        return new Pair<>(start, end);
                    });
        } else if (IS_SCROLLING_SUPPLIER_CALLBACK == propertyKey) {
            Callback<ObservableSupplier<Boolean>> callback =
                    model.get(IS_SCROLLING_SUPPLIER_CALLBACK);
            ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>(false);
            view.addOnScrollListener(
                    new OnScrollListener() {
                        @Override
                        public void onScrollStateChanged(
                                @NonNull RecyclerView recyclerView, int newState) {
                            supplier.set(newState != RecyclerView.SCROLL_STATE_IDLE);
                        }
                    });
            callback.onResult(supplier);
        }
    }

    private static int computeOffset(TabListRecyclerView view, PropertyModel model) {
        int width = view.getWidth();
        int height = view.getHeight();
        final BrowserControlsStateProvider browserControlsStateProvider =
                model.get(BROWSER_CONTROLS_STATE_PROVIDER);
        // If layout hasn't happened yet fallback to dimensions based on visible display frame. This
        // works for multi-window and different orientations. Don't use View#post() because this
        // will cause animation jank for expand/shrink animations.
        if (width == 0 || height == 0) {
            Rect frame = new Rect();
            ((Activity) view.getContext())
                    .getWindow()
                    .getDecorView()
                    .getWindowVisibleDisplayFrame(frame);
            width = frame.width();
            // Remove toolbar height from height.
            height =
                    frame.height()
                            - Math.round(browserControlsStateProvider.getTopVisibleContentOffset());
        }
        if (width <= 0 || height <= 0) return 0;

        @TabListCoordinator.TabListMode int mode = model.get(MODE);
        LinearLayoutManager layoutManager = (LinearLayoutManager) view.getLayoutManager();
        if (mode == TabListCoordinator.TabListMode.GRID) {
            GridLayoutManager gridLayoutManager = (GridLayoutManager) layoutManager;
            int cardWidth = width / gridLayoutManager.getSpanCount();
            int cardHeight =
                    TabUtils.deriveGridCardHeight(
                            cardWidth, view.getContext(), browserControlsStateProvider);
            return Math.max(0, height / 2 - cardHeight / 2);
        }
        if (mode == TabListCoordinator.TabListMode.LIST) {
            // Avoid divide by 0 when there are no tabs.
            if (layoutManager.getItemCount() == 0) return 0;

            return Math.max(
                    0,
                    height / 2
                            - view.computeVerticalScrollRange() / layoutManager.getItemCount() / 2);
        }
        assert false : "Unexpected MODE when setting INITIAL_SCROLL_INDEX.";
        return 0;
    }
}
