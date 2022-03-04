// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

@JNINamespace("history_clusters")
/** JNI bridge that provides access to HistoryClusters data. */
public class HistoryClustersBridge {
    private long mNativeBridge;

    /* Construct a new HistoryClustersBridge. */
    public HistoryClustersBridge(Profile profile) {
        mNativeBridge = HistoryClustersBridgeJni.get().init(profile);
    }

    /* Start a new query for clusters, fetching the first page of results. */
    void queryClusters(String query, Callback<HistoryClustersResult> callback) {
        HistoryClustersBridgeJni.get().queryClusters(mNativeBridge, this, query, callback);
    }

    /* Continue the current query for clusters, fetching the next page of results. */
    void loadMoreClusters(String query, Callback<HistoryClustersResult> callback) {
        HistoryClustersBridgeJni.get().loadMoreClusters(mNativeBridge, this, query, callback);
    }

    @CalledByNative
    static HistoryClustersResult buildClusterResult(
            HistoryCluster[] clusters, String query, boolean canLoadMore, boolean isContinuation) {
        List<HistoryCluster> clustersList = Arrays.asList(clusters);
        return new HistoryClustersResult(clustersList, query, canLoadMore, isContinuation);
    }

    @CalledByNative
    static HistoryCluster buildCluster(ClusterVisit[] visits, String[] keywords) {
        List<String> keywordList = Arrays.asList(keywords);
        List<ClusterVisit> clusterVisitList = Arrays.asList(visits);
        return new HistoryCluster(keywordList, clusterVisitList);
    }

    @CalledByNative
    static ClusterVisit buildClusterVisit(float score, GURL url) {
        return new ClusterVisit(score, url);
    }

    @NativeMethods
    interface Natives {
        long init(Profile profile);
        void queryClusters(long nativeHistoryClustersBridge, HistoryClustersBridge caller,
                String query, Callback<HistoryClustersResult> callback);
        void loadMoreClusters(long nativeHistoryClustersBridge, HistoryClustersBridge caller,
                String query, Callback<HistoryClustersResult> callback);
    }
}
