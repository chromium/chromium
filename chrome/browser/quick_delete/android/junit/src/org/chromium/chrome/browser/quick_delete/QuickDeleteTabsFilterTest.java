// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.quick_delete.QuickDeleteTabsFilter.FIFTEEN_MINUTES_IN_MS;
import static org.chromium.chrome.browser.quick_delete.QuickDeleteTabsFilter.FOUR_WEEKS_IN_MS;
import static org.chromium.chrome.browser.quick_delete.QuickDeleteTabsFilter.ONE_WEEK_IN_MS;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import java.util.ArrayList;
import java.util.List;

/** Robolectric tests for {@link QuickDeleteTabsFilter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class QuickDeleteTabsFilterTest {
    private static final long INITIAL_TIME_IN_MS = 1000;

    private QuickDeleteTabsFilter mQuickDeleteTabsFilter;
    private final List<MockTab> mMockTabList = new ArrayList<>();

    @Mock private TabGroupModelFilter mTabGroupModelFilterMock;
    @Mock private TabModel mTabModelMock;
    @Mock private Profile mProfileMock;

    private void createTabsAndUpdateTabModel(int countOfTabs) {
        // Create tabs.
        for (int id = 0; id < countOfTabs; id++) {
            MockTab mockTab = new MockTab(id, mProfileMock);
            mockTab.setRootId(id);
            mMockTabList.add(mockTab);
        }
        // Update the tab model.
        doReturn(countOfTabs).when(mTabModelMock).getCount();
        for (int i = 0; i < countOfTabs; i++) {
            when(mTabModelMock.getTabAt(i)).thenReturn(mMockTabList.get(i));
        }
    }

    @Before
    public void setUp() {
        initMocks(this);
        doReturn(false).when(mTabGroupModelFilterMock).isIncognito();
        doReturn(false).when(mTabModelMock).isIncognito();
        doReturn(mTabModelMock).when(mTabGroupModelFilterMock).getTabModel();
        mQuickDeleteTabsFilter = new QuickDeleteTabsFilter(mTabGroupModelFilterMock);

        doReturn(true).when(mTabGroupModelFilterMock).closeTabs(any());
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testIncognitoTabModel_ThrowsAssertionError() {
        doReturn(true).when(mTabGroupModelFilterMock).isIncognito();
        mQuickDeleteTabsFilter = new QuickDeleteTabsFilter(mTabGroupModelFilterMock);
    }

    @Test
    @SmallTest
    public void testAddOneTabOutside15MinutesRange() {
        createTabsAndUpdateTabModel(1);
        mMockTabList.get(0).setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS);

        // 15 minutes passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.LAST_15_MINUTES);
        List<Tab> filteredTabs = mQuickDeleteTabsFilter.getListOfTabsFilteredToBeClosed();
        assertTrue(filteredTabs.isEmpty());
    }

    @Test
    @SmallTest
    public void testAddOneTabWithin15MinutesRange() {
        createTabsAndUpdateTabModel(1);
        mMockTabList.get(0).setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);

        // 15 minutes passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.LAST_15_MINUTES);
        List<Tab> filteredTabs = mQuickDeleteTabsFilter.getListOfTabsFilteredToBeClosed();
        assertEquals(1, filteredTabs.size());
        assertEquals(mMockTabList.get(0), filteredTabs.get(0));
    }

    @Test
    @SmallTest
    public void testCustomTab_NotConsideredInFlow() {
        createTabsAndUpdateTabModel(1);
        // Mock the tab as custom tab.
        mMockTabList.get(0).setIsCustomTab(true);
        // Make the custom tab in the right period for consideration for quick delete.
        mMockTabList.get(0).setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);

        // 15 minutes passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.LAST_15_MINUTES);
        List<Tab> filteredTabs = mQuickDeleteTabsFilter.getListOfTabsFilteredToBeClosed();
        assertTrue(filteredTabs.isEmpty());
    }

    @Test
    @SmallTest
    public void testAddOneTabWithinAndOneOutside15MinutesRange() {
        createTabsAndUpdateTabModel(2);

        // Tab_0: Outside 15 minutes range.
        mMockTabList.get(0).setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS);

        // 15 minutes passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        // Tab_1: Within 15 minutes range.
        mMockTabList
                .get(1)
                .setLastNavigationCommittedTimestampMillis(
                        INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.LAST_15_MINUTES);
        List<Tab> filteredTabs = mQuickDeleteTabsFilter.getListOfTabsFilteredToBeClosed();
        assertEquals(1, filteredTabs.size());
        assertEquals(mMockTabList.get(1), filteredTabs.get(0));
    }

    @Test
    @SmallTest
    public void testClosureOfFilteredTabs_ClosesTabsFromTabModel() {
        // Test close tabs behaviour.
        createTabsAndUpdateTabModel(2);

        // Tab 1
        mMockTabList.get(0).setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);
        // Tab 2
        mMockTabList.get(1).setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 20);

        // 15 minutes passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        // Initiate quick delete tabs closure.
        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.LAST_15_MINUTES);
        List<Tab> filteredTabs = mQuickDeleteTabsFilter.getListOfTabsFilteredToBeClosed();

        mQuickDeleteTabsFilter.closeTabsFilteredForQuickDelete();
        TabClosureParams params =
                TabClosureParams.closeTabs(filteredTabs)
                        .allowUndo(false)
                        .saveToTabRestoreService(false)
                        .build();
        verify(mTabGroupModelFilterMock).closeTabs(params);
    }

    @Test(expected = IllegalStateException.class)
    @SmallTest
    public void testUnsupportedTimeRange_ThrowsException() {
        createTabsAndUpdateTabModel(1);
        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.OLDER_THAN_30_DAYS);
    }

    @Test
    @SmallTest
    public void testClosureOfFilteredTabs_ClosesTabsFromTabModel_AllTime() {
        final int countOfTabs = 5;
        createTabsAndUpdateTabModel(countOfTabs);

        // Set these mock tabs 1 week apart, total of 5 weeks.
        for (int i = 0; i < countOfTabs; ++i) {
            mMockTabList
                    .get(i)
                    .setLastNavigationCommittedTimestampMillis(
                            INITIAL_TIME_IN_MS + ONE_WEEK_IN_MS * (i + 1));
        }

        // 5 weeks passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(
                INITIAL_TIME_IN_MS + FOUR_WEEKS_IN_MS + ONE_WEEK_IN_MS);

        // Initiate quick delete tabs closure.
        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.ALL_TIME);
        List<Tab> filteredTabs = mQuickDeleteTabsFilter.getListOfTabsFilteredToBeClosed();
        assertEquals(5, filteredTabs.size());

        mQuickDeleteTabsFilter.closeTabsFilteredForQuickDelete();
        TabClosureParams params =
                TabClosureParams.closeTabs(filteredTabs)
                        .allowUndo(false)
                        .saveToTabRestoreService(false)
                        .build();
        verify(mTabGroupModelFilterMock).closeTabs(params);
    }

    @Test
    @SmallTest
    public void testClosureOfFilteredTabs_ClosesFourWeeksOldTabsOnlyFromTabModel_FiveWeeks() {
        final int countOfTabs = 5;
        createTabsAndUpdateTabModel(countOfTabs);

        // Set these mock tabs 1 week apart, total of 5 weeks.
        for (int i = 0; i < countOfTabs; ++i) {
            mMockTabList
                    .get(i)
                    .setLastNavigationCommittedTimestampMillis(
                            INITIAL_TIME_IN_MS + ONE_WEEK_IN_MS * (i + 1));
        }

        // 5 weeks passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(
                INITIAL_TIME_IN_MS + FOUR_WEEKS_IN_MS + ONE_WEEK_IN_MS);

        // Initiate quick delete tabs closure but for last four weeks only.
        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.FOUR_WEEKS);
        List<Tab> filteredTabs = mQuickDeleteTabsFilter.getListOfTabsFilteredToBeClosed();
        assertEquals(countOfTabs - 1, filteredTabs.size());
        // The oldest tab created in the first week should not be filtered out.
        assertFalse(filteredTabs.contains(mMockTabList.get(0)));

        mQuickDeleteTabsFilter.closeTabsFilteredForQuickDelete();
        TabClosureParams params =
                TabClosureParams.closeTabs(filteredTabs)
                        .allowUndo(false)
                        .saveToTabRestoreService(false)
                        .build();
        verify(mTabGroupModelFilterMock).closeTabs(params);
    }
}
