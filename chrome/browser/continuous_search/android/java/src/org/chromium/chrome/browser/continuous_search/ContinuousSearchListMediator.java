// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Business logic for the UI component of Continuous Search Navigation. This class updates the UI on
 * search result updates.
 */
class ContinuousSearchListMediator implements SearchResultUserDataObserver {
    private final ModelList mModelList;
    private final Callback<Boolean> mSetLayoutVisibility;
    private Tab mCurrentTab;
    private SearchResultUserData mCurrentSearchUserData;

    ContinuousSearchListMediator(ModelList modelList, Callback<Boolean> setLayoutVisibility) {
        mModelList = modelList;
        mSetLayoutVisibility = setLayoutVisibility;
    }

    private void handleResultClick(GURL url) {
        if (url == null || mCurrentTab == null) return;

        LoadUrlParams params = new LoadUrlParams(url.getSpec());
        params.setReferrer(new Referrer("https://www.google.com", ReferrerPolicy.STRICT_ORIGIN));
        mCurrentTab.loadUrl(params);
    }

    void onObserverNewTab(Tab tab) {
        mSetLayoutVisibility.onResult(false);
        if (mCurrentSearchUserData != null) {
            mCurrentSearchUserData.removeObserver(this);
        }
        mCurrentSearchUserData = SearchResultUserData.getForTab(tab);
        if (mCurrentSearchUserData != null) {
            mCurrentSearchUserData.addObserver(this);
        }
        mCurrentTab = tab;
    }

    @Override
    public void onInvalidate() {
        mModelList.clear();
        mSetLayoutVisibility.onResult(false);
    }

    @Override
    public void onUpdate(SearchResultMetadata metadata, GURL currentUrl) {
        mModelList.clear();

        for (SearchResultGroup group : metadata.getGroups()) {
            if (!group.isAdGroup()) {
                mModelList.add(new ListItem(
                        ListItemType.GROUP_LABEL, generateListItem(group.getLabel(), null)));
            }
            int itemType = group.isAdGroup() ? ListItemType.AD : ListItemType.SEARCH_RESULT;
            for (SearchResult result : group.getResults()) {
                mModelList.add(new ListItem(
                        itemType, generateListItem(result.getTitle(), result.getUrl())));
            }
        }
        mSetLayoutVisibility.onResult(mModelList.size() > 0);
    }

    @Override
    public void onUrlChanged(GURL currentUrl) {
        for (ListItem listItem : mModelList) {
            if (listItem.type == ListItemType.GROUP_LABEL) continue;

            boolean isSelected = currentUrl != null
                    && currentUrl.equals(listItem.model.get(ContinuousSearchListProperties.URL));
            listItem.model.set(ContinuousSearchListProperties.IS_SELECTED, isSelected);
        }
    }

    private PropertyModel generateListItem(String text, GURL url) {
        return new PropertyModel.Builder(ContinuousSearchListProperties.ALL_KEYS)
                .with(ContinuousSearchListProperties.LABEL, text)
                .with(ContinuousSearchListProperties.URL, url)
                .with(ContinuousSearchListProperties.IS_SELECTED, false)
                .with(ContinuousSearchListProperties.CLICK_LISTENER,
                        (view) -> handleResultClick(url))
                .build();
    }

    void destroy() {
        if (mCurrentSearchUserData != null) mCurrentSearchUserData.removeObserver(this);
    }
}
