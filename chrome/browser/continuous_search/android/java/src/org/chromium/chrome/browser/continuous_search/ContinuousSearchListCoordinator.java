// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Coordinator for Continuous Search Navigation UI: A RecyclerView that displays items provided
 * by the CSN infrastructure.
 */
public class ContinuousSearchListCoordinator {
    private final ContinuousSearchListMediator mListMediator;
    private final SimpleRecyclerViewAdapter mRecyclerViewAdapter;

    public ContinuousSearchListCoordinator(Callback<Boolean> setLayoutVisibility) {
        ModelList listItems = new ModelList();
        mRecyclerViewAdapter = new SimpleRecyclerViewAdapter(listItems);
        mRecyclerViewAdapter.registerType(ListItemType.GROUP_LABEL,
                (parent)
                        -> inflateListItemView(parent, ListItemType.GROUP_LABEL),
                ContinuousSearchListViewBinder::bind);
        mRecyclerViewAdapter.registerType(ListItemType.SEARCH_RESULT,
                (parent)
                        -> inflateListItemView(parent, ListItemType.SEARCH_RESULT),
                ContinuousSearchListViewBinder::bind);
        mRecyclerViewAdapter.registerType(ListItemType.AD,
                (parent)
                        -> inflateListItemView(parent, ListItemType.AD),
                ContinuousSearchListViewBinder::bind);

        mListMediator = new ContinuousSearchListMediator(listItems, setLayoutVisibility);
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

    void initializeLayout(ViewGroup root) {
        RecyclerView recyclerView = new RecyclerView(root.getContext());
        ViewGroup.LayoutParams lp = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        root.addView(recyclerView, lp);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(root.getContext(), LinearLayoutManager.HORIZONTAL, false);
        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(mRecyclerViewAdapter);
    }

    void onObserverNewTab(Tab tab) {
        mListMediator.onObserverNewTab(tab);
    }

    void destroy() {
        mListMediator.destroy();
    }
}
