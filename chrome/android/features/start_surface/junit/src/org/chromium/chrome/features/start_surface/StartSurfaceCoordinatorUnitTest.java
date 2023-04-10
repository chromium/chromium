// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/** Tests for {@link StartSurfaceCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.START_SURFACE_ANDROID)
@Features.DisableFeatures({ChromeFeatureList.WEB_FEED, ChromeFeatureList.SHOPPING_LIST})
public class StartSurfaceCoordinatorUnitTest {
    private static final long MILLISECONDS_PER_MINUTE = TimeUtils.SECONDS_PER_MINUTE * 1000;
    private static final String START_SURFACE_TIME_SPENT = "StartSurface.TimeSpent";
    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";
    private static final String USER_ACTION_START_SURFACE_MVT_CLICK =
            "Suggestions.Tile.Tapped.StartSurface";
    private static final String TEST_URL = "https://www.example.com/";

    @Mock
    private Callback mOnVisitComplete;
    @Mock
    private Runnable mOnPageLoaded;

    @Rule
    public StartSurfaceCoordinatorUnitTestRule mTestRule =
            new StartSurfaceCoordinatorUnitTestRule();

    StartSurfaceCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCoordinator = mTestRule.getCoordinator();
        mCoordinator.initWithNative();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testShowAndHideWithRefactorEnabled() {
        assertTrue(ChromeFeatureList.sStartSurfaceRefactor.isEnabled());
        assertNull(mCoordinator.getTasksSurfaceForTesting());
        TabSwitcher tabSwitcherModule =
                mCoordinator.getMediatorForTesting().getTabSwitcherModuleForTesting();
        assertNotNull(tabSwitcherModule);
        assertEquals(TabSwitcherType.SINGLE,
                tabSwitcherModule.getTabListDelegate().getListModeForTesting());
        assertNotNull(mCoordinator.getViewForTesting());

        mCoordinator.showOverview(false);
        assertTrue(mCoordinator.isMVTilesInitializedForTesting());
        assertFalse(mCoordinator.isMVTilesCleanedUpForTesting());
        assertNotNull(mCoordinator.getTileGroupDelegateForTesting());

        mCoordinator.onHide();
        assertTrue(mCoordinator.isMVTilesCleanedUpForTesting());
        assertFalse(mCoordinator.isMVTilesInitializedForTesting());
        assertNull(mCoordinator.getTileGroupDelegateForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.START_SURFACE_REFACTOR)
    public void testCleanUpMVTilesAfterHiding() {
        assertFalse(ChromeFeatureList.sStartSurfaceRefactor.isEnabled());
        assertNotNull(mCoordinator.getTasksSurfaceForTesting());
        assertNull(mCoordinator.getMediatorForTesting().getTabSwitcherModuleForTesting());
        assertNull(mCoordinator.getViewForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);

        Assert.assertFalse(mCoordinator.isMVTilesCleanedUpForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertTrue(mCoordinator.isMVTilesCleanedUpForTesting());
    }

    @Test
    public void testMVTilesInitialized() {
        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        Assert.assertFalse(mCoordinator.isMVTilesInitializedForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        Assert.assertTrue(mCoordinator.isMVTilesInitializedForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertFalse(mCoordinator.isMVTilesInitializedForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertFalse(mCoordinator.isMVTilesInitializedForTesting());
    }

    @Test
    public void testDoNotInitializeSecondaryTasksSurfaceWithoutOpenGridTabSwitcher() {
        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        Assert.assertTrue(mCoordinator.isSecondaryTasksSurfaceEmptyForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertTrue(mCoordinator.isSecondaryTasksSurfaceEmptyForTesting());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        Assert.assertFalse(mCoordinator.isSecondaryTasksSurfaceEmptyForTesting());
    }

    @Test
    @MediumTest
    public void testFeedSwipeLayoutVisibility() {
        assert mCoordinator.getStartSurfaceState() == StartSurfaceState.NOT_SHOWN;
        Assert.assertEquals(
                View.GONE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        Assert.assertEquals(
                View.VISIBLE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertEquals(
                View.GONE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWN_TABSWITCHER);
        mCoordinator.showOverview(false);
        Assert.assertEquals(
                View.VISIBLE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());

        mCoordinator.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);
        mCoordinator.onHide();
        Assert.assertEquals(
                View.GONE, mCoordinator.getFeedSwipeRefreshLayoutForTesting().getVisibility());
    }

    /**
     * Tests the logic of recording time spend in start surface.
     */
    @Test
    public void testRecordTimeSpendInStart() {
        mCoordinator.setStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        mCoordinator.showOverview(false);
        mCoordinator.onHide();
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(START_SURFACE_TIME_SPENT));
    }

    /**
     * Test whether the clicking action on MV tiles in {@link StartSurface} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramMostVisitedItemClick_StartSurface() {
        Tile tileForTest =
                new Tile(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.foo.com"),
                                 TileTitleSource.TITLE_TAG, TileSource.TOP_SITES,
                                 TileSectionType.PERSONALIZED),
                        0);
        TileGroupDelegateImpl tileGroupDelegate = mCoordinator.getTileGroupDelegateForTesting();

        // Test clicking on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when click on MV tiles.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then open in new tab on MV tiles.
        tileGroupDelegate.openMostVisitedItem(
                WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK + " is not recorded "
                        + "correctly when long press then open in new tab on MV tiles.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then open in other window on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.NEW_WINDOW, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " shouldn't be recorded when long press then open in other window "
                        + "on MV tiles.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then download link on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.SAVE_TO_DISK, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when long press then download link "
                        + "on MV tiles.",
                3,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));

        // Test long press then open in Incognito tab on MV tiles.
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.OFF_THE_RECORD, tileForTest);
        Assert.assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK + " is not recorded correctly "
                        + "when long press then open in Incognito tab on MV tiles.",
                4,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES));
    }

    /**
     * Test whether the clicking action on MV tiles in {@link StartSurface} is been recorded
     * as user actions correctly.
     */
    @Test
    @SmallTest
    public void testRecordUserActionMostVisitedItemClick_StartSurface() {
        Tile tileForTest =
                new Tile(new SiteSuggestion("0 TOP_SITES", new GURL("https://www.foo.com"),
                                 TileTitleSource.TITLE_TAG, TileSource.TOP_SITES,
                                 TileSectionType.PERSONALIZED),
                        0);
        TileGroupDelegateImpl tileGroupDelegate = mCoordinator.getTileGroupDelegateForTesting();

        // Test clicking on MV tiles.
        HistogramWatcher mvtClickHistogram = expectMvtClickHistogramRecords(1);
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, tileForTest);
        mvtClickHistogram.assertExpected();

        // Test long press then open in new tab on MV tiles.
        mvtClickHistogram = expectMvtClickHistogramRecords(1);
        tileGroupDelegate.openMostVisitedItem(
                WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
        mvtClickHistogram.assertExpected();

        // Test long press then open in other window on MV tiles.
        mvtClickHistogram = expectMvtClickHistogramRecords(1);
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.NEW_WINDOW, tileForTest);
        mvtClickHistogram.assertExpected();

        // Test long press then download link on MV tiles.
        mvtClickHistogram = expectMvtClickHistogramRecords(1);
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.SAVE_TO_DISK, tileForTest);
        mvtClickHistogram.assertExpected();

        // Test long press then open in Incognito tab on MV tiles.
        mvtClickHistogram = expectMvtClickHistogramRecords(2);
        tileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.OFF_THE_RECORD, tileForTest);
        mvtClickHistogram.assertExpected();
    }

    /**
     * Test whether the clicking action on Feeds in {@link StartSurface} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramFeedClick_StartSurface() {
        FeedActionDelegate feedActionDelegate =
                mCoordinator.getMediatorForTesting().getFeedActionDelegateForTesting();
        // Test click on Feeds or long press then check about this source & topic on Feeds.
        feedActionDelegate.openSuggestionUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK), false, mOnPageLoaded,
                mOnVisitComplete);
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when click on Feeds or "
                        + "long press then check about this source & topic on Feeds.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test long press then open in new tab on Feeds.
        feedActionDelegate.openSuggestionUrl(WindowOpenDisposition.NEW_BACKGROUND_TAB,
                new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK), false, mOnPageLoaded,
                mOnVisitComplete);
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when long press then open in "
                        + "new tab on Feeds.",
                2,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test long press then open in incognito tab on Feeds.
        feedActionDelegate.openSuggestionUrl(WindowOpenDisposition.OFF_THE_RECORD,
                new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK), false, mOnPageLoaded,
                mOnVisitComplete);
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when long press then open in incognito tab "
                        + "on Feeds.",
                3,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test manage activity or manage interests on Feeds.
        feedActionDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(TEST_URL, PageTransition.LINK));
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " shouldn't be recorded when manage activity or manage interests "
                        + "on Feeds.",
                3,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));

        // Test click Learn More button on Feeds.
        feedActionDelegate.openHelpPage();
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when click Learn More button on Feeds.",
                4,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED));
    }

    private static HistogramWatcher expectMvtClickHistogramRecords(int times) {
        return HistogramWatcher.newBuilder()
                .expectAnyRecordTimes(USER_ACTION_START_SURFACE_MVT_CLICK, times)
                .build();
    }
}
