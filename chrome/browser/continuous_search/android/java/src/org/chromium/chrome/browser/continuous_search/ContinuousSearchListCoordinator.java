// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.VisibilitySettings;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Coordinator for Continuous Search Navigation UI: A RecyclerView that displays items provided
 * by the CSN infrastructure.
 */
public class ContinuousSearchListCoordinator {
    private final ContinuousSearchListMediator mListMediator;
    private final SimpleRecyclerViewAdapter mRecyclerViewAdapter;
    private final ObservableSupplier<Tab> mTabSupplier;
    private final PropertyModel mRootViewModel;
    private final Resources mResources;

    public ContinuousSearchListCoordinator(
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<Tab> tabSupplier, Callback<VisibilitySettings> setLayoutVisibility,
            ThemeColorProvider themeColorProvider, Resources resources) {
        ContinuousSearchConfiguration.initialize();
        mRootViewModel = new PropertyModel(ContinuousSearchListProperties.ALL_KEYS);
        ModelList listItems = new ModelList();
        mRecyclerViewAdapter = new SimpleRecyclerViewAdapter(listItems);
        mResources = resources;

        mRecyclerViewAdapter.registerType(ListItemType.PROVIDER,
                (parent)
                        -> inflateListItemView(parent, ListItemType.PROVIDER),
                ContinuousSearchListViewBinder::bindProvider);
        mRecyclerViewAdapter.registerType(ListItemType.SEARCH_RESULT,
                (parent)
                        -> inflateListItemView(parent, ListItemType.SEARCH_RESULT),
                ContinuousSearchListViewBinder::bindListItem);
        mRecyclerViewAdapter.registerType(ListItemType.AD,
                (parent)
                        -> inflateListItemView(parent, ListItemType.AD),
                ContinuousSearchListViewBinder::bindListItem);

        mListMediator = new ContinuousSearchListMediator(browserControlsStateProvider, listItems,
                mRootViewModel, setLayoutVisibility, themeColorProvider, resources);
        mTabSupplier = tabSupplier;
        mTabSupplier.addObserver(mListMediator);
    }

    private View inflateListItemView(ViewGroup parentView, @ListItemType int listItemType) {
        int layoutId = R.layout.continuous_search_list_provider;
        switch (listItemType) {
            case ListItemType.SEARCH_RESULT:
                layoutId = R.layout.continuous_search_list_item;
                break;
            case ListItemType.AD:
                layoutId = R.layout.continuous_search_list_ad;
                break;
        }

        return LayoutInflater.from(parentView.getContext()).inflate(layoutId, parentView, false);
    }

    void initializeLayout(ViewGroup container) {
        View rootView = LayoutInflater.from(container.getContext())
                                .inflate(R.layout.continuous_search_layout, container, false);
        PropertyModelChangeProcessor.create(
                mRootViewModel, rootView, ContinuousSearchListViewBinder::bindRootView);

        ViewGroup.LayoutParams lp = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        container.addView(rootView, /*index=*/0, lp);

        RecyclerView recyclerView = rootView.findViewById(R.id.recycler_view);
        LinearLayoutManager layoutManager = new LinearLayoutManager(
                container.getContext(), LinearLayoutManager.HORIZONTAL, false);
        recyclerView.setLayoutManager(layoutManager);
        recyclerView.addItemDecoration(new SpaceItemDecoration(mResources));
        recyclerView.setAdapter(mRecyclerViewAdapter);
        recyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                // onScrolled is also called after a layout calculation. dx will be 0 in that case.
                if (dx != 0) mListMediator.onScrolled();
            }
        });
    }

    void destroy() {
        mTabSupplier.removeObserver(mListMediator);
        mListMediator.destroy();
    }

    private static class SpaceItemDecoration extends ItemDecoration {
        private final int mChipSpacingPx;
        private final int mSidePaddingPx;

        public SpaceItemDecoration(Resources resources) {
            mChipSpacingPx =
                    (int) resources.getDimensionPixelSize(R.dimen.csn_chip_list_chip_spacing);
            mSidePaddingPx =
                    (int) resources.getDimensionPixelSize(R.dimen.csn_chip_list_side_padding);
        }

        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
            int position = parent.getChildAdapterPosition(view);
            boolean isFirst = position == 0;
            boolean isLast = position == parent.getAdapter().getItemCount() - 1;

            outRect.left = isFirst ? mSidePaddingPx : mChipSpacingPx;
            outRect.right = isLast ? mSidePaddingPx : mChipSpacingPx;
        }
    }
}
