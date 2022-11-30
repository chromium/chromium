// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.lessThanOrEqualTo;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Tests for the query tiles section on new tab page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class QueryTileUtilsTest {
    private static final int MILLISECONDS_PER_MINUTE = 60 * 1000;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        QueryTileUtils.setSegmentationResultsForTesting(0 /*UNINITIALIZED*/);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP})
    @DisableFeatures(ChromeFeatureList.QUERY_TILES_SEGMENTATION)
    public void testIsQueryTilesEnabledOnNTPWithoutSegmentation() {
        Assert.assertTrue(QueryTileUtils.isQueryTilesEnabledOnNTP());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP,
            ChromeFeatureList.QUERY_TILES_SEGMENTATION + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:mv_tile_click_threshold/0"})
    public void
    testShouldShowQueryTilesWithLowerThreshold() {
        Assert.assertFalse(QueryTileUtils.shouldShowQueryTiles());

        nextDecisionTimeStampInDays(QueryTileUtils.DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD);

        // Existing decision will be used before the next decision time.
        Assert.assertFalse(QueryTileUtils.shouldShowQueryTiles());
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOW_ON_NTP, true);
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());

        // A new decision will be made if the next decision time expires.
        nextDecisionTimeReached();
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());
        // Query tiles will continue to be shown for a period of time.
        queryTilesWillBeShownFromNowOn();

        // Clicking on MV tiles will hide query tiles for a while.
        nextDecisionTimeReached();
        QueryTileUtils.onMostVisitedTileClicked();
        Assert.assertFalse(QueryTileUtils.shouldShowQueryTiles());
        queryTilesWillBeHiddenFromNowOn();

        // Clicking on MV tiles will continue hiding query tiles.
        nextDecisionTimeReached();
        QueryTileUtils.onMostVisitedTileClicked();
        Assert.assertFalse(QueryTileUtils.shouldShowQueryTiles());
        queryTilesWillBeHiddenFromNowOn();

        // Not clicking on MV tiles will allow query tiles to be shown.
        nextDecisionTimeReached();
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());
        queryTilesWillBeShownFromNowOn();

        // Clicking on more query tiles than MV tiles will continue showing query tiles.
        nextDecisionTimeReached();
        QueryTileUtils.onQueryTileClicked();
        QueryTileUtils.onMostVisitedTileClicked();
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());
        queryTilesWillBeShownFromNowOn();
    }

    /**
     * Query tiles should be shown if increasing MV tile click threshold to 1.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP,
            ChromeFeatureList.QUERY_TILES_SEGMENTATION})
    public void
    testShouldShowQueryTilesWithDefaultThreshold() {
        nextDecisionTimeReached();
        QueryTileUtils.onMostVisitedTileClicked();
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());
        queryTilesWillBeShownFromNowOn();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP,
            ChromeFeatureList.QUERY_TILES_SEGMENTATION + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:num_days_keep_showing_query_tiles/10/"
                    + "num_days_mv_clicks_below_threshold/2/mv_tile_click_threshold/0"})
    public void
    testShouldShowQueryTilesWithShorterDisplayDurations() {
        nextDecisionTimeReached();
        QueryTileUtils.onMostVisitedTileClicked();
        Assert.assertFalse(QueryTileUtils.shouldShowQueryTiles());
        nextDecisionTimeStampInDays(2);

        nextDecisionTimeReached();
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());
        nextDecisionTimeStampInDays(10);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP,
            ChromeFeatureList.QUERY_TILES_SEGMENTATION + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:behavioural_targeting/model_comparison/"
                    + "mv_tile_click_threshold/0"})
    public void
    testShowQueryTilesSegmentationResultComparison() {
        QueryTileUtils.setSegmentationResultsForTesting(1 /*DONT_SHOW*/);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.QueryTiles.ShowQueryTilesSegmentationResultComparison"));

        nextDecisionTimeReached();
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());
        Assert.assertEquals(2,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.QueryTiles.ShowQueryTilesSegmentationResultComparison"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.QueryTiles.ShowQueryTilesSegmentationResultComparison",
                        QueryTileUtils.ShowQueryTilesSegmentationResultComparison
                                .SEGMENTATION_DISABLED_LOGIC_ENABLED));

        nextDecisionTimeReached();
        QueryTileUtils.onMostVisitedTileClicked();
        Assert.assertFalse(QueryTileUtils.shouldShowQueryTiles());
        Assert.assertEquals(3,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Search.QueryTiles.ShowQueryTilesSegmentationResultComparison"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.QueryTiles.ShowQueryTilesSegmentationResultComparison",
                        QueryTileUtils.ShowQueryTilesSegmentationResultComparison
                                .SEGMENTATION_DISABLED_LOGIC_DISABLED));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.QUERY_TILES_SEGMENTATION + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:behavioural_targeting/model"})
    public void
    testShouldUseSegmentationModel() {
        // Set segmentation model to show query tiles.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOW_ON_NTP, false);
        QueryTileUtils.setSegmentationResultsForTesting(2 /*SHOW*/);

        // Verify that query tiles is shown via segmentation model when no previous history.
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());

        // Verify that query tiles is shown via segmentation model when previous decision time
        // expired.
        nextDecisionTimeReached();
        Assert.assertTrue(QueryTileUtils.shouldShowQueryTiles());

        // Verify that segmentation is not used when previous decision time did not expire.
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS,
                System.currentTimeMillis() + QueryTileUtils.MILLISECONDS_PER_DAY);
        Assert.assertFalse(QueryTileUtils.shouldShowQueryTiles());
    }

    /**
     * Check that the next decision time is within |numOfDays| from now.
     * @param numOfDays Number of days to check.
     */
    private void nextDecisionTimeStampInDays(int numOfDays) {
        long approximateTime =
                System.currentTimeMillis() + numOfDays * QueryTileUtils.MILLISECONDS_PER_DAY;
        long nextDecisionTime = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS, 0);

        assertThat("new decision time lower bound", approximateTime - MILLISECONDS_PER_MINUTE,
                lessThanOrEqualTo(nextDecisionTime));

        assertThat("new decision time upper bound", approximateTime + MILLISECONDS_PER_MINUTE,
                greaterThanOrEqualTo(nextDecisionTime));
    }

    /**
     * Helper method to simulate that the next decision time has reached.
     */
    void nextDecisionTimeReached() {
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS,
                System.currentTimeMillis() - MILLISECONDS_PER_MINUTE);
    }

    /**
     * Helper method to check that query tiles will be shown from now on for a period of time.
     */
    void queryTilesWillBeShownFromNowOn() {
        nextDecisionTimeStampInDays(QueryTileUtils.DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES);
        Assert.assertTrue(SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOW_ON_NTP, false));
    }

    /**
     * Helper method to check that query tiles will be hidden from now on for a period of time.
     */
    void queryTilesWillBeHiddenFromNowOn() {
        nextDecisionTimeStampInDays(QueryTileUtils.DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD);
        Assert.assertFalse(SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOW_ON_NTP, false));
    }
}
