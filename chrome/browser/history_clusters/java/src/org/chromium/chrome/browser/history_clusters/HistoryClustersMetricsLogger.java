// Copyright 2022 The Chromium Authors
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

    @IntDef({
        InitialState.UNINITIALIZED,
        InitialState.DIRECT_NAVIGATION,
        InitialState.INDIRECT_NAVIGATION,
        InitialState.SAME_DOCUMENT,
        InitialState.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface InitialState {
        int UNINITIALIZED = -1;
        // The HistoryClusters UI was opened via direct URL, i.e., not opened via any
        // other surface/path such as an omnibox action or other UI surface.
        // Currently unused on Android because it's rare and hard to discern, but retained for
        // completeness and parity with desktop.
        int DIRECT_NAVIGATION = 1;
        // The HistoryClusters UI was opened indirectly; e.g., using an omnibox
        // action.
        int INDIRECT_NAVIGATION = 2;
        // The HistoryClusters UI was opened via a same-document navigation, which
        // means the user likely clicked the tab over from History to Journeys.
        int SAME_DOCUMENT = 3;
        int NUM_ENTRIES = 4;
    }

    private int mVisitDeleteCount;
    private int mLinkOpenCount;
    private int mRelatedSearchesClickCount;
    private int mToggledVisibilityCount;
    private int mQueryCount;
    private int mTogglesToBasicHistoryCount;
    private int mInitialState = InitialState.UNINITIALIZED;
    private final TemplateUrlService mTemplateUrlService;

    HistoryClustersMetricsLogger(TemplateUrlService templateUrlService) {
        mTemplateUrlService = templateUrlService;
    }

    void setInitialState(@InitialState int initialState) {
        if (mInitialState == InitialState.UNINITIALIZED) {
            mInitialState = initialState;
        }
    }

    void incrementToggleCount() {
        if (mInitialState == InitialState.UNINITIALIZED) {
            mInitialState = InitialState.SAME_DOCUMENT;
        }

        mTogglesToBasicHistoryCount++;
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
        @VisitType int visitType = getVisitType(clusterVisit.getNormalizedUrl());
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.UIActions.Visit." + visitActionToString(visitAction),
                clusterVisit.getIndexInParent());
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.UIActions."
                        + visitTypeToString(visitType)
                        + "Visit."
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
        if (mInitialState == InitialState.UNINITIALIZED) {
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "History.Clusters.Actions.InitialState", mInitialState, InitialState.NUM_ENTRIES);
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
        RecordHistogram.recordCount100Histogram(
                "History.Clusters.Actions.FinalState.TogglesToBasicHistory",
                mTogglesToBasicHistoryCount);
        RecordHistogram.recordBooleanHistogram(
                "History.Clusters.Actions.FinalState.WasSuccessful",
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
