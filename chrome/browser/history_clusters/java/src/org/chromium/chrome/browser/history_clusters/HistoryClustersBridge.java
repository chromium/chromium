// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

@JNINamespace("history_clusters")
/** JNI bridge that provides access to HistoryClusters data. */
class HistoryClustersBridge {
    private long mNativeBridge;

    /** Access the instance of HistoryClustersBridge associated with the given profile. */
    public static HistoryClustersBridge getForProfile(Profile profile) {
        return HistoryClustersBridgeJni.get().getForProfile(profile);
    }

    /* Start a new query for clusters, fetching the first page of results. */
    Promise<HistoryClustersResult> queryClusters(String query) {
        Promise<HistoryClustersResult> returnedPromise = new Promise<>();
        HistoryClustersBridgeJni.get().queryClusters(
                mNativeBridge, this, query, returnedPromise::fulfill);
        return returnedPromise;
    }

    /* Continue the current query for clusters, fetching the next page of results. */
    Promise<HistoryClustersResult> loadMoreClusters(String query) {
        Promise<HistoryClustersResult> returnedPromise = new Promise<>();
        HistoryClustersBridgeJni.get().loadMoreClusters(
                mNativeBridge, this, query, returnedPromise::fulfill);
        return returnedPromise;
    }

    /* Constructs a new HistoryClustersBridge. */
    private HistoryClustersBridge(long nativeBridgePointer) {
        mNativeBridge = nativeBridgePointer;
    }

    @CalledByNative
    private static HistoryClustersBridge create(long nativeBridgePointer) {
        return new HistoryClustersBridge(nativeBridgePointer);
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
    static ClusterVisit buildClusterVisit(float score, GURL url, String title) {
        return new ClusterVisit(score, url, title);
    }

    @NativeMethods
    interface Natives {
        HistoryClustersBridge getForProfile(Profile profile);
        void queryClusters(long nativeHistoryClustersBridge, HistoryClustersBridge caller,
                String query, Callback<HistoryClustersResult> callback);
        void loadMoreClusters(long nativeHistoryClustersBridge, HistoryClustersBridge caller,
                String query, Callback<HistoryClustersResult> callback);
    }
}
