// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
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

    public ContinuousSearchListCoordinator(ObservableSupplier<Tab> tabSupplier,
            Callback<Boolean> setLayoutVisibility, ThemeColorProvider themeColorProvider,
            Resources resources) {
        mRootViewModel = new PropertyModel(ContinuousSearchListProperties.ROOT_VIEW_KEYS);
        ModelList listItems = new ModelList();
        mRecyclerViewAdapter = new SimpleRecyclerViewAdapter(listItems);

        final PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> viewBinder =
                ContinuousSearchListViewBinder::bindListItem;
        mRecyclerViewAdapter.registerType(ListItemType.GROUP_LABEL,
                (parent) -> inflateListItemView(parent, ListItemType.GROUP_LABEL), viewBinder);
        mRecyclerViewAdapter.registerType(ListItemType.SEARCH_RESULT,
                (parent) -> inflateListItemView(parent, ListItemType.SEARCH_RESULT), viewBinder);
        mRecyclerViewAdapter.registerType(ListItemType.AD,
                (parent) -> inflateListItemView(parent, ListItemType.AD), viewBinder);

        mListMediator = new ContinuousSearchListMediator(
                listItems, mRootViewModel, setLayoutVisibility, themeColorProvider, resources);
        mTabSupplier = tabSupplier;
        mTabSupplier.addObserver(mListMediator);
    }

    private View inflateListItemView(ViewGroup parentView, @ListItemType int listItemType) {
        int layoutId = R.layout.continuous_search_list_group_label;
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
}
