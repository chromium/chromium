// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import java.util.List;

class HistoryClustersResult {
    private final List<HistoryCluster> mClusters;
    private final String mQuery;
    private final boolean mCanLoadMore;
    private final boolean mIsContinuation;

    public HistoryClustersResult(List<HistoryCluster> clusters, String query, boolean canLoadMore,
            boolean isContinuation) {
        mClusters = clusters;
        mQuery = query;
        mCanLoadMore = canLoadMore;
        mIsContinuation = isContinuation;
    }

    public List<HistoryCluster> getClusters() {
        return mClusters;
    }
}
