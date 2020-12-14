// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.continuous_search.SearchResultListProperties.ListItemType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Business logic for the UI component of Continuous Search Navigation. This class updates the UI on
 * search result updates.
 */
public class SearchResultListMediator implements SearchResultUserDataObserver {
    private ModelList mModelList;
    private Callback<GURL> mUrlClickHandler;

    SearchResultListMediator(ModelList modelList, Callback<GURL> urlClickHandler) {
        mModelList = modelList;
        mUrlClickHandler = urlClickHandler;
    }

    private void handleResultClick(GURL url) {
        if (url == null || mUrlClickHandler == null) return;

        mUrlClickHandler.onResult(url);
    }

    @Override
    public void onInvalidate() {
        mModelList.clear();
    }

    @Override
    public void onUpdate(SearchResultMetadata metadata, GURL currentUrl) {
        mModelList.clear();
        for (SearchResultGroup group : metadata.getGroups()) {
            if (!group.isAdGroup()) {
                mModelList.add(new ModelListAdapter.ListItem(
                        ListItemType.GROUP_LABEL, generateListItem(group.getLabel(), null)));
            }
            int itemType = group.isAdGroup() ? ListItemType.AD : ListItemType.SEARCH_RESULT;
            for (SearchResult result : group.getResults()) {
                mModelList.add(new ListItem(
                        itemType, generateListItem(result.getTitle(), result.getUrl())));
            }
        }
    }

    @Override
    public void onUrlChanged(GURL currentUrl) {
        for (ListItem listItem : mModelList) {
            if (listItem.type == ListItemType.GROUP_LABEL) continue;

            boolean isSelected = currentUrl != null
                    && currentUrl.equals(listItem.model.get(SearchResultListProperties.URL));
            listItem.model.set(SearchResultListProperties.IS_SELECTED, isSelected);
        }
    }

    private PropertyModel generateListItem(String text, GURL url) {
        return new PropertyModel.Builder(SearchResultListProperties.ALL_KEYS)
                .with(SearchResultListProperties.LABEL, text)
                .with(SearchResultListProperties.URL, url)
                .with(SearchResultListProperties.IS_SELECTED, false)
                .with(SearchResultListProperties.CLICK_LISTENER, (view) -> handleResultClick(url))
                .build();
    }
}
