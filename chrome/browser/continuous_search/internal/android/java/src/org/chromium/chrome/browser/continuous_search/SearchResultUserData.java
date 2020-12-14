// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.util.HashSet;

/**
 * Per-tab storage of {@link SearchResultMetadata}.
 */
public class SearchResultUserData implements UserData {
    public static final int INVALID_POSITION = -2;
    public static final int ON_SRP = -1;
    private static final Class<SearchResultUserData> USER_DATA_KEY = SearchResultUserData.class;

    private SearchResultMetadata mData;
    private HashSet<GURL> mValidUrls;
    private HashSet<SearchResultUserDataObserver> mObservers = new HashSet<>();
    private GURL mCurrentUrl;
    private int mCurrentPosition = INVALID_POSITION;

    static void createForTab(Tab tab) {
        assert tab.getUserDataHost().getUserData(USER_DATA_KEY) == null;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new SearchResultUserData());
    }

    static SearchResultUserData getForTab(Tab tab) {
        assert tab.getUserDataHost().getUserData(USER_DATA_KEY) != null;
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private SearchResultUserData() {}

    /**
     * @return Whether this contains valid data.
     */
    boolean isValid() {
        return mData != null;
    }

    void addObserver(SearchResultUserDataObserver observer) {
        mObservers.add(observer);
        if (isValid()) {
            observer.onUpdate(mData, mCurrentUrl);
        }
    }

    void removeObserver(SearchResultUserDataObserver observer) {
        mObservers.remove(observer);
    }

    void updateData(SearchResultMetadata metadata, GURL currentUrl) {
        mData = metadata;
        mValidUrls = new HashSet<>();
        for (int i = 0; i < mData.getGroups().size(); i++) {
            SearchResultGroup group = mData.getGroups().get(i);
            for (int j = 0; j < group.getResults().size(); j++) {
                mValidUrls.add(group.getResults().get(j).getUrl());
            }
        }
        updateCurrentUrlInternal(currentUrl, false);

        for (SearchResultUserDataObserver observer : mObservers) {
            observer.onUpdate(mData, mCurrentUrl);
        }
    }

    void invalidateData() {
        mData = null;
        mValidUrls = null;
        mCurrentUrl = null;
        for (SearchResultUserDataObserver observer : mObservers) {
            observer.onInvalidate();
        }
    }

    boolean isUrlInResults(GURL url) {
        if (!isValid()) return false;

        return mValidUrls.contains(url);
    }

    void updateCurrentUrl(GURL url) {
        updateCurrentUrlInternal(url, true);
    }

    private void updateCurrentUrlInternal(GURL url, boolean notify) {
        if (!isValid()) return;

        mCurrentUrl = url;
        boolean urlInResults = isUrlInResults(mCurrentUrl);
        boolean onSrp = mCurrentUrl.equals(mData.getResultUrl());
        if (!urlInResults && !onSrp) {
            invalidateData();
            return;
        }

        if (!notify) return;

        for (SearchResultUserDataObserver observer : mObservers) {
            observer.onUrlChanged(url);
        }
    }
}
