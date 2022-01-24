// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
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
    @VisibleForTesting
    boolean mAllowNativeUrlChecks = true;

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
        TraceEvent.begin("ContinuousNavigationUserDataImpl#updateData");
        mData = metadata;
        mValidUrls = new HashSet<>();
        for (int i = 0; i < mData.getGroups().size(); i++) {
            PageGroup group = mData.getGroups().get(i);
            for (int j = 0; j < group.getPageItems().size(); j++) {
                mValidUrls.add(group.getPageItems().get(j).getUrl());
            }
        }
        updateCurrentUrlInternal(currentUrl, false);
        if (mData == null) {
            TraceEvent.end("ContinuousNavigationUserDataImpl#updateData");
            return;
        }

        for (ContinuousNavigationUserDataObserver observer : mObservers) {
            observer.onUpdate(mData);
            observer.onUrlChanged(mCurrentUrl, isMatchingSrp(mCurrentUrl));
        }
        TraceEvent.end("ContinuousNavigationUserDataImpl#updateData");
    }

    void invalidateData() {
        TraceEvent.begin("ContinuousNavigationUserDataImpl#invalidateData");
        mData = null;
        mValidUrls = null;
        mCurrentUrl = null;
        for (ContinuousNavigationUserDataObserver observer : mObservers) {
            observer.onInvalidate();
        }
        TraceEvent.end("ContinuousNavigationUserDataImpl#invalidateData");
    }

    @Nullable
    GURL maybeGetUrlInResults(GURL url) {
        if (!isValid()) return null;

        for (GURL validUrl : mValidUrls) {
            if (equalsValidUrl(validUrl, url)) {
                return validUrl;
            }
        }
        return null;
    }

    @Override
    public void updateCurrentUrl(GURL url) {
        updateCurrentUrlInternal(url, true);
    }

    boolean isMatchingSrp(GURL url) {
        if (mData == null || mData.getQuery() == null || mData.getQuery().isEmpty()) {
            return false;
        }

        String query = SearchUrlHelper.getQueryIfValidSrpUrl(url);
        return query != null && query.equals(mData.getQuery())
                && SearchUrlHelper.getSrpPageCategoryFromUrl(url)
                == mData.getProvider().getCategory();
    }

    private void updateCurrentUrlInternal(GURL url, boolean notify) {
        if (!isValid()) return;
        TraceEvent.begin("ContinuousNavigationUserDataImpl#updateCurrentUrlInternal");

        GURL urlFromResults = maybeGetUrlInResults(url);
        if (urlFromResults == null) {
            mCurrentUrl = url;
        } else {
            mCurrentUrl = urlFromResults;
        }
        boolean onSrp = isMatchingSrp(url);
        if (urlFromResults == null && !onSrp) {
            invalidateData();
            TraceEvent.end("ContinuousNavigationUserDataImpl#updateCurrentUrlInternal");
            return;
        }

        if (!notify) {
            TraceEvent.end("ContinuousNavigationUserDataImpl#updateCurrentUrlInternal");
            return;
        }

        for (ContinuousNavigationUserDataObserver observer : mObservers) {
            observer.onUrlChanged(url, onSrp);
        }
        TraceEvent.end("ContinuousNavigationUserDataImpl#updateCurrentUrlInternal");
    }

    private boolean equalsValidUrl(GURL validUrl, GURL url) {
        // Allow matching the origin and path ignoring query and ref.
        return validUrl.equals(url)
                || (mAllowNativeUrlChecks && validUrl.getOrigin().equals(url.getOrigin())
                        && validUrl.getPath().equals(url.getPath()));
    }

    @VisibleForTesting
    static void setInstanceForTesting(ContinuousNavigationUserDataImpl instance) {
        sInstanceForTesting = instance;
    }
}
