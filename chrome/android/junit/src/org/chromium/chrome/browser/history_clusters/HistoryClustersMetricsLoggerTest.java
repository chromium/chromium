// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.history_clusters.HistoryClustersMetricsLogger.InitialState;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Unit tests for HistoryClustersMetricsLogger. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryClustersMetricsLoggerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private TemplateUrlService mTemplateUrlService;

    private ClusterVisit mSrpVisit;
    private ClusterVisit mNonSrpVisit;
    private GURL mSrpGurl;
    private GURL mNonSrpGurl;
    private HistoryClustersMetricsLogger mMetricsLogger;

    @Before
    public void setUp() {
        mSrpGurl = JUnitTestGURLs.GOOGLE_URL_CAT;
        mNonSrpGurl = JUnitTestGURLs.EXAMPLE_URL;
        mSrpVisit = new ClusterVisit(1.0F, mSrpGurl, "Title 4", "url3.com/foo", new ArrayList<>(),
                new ArrayList<>(), mSrpGurl, 123L, new ArrayList<>());
        mSrpVisit.setIndexInParent(0);
        mNonSrpVisit = new ClusterVisit(1.0F, mNonSrpGurl, "Title 4", "url3.com/foo",
                new ArrayList<>(), new ArrayList<>(), mNonSrpGurl, 123L, new ArrayList<>());
        mNonSrpVisit.setIndexInParent(1);
        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(true)
                .when(mTemplateUrlService)
                .isSearchResultsPageFromDefaultSearchProvider(mSrpGurl);

        mMetricsLogger = new HistoryClustersMetricsLogger(mTemplateUrlService);
    }

    @Test
    public void testDestroyBeforeInitialStateSet() {
        mMetricsLogger.destroy();
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.InitialState"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.FinalState.NumberLinksOpened"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.FinalState.NumberRelatedSearchesClicked"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.FinalState.NumberVisibilityToggles"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.FinalState.NumberIndividualVisitsDeleted"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.FinalState.TogglesToBasicHistory"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.FinalState.WasSuccessful"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.DidMakeQuery"),
                0);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.NumQueries"),
                0);
    }

    @Test
    public void testSetInitialStateKeepsFirstValue() {
        mMetricsLogger.setInitialState(InitialState.SAME_DOCUMENT);
        mMetricsLogger.setInitialState(InitialState.INDIRECT_NAVIGATION);
        mMetricsLogger.destroy();

        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.InitialState"),
                1);
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.Actions.InitialState", InitialState.SAME_DOCUMENT),
                1);
    }

    @Test
    public void testRelatedSearchesClickNoQueries() {
        mMetricsLogger.setInitialState(InitialState.SAME_DOCUMENT);
        mMetricsLogger.recordRelatedSearchesClick(3);
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.UIActions.RelatedSearch.Clicked", 3),
                1);

        mMetricsLogger.destroy();
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.Actions.InitialState", InitialState.SAME_DOCUMENT),
                1);
        assertEquals(RecordHistogram.getHistogramTotalCountForTesting(
                             "History.Clusters.Actions.FinalState.NumberRelatedSearchesClicked"),
                1);
        assertEquals(
                RecordHistogram.getHistogramValueCountForTesting(
                        "History.Clusters.Actions.DidMakeQuery", /* int equivalent of false */ 0),
                1);
    }

    @Test
    public void testVisitActions() {
        mMetricsLogger.setInitialState(InitialState.INDIRECT_NAVIGATION);
        mMetricsLogger.recordVisitAction(
                HistoryClustersMetricsLogger.VisitAction.DELETED, mSrpVisit);
        mMetricsLogger.recordVisitAction(
                HistoryClustersMetricsLogger.VisitAction.CLICKED, mNonSrpVisit);
        assertEquals(
                RecordHistogram.getHistogramValueCountForTesting(
                        "History.Clusters.UIActions.Visit.Deleted", mSrpVisit.getIndexInParent()),
                1);
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.UIActions.Visit.Clicked",
                             mNonSrpVisit.getIndexInParent()),
                1);
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.UIActions.SRPVisit.Deleted",
                             mSrpVisit.getIndexInParent()),
                1);
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.UIActions.nonSRPVisit.Clicked",
                             mNonSrpVisit.getIndexInParent()),
                1);

        mMetricsLogger.destroy();
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.Actions.FinalState.NumberLinksOpened", 1),
                1);
    }

    @Test
    public void testSuccessfulFinalState() {
        mMetricsLogger.setInitialState(InitialState.INDIRECT_NAVIGATION);
        mMetricsLogger.incrementQueryCount();
        mMetricsLogger.incrementQueryCount();
        mMetricsLogger.recordVisitAction(
                HistoryClustersMetricsLogger.VisitAction.CLICKED, mSrpVisit);

        mMetricsLogger.destroy();
        assertEquals(
                RecordHistogram.getHistogramValueCountForTesting(
                        "History.Clusters.Actions.InitialState", InitialState.INDIRECT_NAVIGATION),
                1);
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.Actions.FinalState.WasSuccessful",
                             /* int equivalent of true */ 1),
                1);
        assertEquals(
                RecordHistogram.getHistogramValueCountForTesting(
                        "History.Clusters.Actions.DidMakeQuery", /* int equivalent of true */ 1),
                1);
        assertEquals(RecordHistogram.getHistogramValueCountForTesting(
                             "History.Clusters.Actions.NumQueries", 2),
                1);
    }
}
