// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link TabArchiverImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabArchiverUnitTest {
    private static final GURL TEST_GURL = new GURL("https://www.google.com");
    private static final GURL TEST_GURL_2 = new GURL("https://www.example.com");
    private static final long CURRENT_TIMESTAMP = TimeUnit.HOURS.toMillis(2);

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private @Mock TabModel mArchivedTabModel;
    private @Mock TabCreator mArchivedTabCreator;
    private @Mock TabArchiveSettings mTabArchiveSettings;
    private @Mock TabArchiverImpl.Clock mClock;
    private @Mock Profile mProfile;
    private @Mock Profile mIncognitoProfile;
    private @Mock WebContentsState mWebContentsState;
    private @Mock TabGroupSyncService mTabGroupSyncService;
    private @Mock TabRemover mTabRemover;
    private @Mock TabRemover mIncogTabRemover;

    private MockTabModelSelector mTabModelSelector;
    private TabArchiverImpl mTabArchiver;

    @Before
    public void setUp() {
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        TabIdManager.resetInstanceForTesting();

        // Testing setup is:
        // 50 regular tabs, 10 incognito tabs.
        // Clock is setup 1 hour past epoch 0.
        // Tab timestamps set at epoch 0.
        setupTabModels();
        setupTabsForArchive();
        mTabArchiver =
                new TabArchiverImpl(
                        mArchivedTabModel,
                        mArchivedTabCreator,
                        mTabArchiveSettings,
                        mClock,
                        mTabGroupSyncService);
    }

    @After
    public void tearDown() {
        TabStateExtractor.resetTabStatesForTesting();
    }

    private void setupTabModels() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        // Setup the archived tab model. This behavior can be overridden in tests to test if an
        // archived tab exists in the regular tab model.
        MockTab tab = new MockTab(0, mProfile);
        tab.setIsInitialized(true);
        tab.setWebContentsState(mWebContentsState);
        doReturn(tab).when(mArchivedTabCreator).createFrozenTab(any(), anyInt(), anyInt());
        doAnswer(inv -> Collections.emptyList().iterator()).when(mArchivedTabModel).iterator();

        mTabModelSelector =
                new MockTabModelSelector(mProfile, mIncognitoProfile, 50, 10, this::createTab);
        mTabModelSelector.markTabStateInitialized();

        MockTabModel regularModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ false);
        regularModel.setTabRemoverForTesting(mTabRemover);
        MockTabModel incognitoModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ true);
        incognitoModel.setTabRemoverForTesting(mIncogTabRemover);
    }

    private MockTab createTab(int id, boolean incognito) {
        Profile profile = incognito ? mIncognitoProfile : mProfile;
        MockTab tab = MockTab.createAndInitialize(id, profile);
        tab.setIsInitialized(true);
        return tab;
    }

    private void setupTabsForArchive() {
        doReturn(true).when(mTabArchiveSettings).getArchiveEnabled();
        // Set the tab to expire after 2 hour to simplify testing.
        doReturn(2).when(mTabArchiveSettings).getArchiveTimeDeltaHours();

        // Set the clock to 2 hour after 0.
        doReturn(CURRENT_TIMESTAMP).when(mClock).currentTimeMillis();
        TabList regularTabs =
                mTabModelSelector.getModel(/* incognito= */ false).getComprehensiveModel();
        for (int i = 0; i < regularTabs.getCount(); i++) {
            TabImpl tab = (TabImpl) regularTabs.getTabAt(i);
            tab.setTimestampMillis(0L);
            // Set the navigation timestamp for both tabs at 1 to pass user active check.
            tab.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));
            tab.setWebContentsState(mWebContentsState);
            // Always return a tab state for each regular tab to unblock archiving.
            TabState tabState = new TabState();
            tabState.contentsState = mWebContentsState;
            TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);
        }
    }

    @Test
    public void testMaxSimultaneousArchives() {
        when(mTabArchiveSettings.getMaxSimultaneousArchives()).thenReturn(20);
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.ArchivedTabs.MaxLimitReachedAt", 20);

        mTabArchiver.doArchivePass(mTabModelSelector);

        verify(mArchivedTabCreator, times(20)).createFrozenTab(any(), anyInt(), anyInt());
        watcher.assertExpected();
    }

    @Test
    public void testDoArchivePass_currentModelIsIncognito() {
        when(mTabArchiveSettings.getMaxSimultaneousArchives()).thenReturn(1);
        mTabModelSelector.selectModel(/* incognito= */ true);

        mTabArchiver.doArchivePass(mTabModelSelector);

        verify(mArchivedTabCreator).createFrozenTab(any(), anyInt(), anyInt());
        verify(mTabRemover).closeTabs(any(), anyBoolean());
        verify(mIncogTabRemover, never()).closeTabs(any(), anyBoolean());
    }

    @Test
    public void testArchiveAndRemoveTabs_TabIdAlreadyArchived_SameUrl() {
        MockTab tab = (MockTab) mTabModelSelector.getModel(/* incognito= */ false).getTabAt(0);
        tab.setGurlOverrideForTesting(TEST_GURL);

        MockTab archivedTab = new MockTab(tab.getId(), mProfile);
        archivedTab.setGurlOverrideForTesting(TEST_GURL);
        when(mArchivedTabModel.getTabById(tab.getId())).thenReturn(archivedTab);

        mTabArchiver.archiveAndRemoveTabs(
                mTabModelSelector.getModel(/* incognito= */ false), Collections.singletonList(tab));

        verify(mArchivedTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
    }

    @Test
    public void testArchiveAndRemoveTabs_TabIdAlreadyArchived_DifferentUrl() {
        MockTab tab = (MockTab) mTabModelSelector.getModel(/* incognito= */ false).getTabAt(0);
        tab.setGurlOverrideForTesting(TEST_GURL);

        MockTab archivedTab = new MockTab(tab.getId(), mProfile);
        archivedTab.setGurlOverrideForTesting(TEST_GURL_2);
        when(mArchivedTabModel.getTabById(tab.getId())).thenReturn(archivedTab);

        mTabArchiver.archiveAndRemoveTabs(
                mTabModelSelector.getModel(/* incognito= */ false), Collections.singletonList(tab));

        verify(mArchivedTabCreator, times(1))
                .createFrozenTab(any(), not(eq(tab.getId())), anyInt());
    }

    @Test
    public void testDoArchivePass_NoTabsArchived_TriggersPersistedTabDataCreated() {
        // Setup tabs so that none are eligible for archive.
        TabList regularTabs =
                mTabModelSelector.getModel(/* incognito= */ false).getComprehensiveModel();
        for (int i = 0; i < regularTabs.getCount(); i++) {
            TabImpl tab = (TabImpl) regularTabs.getTabAt(i);
            tab.setTimestampMillis(TimeUnit.HOURS.toMillis(2)); // Same as clock
        }

        TabArchiver.Observer observer = mock(TabArchiver.Observer.class);
        mTabArchiver.addObserver(observer);

        mTabArchiver.doArchivePass(mTabModelSelector);
        shadowOf(Looper.getMainLooper()).idle();
        verify(observer).onArchivePersistedTabDataCreated();
    }

    @Test
    public void testIsTabEligibleForArchive_UninitializedTab_Skipped() {
        TabModel regularModel = mTabModelSelector.getModel(/* incognito= */ false);
        MockTab tab = (MockTab) regularModel.getTabAt(1);
        tab.setIsInitialized(false);

        List<Tab> tabsToArchive = mTabArchiver.getTabsToArchive(regularModel);
        assertFalse(tabsToArchive.contains(tab));
    }

    @Test
    public void testIsTabEligibleForArchive_DestroyedTab_Skipped() {
        TabModel regularModel = mTabModelSelector.getModel(/* incognito= */ false);
        MockTab tab = (MockTab) regularModel.getTabAt(1);
        tab.destroy();

        List<Tab> tabsToArchive = mTabArchiver.getTabsToArchive(regularModel);
        assertFalse(tabsToArchive.contains(tab));
    }

    @Test
    public void testIsTabEligibleForArchive_NullContentsAndState_Skipped() {
        TabModel regularModel = mTabModelSelector.getModel(/* incognito= */ false);
        MockTab tab = (MockTab) regularModel.getTabAt(1);
        tab.setWebContentsState(null);
        tab.setWebContentsOverrideForTesting(null);

        List<Tab> tabsToArchive = mTabArchiver.getTabsToArchive(regularModel);
        assertFalse(tabsToArchive.contains(tab));
    }

    @Test
    public void testIneligibleTabs_RecentTab_NotSerialized() {
        MockTabModel regularModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ false);

        MockTab recentTab = spy(MockTab.createAndInitialize(1234, mProfile));
        recentTab.setIsInitialized(true);
        recentTab.setWebContentsState(mWebContentsState);
        recentTab.setTimestampMillis(CURRENT_TIMESTAMP);
        recentTab.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));
        regularModel.addTab(
                recentTab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        TabArchiverImpl spyArchiver = spy(mTabArchiver);
        List<Tab> tabsToArchive = spyArchiver.getTabsToArchive(regularModel);

        assertFalse(tabsToArchive.contains(recentTab));
        verify(spyArchiver, never()).prepareTabState(recentTab);
        verify(recentTab, never()).getUserAgent();
        verify(recentTab, never()).getTabLaunchTypeAtCreation();
    }

    @Test
    public void testIneligibleTabs_PinnedTab_NotSerialized() {
        MockTabModel regularModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ false);

        MockTab pinnedTab = spy(MockTab.createAndInitialize(5678, mProfile));
        pinnedTab.setIsInitialized(true);
        pinnedTab.setWebContentsState(mWebContentsState);
        pinnedTab.setTimestampMillis(0L); // Old enough to archive
        pinnedTab.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));
        pinnedTab.setIsPinned(true);
        regularModel.addTab(
                pinnedTab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        TabArchiverImpl spyArchiver = spy(mTabArchiver);
        List<Tab> tabsToArchive = spyArchiver.getTabsToArchive(regularModel);

        assertFalse(tabsToArchive.contains(pinnedTab));
        verify(spyArchiver, never()).prepareTabState(pinnedTab);
        verify(pinnedTab, never()).getUserAgent();
        verify(pinnedTab, never()).getTabLaunchTypeAtCreation();
    }

    @Test
    public void testArchiveAndRemoveTabs_TabGroupsDoNotCallPrepareTabState() {
        MockTabModel regularModel =
                spy((MockTabModel) mTabModelSelector.getModel(/* incognito= */ false));
        MockTab tab1 = spy(MockTab.createAndInitialize(101, mProfile));
        tab1.setIsInitialized(true);
        tab1.setWebContentsState(mWebContentsState);
        tab1.setTimestampMillis(0L);
        tab1.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));
        Token groupId = new Token(1L, 2L);
        tab1.setTabGroupId(groupId);

        MockTab tab2 = spy(MockTab.createAndInitialize(102, mProfile));
        tab2.setIsInitialized(true);
        tab2.setWebContentsState(mWebContentsState);
        tab2.setTimestampMillis(0L);
        tab2.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));
        tab2.setTabGroupId(groupId);

        doReturn(Arrays.asList(tab1, tab2)).when(regularModel).getTabsInGroup(groupId);

        TabArchiverImpl spyArchiver = spy(mTabArchiver);

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = "test_sync_id";
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedGroup.savedTabs = Arrays.asList(savedTab1, savedTab2);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedGroup);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Tabs.TabArchived.TabCount", 2)
                        .expectIntRecord("TabGroups.TabGroupDeclutter.ArchivedTabGroups", 1)
                        .expectIntRecord("TabGroups.TabGroupDeclutter.ArchivedTabGroupTabCount", 2)
                        .build();

        spyArchiver.archiveAndRemoveTabs(regularModel, Arrays.asList(tab1, tab2));

        verify(spyArchiver, never()).prepareTabState(any());
        verify(tab1, never()).getUserAgent();
        verify(tab2, never()).getUserAgent();
        verify(mArchivedTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
        verify(mTabGroupSyncService)
                .updateArchivalStatus("test_sync_id", /* archivalStatus= */ true);
        verify(mTabRemover)
                .closeTabs(argThat(params -> params.isTabGroup), /* allowDialog= */ eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testArchiveAndRemoveTabs_NullTabState_Skipped() {
        TabModel regularModel = mTabModelSelector.getModel(/* incognito= */ false);
        MockTab tab = (MockTab) regularModel.getTabAt(1);
        TabStateExtractor.setTabStateForTesting(tab.getId(), null);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.TabArchived.TabCount", 0);

        mTabArchiver.archiveAndRemoveTabs(regularModel, Collections.singletonList(tab));

        verify(mArchivedTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs != null && params.tabs.isEmpty()),
                        /* allowDialog= */ eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testArchiveAndRemoveTabs_NullContentsState_Skipped() {
        TabModel regularModel = mTabModelSelector.getModel(/* incognito= */ false);
        MockTab tab = (MockTab) regularModel.getTabAt(1);

        TabState tabState = new TabState();
        tabState.contentsState = null;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.TabArchived.TabCount", 0);

        mTabArchiver.archiveAndRemoveTabs(regularModel, Collections.singletonList(tab));

        verify(mArchivedTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs != null && params.tabs.isEmpty()),
                        /* allowDialog= */ eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testUnarchiveAndRestoreTabs_NullTabState_Skipped() {
        MockTab tab = new MockTab(100, mProfile);
        tab.setTimestampMillis(12345L);
        when(mArchivedTabModel.getTabRemover()).thenReturn(mTabRemover);

        TabStateExtractor.setTabStateForTesting(tab.getId(), null);

        TabCreator regularTabCreator = mock(TabCreator.class);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.ArchivedTabRestored.TabCount", 0);

        mTabArchiver.unarchiveAndRestoreTabs(
                regularTabCreator,
                Collections.singletonList(tab),
                /* updateTimestamp= */ true,
                /* areTabsBeingOpened= */ false);

        verify(regularTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
        assertEquals(12345L, tab.getTimestampMillis());
        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs != null && params.tabs.isEmpty()),
                        /* allowDialog= */ eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testUnarchiveAndRestoreTabs_CreateFrozenTab_Null() {
        MockTab tab = new MockTab(100, mProfile);
        tab.setTimestampMillis(12345L);
        when(mArchivedTabModel.getTabRemover()).thenReturn(mTabRemover);

        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);

        TabCreator regularTabCreator = mock(TabCreator.class);
        doReturn(null).when(regularTabCreator).createFrozenTab(any(), eq(tab.getId()), anyInt());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.ArchivedTabRestored.TabCount", 0);

        mTabArchiver.unarchiveAndRestoreTabs(
                regularTabCreator,
                Collections.singletonList(tab),
                /* updateTimestamp= */ true,
                /* areTabsBeingOpened= */ false);

        verify(regularTabCreator).createFrozenTab(any(), eq(tab.getId()), anyInt());
        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs != null && params.tabs.isEmpty()),
                        /* allowDialog= */ eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testUnarchiveAndRestoreTabs_Success() {
        MockTab tab = new MockTab(100, mProfile);
        tab.setTimestampMillis(12345L);
        when(mArchivedTabModel.getTabRemover()).thenReturn(mTabRemover);

        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);

        TabCreator regularTabCreator = mock(TabCreator.class);
        MockTab restoredTab = mock(MockTab.class);
        doReturn(restoredTab)
                .when(regularTabCreator)
                .createFrozenTab(any(), eq(tab.getId()), anyInt());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Tabs.ArchivedTabRestored.TabCount", 1);

        mTabArchiver.unarchiveAndRestoreTabs(
                regularTabCreator,
                Collections.singletonList(tab),
                /* updateTimestamp= */ true,
                /* areTabsBeingOpened= */ false);

        assertEquals(12345L, tab.getTimestampMillis());
        ArgumentCaptor<TabState> tabStateCaptor = ArgumentCaptor.forClass(TabState.class);
        verify(regularTabCreator)
                .createFrozenTab(tabStateCaptor.capture(), eq(tab.getId()), anyInt());
        assertEquals(CURRENT_TIMESTAMP, tabStateCaptor.getValue().timestampMillis);
        verify(restoredTab).setTimestampMillis(eq(CURRENT_TIMESTAMP));
        verify(restoredTab).onTabRestoredFromArchivedTabModel();
        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs != null && params.tabs.contains(tab)),
                        /* allowDialog= */ eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testUnarchiveAndRestoreTabs_UpdateTimestamp_NotEligibleForArchive() {
        when(mTabArchiveSettings.getMaxSimultaneousArchives()).thenReturn(100);
        MockTab tab = new MockTab(100, mProfile);
        tab.setTimestampMillis(0L);
        when(mArchivedTabModel.getTabRemover()).thenReturn(mTabRemover);

        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;
        tabState.timestampMillis = 0L;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);

        MockTab restoredTab = new MockTab(100, mProfile);
        restoredTab.setIsInitialized(true);
        restoredTab.setWebContentsState(mWebContentsState);
        restoredTab.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));

        TabCreator regularTabCreator = mock(TabCreator.class);
        doReturn(restoredTab)
                .when(regularTabCreator)
                .createFrozenTab(any(), eq(tab.getId()), anyInt());

        mTabArchiver.unarchiveAndRestoreTabs(
                regularTabCreator,
                Collections.singletonList(tab),
                /* updateTimestamp= */ true,
                /* areTabsBeingOpened= */ false);

        assertEquals(CURRENT_TIMESTAMP, restoredTab.getTimestampMillis());

        MockTabModel regularModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ false);
        regularModel.addTab(
                restoredTab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.FROZEN_ON_RESTORE);

        List<Tab> tabsToArchive = mTabArchiver.getTabsToArchive(regularModel);
        assertFalse(tabsToArchive.contains(restoredTab));
    }

    @Test
    public void testUnarchiveAndRestoreTabs_NoUpdateTimestamp_EligibleForArchive() {
        when(mTabArchiveSettings.getMaxSimultaneousArchives()).thenReturn(100);
        MockTab tab = new MockTab(100, mProfile);
        tab.setTimestampMillis(0L);
        when(mArchivedTabModel.getTabRemover()).thenReturn(mTabRemover);

        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;
        tabState.timestampMillis = 0L;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabState);

        MockTab restoredTab = new MockTab(100, mProfile);
        restoredTab.setIsInitialized(true);
        restoredTab.setWebContentsState(mWebContentsState);
        restoredTab.setTimestampMillis(0L);
        restoredTab.setLastNavigationCommittedTimestampMillis(TimeUnit.HOURS.toMillis(1));

        TabCreator regularTabCreator = mock(TabCreator.class);
        doReturn(restoredTab)
                .when(regularTabCreator)
                .createFrozenTab(any(), eq(tab.getId()), anyInt());

        mTabArchiver.unarchiveAndRestoreTabs(
                regularTabCreator,
                Collections.singletonList(tab),
                /* updateTimestamp= */ false,
                /* areTabsBeingOpened= */ false);

        assertEquals(0L, restoredTab.getTimestampMillis());

        MockTabModel regularModel =
                (MockTabModel) mTabModelSelector.getModel(/* incognito= */ false);
        regularModel.addTab(
                restoredTab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.FROZEN_ON_RESTORE);

        List<Tab> tabsToArchive = mTabArchiver.getTabsToArchive(regularModel);
        assertTrue(tabsToArchive.contains(restoredTab));
    }

    @Test
    public void testPrepareTabState() {
        MockTab tab = (MockTab) mTabModelSelector.getModel(/* incognito= */ false).getTabAt(1);

        // Null TabState
        TabStateExtractor.setTabStateForTesting(tab.getId(), null);
        assertNull(mTabArchiver.prepareTabState(tab));

        // TabState with null contentsState
        TabState tabStateWithoutContents = new TabState();
        tabStateWithoutContents.contentsState = null;
        TabStateExtractor.setTabStateForTesting(tab.getId(), tabStateWithoutContents);
        assertNull(mTabArchiver.prepareTabState(tab));

        // Valid TabState with parentId and rootId set
        TabState validTabState = new TabState();
        validTabState.contentsState = mWebContentsState;
        validTabState.parentId = 123;
        validTabState.rootId = 456;
        TabStateExtractor.setTabStateForTesting(tab.getId(), validTabState);

        TabState prepared = mTabArchiver.prepareTabState(tab);
        assertNotNull(prepared);
        assertEquals(Tab.INVALID_TAB_ID, prepared.parentId);
        assertEquals(Tab.INVALID_TAB_ID, prepared.rootId);
    }
}
