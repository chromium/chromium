// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class HistoryClustersMetricsLogger {
    @IntDef({VisitAction.CLICKED, VisitAction.DELETED})
    @Retention(RetentionPolicy.SOURCE)
    @interface VisitAction {
        int CLICKED = 0;
        int DELETED = 1;
    }

    @IntDef({VisitType.SRP, VisitType.NON_SRP})
    @Retention(RetentionPolicy.SOURCE)
    @interface VisitType {
        int SRP = 0;
        int NON_SRP = 1;
    }

    // TODO(https://crbug.com/1303171): Add UKM metrics.
    private int mVisitDeleteCount;
    private int mLinkOpenCount;
    private int mRelatedSearchesClickCount;
    private int mToggledVisibilityCount;
    private int mQueryCount;
    private final TemplateUrlService mTemplateUrlService;

    HistoryClustersMetricsLogger(TemplateUrlService templateUrlService) {
        mTemplateUrlService = templateUrlService;
    }

    void incrementQueryCount() {
        mQueryCount++;
    }

    // TODO(https://crbug.com/1303171): Call this when opt-out is implemented.
    void recordToggledVisibility(boolean visible) {
        RecordHistogram.recordBooleanHistogram(
                "History.Clusters.UIActions.ToggledVisiblity", visible);
        mToggledVisibilityCount++;
    }

    void recordVisitAction(@VisitAction int visitAction, ClusterVisit clusterVisit) {
        @VisitType
        int visitType = getVisitType(clusterVisit.getNormalizedUrl());
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.UIActions.Visit." + visitActionToString(visitAction),
                clusterVisit.getIndexInParent());
        RecordHistogram.recordCount100Histogram("History.Clusters.UIActions."
                        + visitTypeToString(visitType) + "Visit."
                        + visitActionToString(visitAction),
                clusterVisit.getIndexInParent());
        switch (visitAction) {
            case VisitAction.CLICKED:
                mLinkOpenCount++;
                break;
            case VisitAction.DELETED:
                mVisitDeleteCount++;
                break;
        }
    }

    private @VisitType int getVisitType(GURL gurl) {
        return mTemplateUrlService.isLoaded()
                        && mTemplateUrlService.isSearchResultsPageFromDefaultSearchProvider(gurl)
                ? HistoryClustersMetricsLogger.VisitType.SRP
                : HistoryClustersMetricsLogger.VisitType.NON_SRP;
    }

    void recordRelatedSearchesClick(int index) {
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.UIActions.RelatedSearch.Clicked", index);
        mRelatedSearchesClickCount++;
    }

    void destroy() {
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.Actions.FinalState.NumberLinksOpened", mLinkOpenCount);
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.Actions.FinalState.NumberRelatedSearchesClicked",
                mRelatedSearchesClickCount);
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.Actions.FinalState.NumberVisibilityToggles",
                mToggledVisibilityCount);
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.Actions.FinalState.NumberIndividualVisitsDeleted",
                mVisitDeleteCount);
        RecordHistogram.recordBooleanHistogram("History.Clusters.Actions.FinalState.WasSuccessful",
                isCurrentlySuccessfulHistoryClustersOutcome());
        RecordHistogram.recordBooleanHistogram(
                "History.Clusters.Actions.DidMakeQuery", mQueryCount > 0);
        if (mQueryCount > 0) {
            RecordHistogram.recordCount100Histogram(
                    "History.Clusters.Actions.NumQueries", mQueryCount);
        }
    }

    private String visitActionToString(@VisitAction int action) {
        switch (action) {
            case VisitAction.DELETED:
                return "Deleted";
            case VisitAction.CLICKED:
                return "Clicked";
        }
        assert false;
        return "";
    }

    private String visitTypeToString(@VisitType int type) {
        switch (type) {
            case VisitType.SRP:
                return "SRP";
            case VisitType.NON_SRP:
                return "nonSRP";
        }
        assert false;
        return "";
    }

    private boolean isCurrentlySuccessfulHistoryClustersOutcome() {
        return (mRelatedSearchesClickCount + mLinkOpenCount + mVisitDeleteCount) > 0;
    }
}
