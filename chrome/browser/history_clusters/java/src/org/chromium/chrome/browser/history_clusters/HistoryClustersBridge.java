// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.history_clusters.HistoryCluster.MatchPosition;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;

@JNINamespace("history_clusters")
/** JNI bridge that provides access to HistoryClusters data. */
public class HistoryClustersBridge {
    static HistoryClustersBridge sInstanceForTesting;
    private long mNativeBridge;

    /** Access the instance of HistoryClustersBridge associated with the given profile. */
    public static HistoryClustersBridge getForProfile(Profile profile) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }

        return HistoryClustersBridgeJni.get().getForProfile(profile);
    }

    @VisibleForTesting
    /** Sets a static singleton instance of the bridge for testing purposes. */
    public static void setInstanceForTesting(HistoryClustersBridge historyClustersBridge) {
        sInstanceForTesting = historyClustersBridge;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    /* Start a new query for clusters, fetching the first page of results. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public Promise<HistoryClustersResult> queryClusters(String query) {
        Promise<HistoryClustersResult> returnedPromise = new Promise<>();
        HistoryClustersBridgeJni.get().queryClusters(mNativeBridge, this, query,
                (HistoryClustersResult result) -> fulfillIfNotRejected(returnedPromise, result));
        return returnedPromise;
    }

    /* Continue the current query for clusters, fetching the next page of results. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public Promise<HistoryClustersResult> loadMoreClusters(String query) {
        Promise<HistoryClustersResult> returnedPromise = new Promise<>();
        HistoryClustersBridgeJni.get().loadMoreClusters(mNativeBridge, this, query,
                (HistoryClustersResult result) -> fulfillIfNotRejected(returnedPromise, result));
        return returnedPromise;
    }

    /* Constructs a new HistoryClustersBridge. */
    private HistoryClustersBridge(long nativeBridgePointer) {
        mNativeBridge = nativeBridgePointer;
    }

    private static void fulfillIfNotRejected(
            Promise<HistoryClustersResult> returnedPromise, HistoryClustersResult result) {
        if (returnedPromise.isRejected()) return;
        returnedPromise.fulfill(result);
    }

    @CalledByNative
    private static HistoryClustersBridge create(long nativeBridgePointer) {
        return new HistoryClustersBridge(nativeBridgePointer);
    }

    @CalledByNative
    static HistoryClustersResult buildClusterResult(HistoryCluster[] clusters,
            String[] uniqueRawLabels, int[] labelCounts, String query, boolean canLoadMore,
            boolean isContinuation) {
        assert uniqueRawLabels.length == labelCounts.length;
        LinkedHashMap<String, Integer> labelCountsMap = new LinkedHashMap<>();
        for (int i = 0; i < uniqueRawLabels.length; i++) {
            labelCountsMap.put(uniqueRawLabels[i], labelCounts[i]);
        }

        List<HistoryCluster> clustersList = Arrays.asList(clusters);
        return new HistoryClustersResult(
                clustersList, labelCountsMap, query, canLoadMore, isContinuation);
    }

    @CalledByNative
    static HistoryCluster buildCluster(ClusterVisit[] visits, String label, String rawLabel,
            int[] labelMatchStarts, int[] labelMatchEnds, long timestamp,
            String[] relatedSearches) {
        List<ClusterVisit> clusterVisitList = Arrays.asList(visits);

        assert labelMatchEnds.length == labelMatchStarts.length;
        List<MatchPosition> matchPositions = new ArrayList<>(labelMatchStarts.length);
        for (int i = 0; i < labelMatchStarts.length; i++) {
            MatchPosition matchPosition = new MatchPosition(labelMatchStarts[i], labelMatchEnds[i]);
            matchPositions.add(matchPosition);
        }

        List<String> relatedSearchesList = Arrays.asList(relatedSearches);
        return new HistoryCluster(
                clusterVisitList, label, rawLabel, matchPositions, timestamp, relatedSearchesList);
    }

    @CalledByNative
    static ClusterVisit buildClusterVisit(float score, GURL normalizedUrl, String urlForDisplay,
            String title, int[] titleMatchStarts, int[] titleMatchEnds, int[] urlMatchStarts,
            int[] urlMatchEnds, GURL rawUrl, long timestamp, long[] duplicateVisitTimestamps,
            GURL[] duplicateVisitUrls) {
        assert titleMatchStarts.length == titleMatchEnds.length;
        assert urlMatchStarts.length == urlMatchEnds.length;
        assert duplicateVisitTimestamps.length == duplicateVisitUrls.length;

        List<MatchPosition> titleMatchPositions = new ArrayList<>(titleMatchStarts.length);
        for (int i = 0; i < titleMatchStarts.length; i++) {
            MatchPosition matchPosition = new MatchPosition(titleMatchStarts[i], titleMatchEnds[i]);
            titleMatchPositions.add(matchPosition);
        }

        List<MatchPosition> urlMatchPositions = new ArrayList<>(urlMatchStarts.length);
        for (int i = 0; i < urlMatchStarts.length; i++) {
            MatchPosition matchPosition = new MatchPosition(urlMatchStarts[i], urlMatchEnds[i]);
            urlMatchPositions.add(matchPosition);
        }

        List<ClusterVisit.DuplicateVisit> duplicateVisits =
                new ArrayList<>(duplicateVisitTimestamps.length);
        for (int i = 0; i < duplicateVisitTimestamps.length; i++) {
            duplicateVisits.add(new ClusterVisit.DuplicateVisit(
                    duplicateVisitTimestamps[i], duplicateVisitUrls[i]));
        }

        return new ClusterVisit(score, normalizedUrl, title, urlForDisplay, titleMatchPositions,
                urlMatchPositions, rawUrl, timestamp, duplicateVisits);
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
