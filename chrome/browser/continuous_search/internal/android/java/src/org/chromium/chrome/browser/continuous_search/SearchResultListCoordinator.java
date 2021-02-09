// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.continuous_search.SearchResultListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Entry point for the UI component of Continuous Search Navigation.
 */
public class SearchResultListCoordinator implements UserData {
    private static final Class<SearchResultListCoordinator> USER_DATA_KEY =
            SearchResultListCoordinator.class;

    private Tab mTab;
    private SearchResultUserData mSearchResultUserData;
    private SearchResultListMediator mMediator;
    private RecyclerView mView;

    static void createForTab(Tab tab) {
        assert tab.getUserDataHost().getUserData(USER_DATA_KEY) == null;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new SearchResultListCoordinator(tab));
    }

    static SearchResultListCoordinator getForTab(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private SearchResultListCoordinator(Tab tab) {
        mTab = tab;
        mView = new RecyclerView(mTab.getContext());
        ModelList listItems = new ModelList();

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listItems);
        adapter.registerType(ListItemType.GROUP_LABEL,
                (parent)
                        -> inflateItemView(parent, ListItemType.GROUP_LABEL),
                SearchResultListViewBinder::bind);
        adapter.registerType(ListItemType.SEARCH_RESULT,
                (parent)
                        -> inflateItemView(parent, ListItemType.SEARCH_RESULT),
                SearchResultListViewBinder::bind);
        adapter.registerType(ListItemType.AD,
                (parent)
                        -> inflateItemView(parent, ListItemType.AD),
                SearchResultListViewBinder::bind);

        mView.setAdapter(adapter);
        mMediator = new SearchResultListMediator(
                listItems, (url) -> tab.loadUrl(new LoadUrlParams(url.getSpec())));
        mSearchResultUserData = SearchResultUserData.getForTab(mTab);
        mSearchResultUserData.addObserver(mMediator);
    }

    private View inflateItemView(ViewGroup parentView, @ListItemType int listItemType) {
        int layoutId = R.layout.continuous_search_list_group_label;
        switch (listItemType) {
            case ListItemType.SEARCH_RESULT:
                layoutId = R.layout.continuous_search_list_result;
                break;
            case ListItemType.AD:
                layoutId = R.layout.continuous_search_list_ad;
                break;
        }

        return LayoutInflater.from(parentView.getContext()).inflate(layoutId, parentView, false);
    }

    public View getView() {
        return mView;
    }

    @Override
    public void destroy() {
        mSearchResultUserData.removeObserver(mMediator);
    }
}
