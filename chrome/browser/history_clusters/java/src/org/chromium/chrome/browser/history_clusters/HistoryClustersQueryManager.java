// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.NonNull;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Implementation of {@link HistoryClustersProvider} that encapsulates a HistoryClustersBridge,
 * providing a safe async interface that insulates callers from the bridge's lifecycle.
 */
class HistoryClustersQueryManager implements HistoryClustersProvider {
    private final HistoryClustersBridge mBridge;
    private String mQuery;
    private Promise<HistoryClustersResult> mPromise;
    private OneShotCallback<Profile> mBridgeCreationCallback;

    /**
     * Construct a HistoryClustersQueryManager.
     * @param profile The profile from which the coordinator should access history data.
     */
    HistoryClustersQueryManager(Profile profile) {
        mBridge = new HistoryClustersBridge(profile);
    }

    /** Destroys the query manager and underlying bridge. */
    void destroy() {
        mBridge.destroy();
    }

    @Override
    public Promise<HistoryClustersResult> queryClusters(String query) {
        if (mPromise != null && !mPromise.isFulfilled()) {
            mPromise.reject();
        }
        mPromise = new Promise<>();
        mQuery = query;
        if (mBridge != null) {
            mBridge.queryClusters(mQuery, mPromise::fulfill);
        }
        return mPromise;
    }

    @Override
    public Promise<HistoryClustersResult> loadMoreClusters(String query) {
        assert mQuery != null;
        assert mQuery.equals(query);
        assert mPromise.isFulfilled();
        mPromise = new Promise<>();
        mBridge.loadMoreClusters(mQuery, mPromise::fulfill);
        return mPromise;
    }

    @NonNull
    @Override
    public Promise<Void> removeCluster(HistoryCluster clusterToRemove) {
        return new Promise<>();
    }
}
