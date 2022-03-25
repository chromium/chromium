// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.NonNull;

import org.chromium.base.Promise;

/**
 * Interface for a provider of history clusters data.
 */
interface HistoryClustersProvider {
    /** Request a fixed number of clusters matching the given query. */
    @NonNull
    Promise<HistoryClustersResult> queryClusters(String query);

    /**
     * Request more clusters matching the most recent query. {@code query} must match that most
     * recent query.
     */
    @NonNull
    Promise<HistoryClustersResult> loadMoreClusters(String query);

    /** Remove all the visits for the given cluster from the history database. */
    @NonNull
    Promise<Void> removeCluster(HistoryCluster clusterToRemove);
}