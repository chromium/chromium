// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.util.HashSet;

/**
 * Implementation of the interface {@link ContinuousNavigationUserData}.
 */
public class ContinuousNavigationUserDataImpl extends ContinuousNavigationUserData {
    public static final int INVALID_POSITION = -2;
    public static final int ON_SRP = -1;

    private ContinuousNavigationMetadata mData;
    private HashSet<GURL> mValidUrls;
    private HashSet<ContinuousNavigationUserDataObserver> mObservers = new HashSet<>();
    private GURL mCurrentUrl;
    private int mCurrentPosition = INVALID_POSITION;

    private static ContinuousNavigationUserDataImpl sInstanceForTesting;

    static ContinuousNavigationUserDataImpl getOrCreateForTab(Tab tab) {
        if (sInstanceForTesting != null) return sInstanceForTesting;

        ContinuousNavigationUserData userData = getForTab(tab);
        if (userData == null) {
            userData = new ContinuousNavigationUserDataImpl();
            setForTab(tab, userData);
        }
        return (ContinuousNavigationUserDataImpl) userData;
    }

    private ContinuousNavigationUserDataImpl() {}

    /**
     * @return Whether this contains valid data.
     */
    boolean isValid() {
        return mData != null;
    }

    void addObserver(ContinuousNavigationUserDataObserver observer) {
        mObservers.add(observer);
        if (isValid()) {
            observer.onUpdate(mData);
            observer.onUrlChanged(mCurrentUrl, isMatchingSrp(mCurrentUrl));
        }
    }

    void removeObserver(ContinuousNavigationUserDataObserver observer) {
        mObservers.remove(observer);
    }

    @Override
    public void updateData(ContinuousNavigationMetadata metadata, GURL currentUrl) {
        mData = metadata;
        mValidUrls = new HashSet<>();
        for (int i = 0; i < mData.getGroups().size(); i++) {
            PageGroup group = mData.getGroups().get(i);
            for (int j = 0; j < group.getPageItems().size(); j++) {
                mValidUrls.add(group.getPageItems().get(j).getUrl());
            }
        }
        updateCurrentUrlInternal(currentUrl, false);
        if (mData == null) return;

        for (ContinuousNavigationUserDataObserver observer : mObservers) {
            observer.onUpdate(mData);
            observer.onUrlChanged(mCurrentUrl, isMatchingSrp(mCurrentUrl));
        }
    }

    void invalidateData() {
        mData = null;
        mValidUrls = null;
        mCurrentUrl = null;
        for (ContinuousNavigationUserDataObserver observer : mObservers) {
            observer.onInvalidate();
        }
    }

    @Nullable
    GURL maybeGetUrlInResults(GURL url) {
        if (!isValid()) return null;

        for (GURL validUrl : mValidUrls) {
            // Match the origin and path ignoring query and ref.
            if (validUrl.getOrigin().equals(url.getOrigin())
                    && validUrl.getPath().equals(url.getPath())) {
                return validUrl;
            }
        }
        return null;
    }

    void updateCurrentUrl(GURL url) {
        updateCurrentUrlInternal(url, true);
    }

    private void updateCurrentUrlInternal(GURL url, boolean notify) {
        if (!isValid()) return;

        GURL urlFromResults = maybeGetUrlInResults(url);
        if (urlFromResults == null) {
            mCurrentUrl = url;
        } else {
            mCurrentUrl = urlFromResults;
        }
        boolean onSrp = isMatchingSrp(url);
        if (urlFromResults == null && !onSrp) {
            invalidateData();
            return;
        }

        if (!notify) return;

        for (ContinuousNavigationUserDataObserver observer : mObservers) {
            observer.onUrlChanged(url, onSrp);
        }
    }

    private boolean isMatchingSrp(GURL url) {
        if (mData.getQuery() == null || mData.getQuery().isEmpty()) return false;

        String query = SearchUrlHelper.getQueryIfValidSrpUrl(url);
        return query != null && query.equals(mData.getQuery())
                && SearchUrlHelper.getSrpPageCategoryFromUrl(url) == mData.getCategory();
    }

    @VisibleForTesting
    static void setInstanceForTesting(ContinuousNavigationUserDataImpl instance) {
        sInstanceForTesting = instance;
    }
}
