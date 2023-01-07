// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.VisibleForTesting;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
/**
   Result representing the response for a query of clusters of related visits from {@link
   HistoryClustersBridge}. Should only be used external to the history_clusters package in tests.
 */
public class HistoryClustersResult {
    private final List<HistoryCluster> mClusters;
    private final LinkedHashMap<String, Integer> mLabelCounts;
    private final String mQuery;
    private final boolean mCanLoadMore;
    private final boolean mIsContinuation;

    /** Create a new result with no clusters and an empty query. */
    public static HistoryClustersResult emptyResult() {
        return new HistoryClustersResult(
                Collections.EMPTY_LIST, new LinkedHashMap<>(), "", false, false);
    }

    /**
     * Constructs a new HistoryClustersResult. {@code labelCounts} must be a LinkedHashMap so that
     * order is stable and preserved.
     */
    HistoryClustersResult(List<HistoryCluster> clusters, LinkedHashMap<String, Integer> labelCounts,
            String query, boolean canLoadMore, boolean isContinuation) {
        mClusters = clusters;
        mLabelCounts = labelCounts;
        mQuery = query;
        mCanLoadMore = canLoadMore;
        mIsContinuation = isContinuation;
    }

    public List<HistoryCluster> getClusters() {
        return mClusters;
    }

    public String getQuery() {
        return mQuery;
    }

    public boolean canLoadMore() {
        return mCanLoadMore;
    }

    public Map<String, Integer> getLabelCounts() {
        return mLabelCounts;
    }

    public boolean isContinuation() {
        return mIsContinuation;
    }
}
