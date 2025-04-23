// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.quick_delete.QuickDeleteTabsFilter.FIFTEEN_MINUTES_IN_MS;
import static org.chromium.chrome.browser.quick_delete.QuickDeleteTabsFilter.FOUR_WEEKS_IN_MS;
import static org.chromium.chrome.browser.quick_delete.QuickDeleteTabsFilter.ONE_WEEK_IN_MS;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;

/** Robolectric tests for {@link QuickDeleteTabsFilter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class QuickDeleteTabsFilterTest {
    private static final long INITIAL_TIME_IN_MS = 1000;
    private static final Token TAB_GROUP_ID = new Token(3748L, 3483L);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID = new LocalTabGroupId(TAB_GROUP_ID);

    private QuickDeleteTabsFilter mQuickDeleteTabsFilter;
    private final List<MockTab> mMockTabList = new ArrayList<>();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilterMock;
    @Mock private TabModel mTabModelMock;
    @Mock private TabList mComprehensiveModel;
    @Mock private TabRemover mTabRemoverMock;
    @Mock private Profile mProfileMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    private void createTabsAndUpdateTabModel(int countOfTabs) {
        // Create tabs.
        for (int id = 0; id < countOfTabs; id++) {
            MockTab mockTab = new MockTab(id, mProfileMock);
            mockTab.setRootId(id);
            mMockTabList.add(mockTab);
        }
        // Update the tab model.
        when(mTabModelMock.getCount()).thenReturn(countOfTabs);
        when(mComprehensiveModel.getCount()).thenReturn(countOfTabs);
        for (int i = 0; i < countOfTabs; i++) {
            when(mTabModelMock.getTabAt(i)).thenReturn(mMockTabList.get(i));
            when(mComprehensiveModel.getTabAt(i)).thenReturn(mMockTabList.get(i));
        }
    }

    @Before
    public void setUp() {
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);

        doReturn(false).when(mTabModelMock).isIncognito();
        doReturn(mTabModelMock).when(mTabGroupModelFilterMock).getTabModel();
        when(mTabModelMock.getTabRemover()).thenReturn(mTabRemoverMock);
        when(mTabModelMock.getComprehensiveModel()).thenReturn(mComprehensiveModel);
        when(mTabModelMock.getProfile()).thenReturn(mProfileMock);
        mQuickDeleteTabsFilter = new QuickDeleteTabsFilter(mTabGroupModelFilterMock);
    }

    @Test(expected = AssertionError.class)
    public void testIncognitoTabModel_ThrowsAssertionError() {
        doReturn(true).when(mTabModelMock).isIncognito();
        mQuickDeleteTabsFilter = new QuickDeleteTabsFilter(mTabGroupModelFilterMock);
    }

    @Test
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
        verify(mTabRemoverMock).closeTabs(params, false);
    }

    @Test(expected = IllegalStateException.class)
    public void testUnsupportedTimeRange_ThrowsException() {
        createTabsAndUpdateTabModel(1);
        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.OLDER_THAN_30_DAYS);
    }

    @Test
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
        verify(mTabRemoverMock).closeTabs(params, false);
    }

    @Test
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
        verify(mTabRemoverMock).closeTabs(params, false);
    }

    @Test
    public void testGetListOfTabsFilteredToBeClosedExcludingPlaceholderTabGroups() {
        createTabsAndUpdateTabModel(3);
        MockTab tab0 = mMockTabList.get(0);
        tab0.setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);
        MockTab tab1 = mMockTabList.get(1);
        tab1.setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);
        MockTab tab2 = mMockTabList.get(2);
        tab2.setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);
        createSavedTabGroup(List.of(tab0, tab1), /* isCollaboration= */ true);

        // 15 minutes passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.LAST_15_MINUTES);
        List<Tab> filteredTabs =
                mQuickDeleteTabsFilter
                        .getListOfTabsFilteredToBeClosedExcludingPlaceholderTabGroups();
        assertEquals(1, filteredTabs.size());
        assertEquals(tab2, filteredTabs.get(0));
    }

    @Test
    public void testGetListOfTabsFilteredToBeClosedExcludingPlaceholderTabGroups_NoPlaceholder() {
        createTabsAndUpdateTabModel(3);
        MockTab tab0 = mMockTabList.get(0);
        tab0.setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);
        MockTab tab1 = mMockTabList.get(1);
        tab1.setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);
        MockTab tab2 = mMockTabList.get(2);
        tab2.setLastNavigationCommittedTimestampMillis(INITIAL_TIME_IN_MS + 10);
        createSavedTabGroup(List.of(tab0, tab1), /* isCollaboration= */ false);

        // 15 minutes passes...
        mQuickDeleteTabsFilter.setCurrentTimeForTesting(INITIAL_TIME_IN_MS + FIFTEEN_MINUTES_IN_MS);

        mQuickDeleteTabsFilter.prepareListOfTabsToBeClosed(TimePeriod.LAST_15_MINUTES);
        List<Tab> filteredTabs =
                mQuickDeleteTabsFilter
                        .getListOfTabsFilteredToBeClosedExcludingPlaceholderTabGroups();
        assertEquals(3, filteredTabs.size());
        assertEquals(tab0, filteredTabs.get(0));
        assertEquals(tab1, filteredTabs.get(1));
        assertEquals(tab2, filteredTabs.get(2));
    }

    private void createSavedTabGroup(List<MockTab> tabs, boolean isCollaboration) {
        List<SavedTabGroupTab> savedTabs = new ArrayList<>();
        for (Tab tab : tabs) {
            SavedTabGroupTab savedTab = new SavedTabGroupTab();
            savedTab.localId = tab.getId();
            tab.setTabGroupId(TAB_GROUP_ID);
            savedTabs.add(savedTab);
        }
        SavedTabGroup savedGroup = new SavedTabGroup();
        String groupIdString = TAB_GROUP_ID.toString();
        savedGroup.syncId = groupIdString + "_SYNC";
        savedGroup.localId = LOCAL_TAB_GROUP_ID;
        savedGroup.savedTabs = savedTabs;
        if (isCollaboration) {
            savedGroup.collaborationId = groupIdString + "_COLLABORATION";
        }
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {savedGroup.syncId});
        when(mTabGroupSyncService.getGroup(savedGroup.syncId)).thenReturn(savedGroup);
        when(mTabGroupSyncService.isObservingLocalChanges()).thenReturn(true);
    }
}
