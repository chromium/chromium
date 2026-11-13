// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link HistoricalTabModelObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HistoricalTabModelObserverUnitTest {
    private static final String COLLABORATION_ID = "collab_id";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private HistoricalTabSaver mHistoricalTabSaver;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private Callback<UndoClosureSnapshot> mUndoClosureCallback;

    @Captor private ArgumentCaptor<List<HistoricalEntry>> mEntriesCaptor;
    @Captor private ArgumentCaptor<UndoClosureSnapshot> mSnapshotCaptor;

    private Context mContext;
    private HistoricalTabModelObserver mObserver;
    private SavedTabGroup mSavedTabGroup;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);

        when(mTabModel.isTabGroupHiding(any())).thenReturn(false);
        when(mTabModel.isTabInTabGroup(any())).thenReturn(false);
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.emptyList().iterator());
        when(mHistoricalTabSaver.createHistoricalBulkClosure(Collections.emptyList()))
                .thenReturn(HistoricalTabSaver.INVALID_TAB_RESTORE_ENTRY_ID);

        mObserver = new HistoricalTabModelObserver(mTabModel, mHistoricalTabSaver);
        mObserver.setUndoClosureCallback(mUndoClosureCallback);
        verify(mTabModel).addObserver(mObserver);

        mContext = spy(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);

        mSavedTabGroup = new SavedTabGroup();
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
    }

    @After
    public void tearDown() {
        mObserver.destroy();
        verify(mTabModel).removeObserver(mObserver);
    }

    @Test
    public void testEmpty() {
        mObserver.onFinishingMultipleTabClosure(new ArrayList<>(), /* canRestore= */ true);

        verifyNoMoreInteractions(mHistoricalTabSaver);
    }

    @Test
    public void testMultipleTabs_NotRestorable() {
        MockTab mockTab = createMockTab(0);

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ false);

        verifyNoMoreInteractions(mHistoricalTabSaver);
    }

    @Test
    public void testSingleTab() {
        MockTab mockTab = createMockTab(0);

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalTab(eq(mockTab));
    }

    @Test
    public void testTabGroupWithSingleTab_NotUndoable() {
        MockTab mockTab = createMockTab(123);
        Token tabGroupId = new Token(1L, 2L);
        String title = "bar";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab});

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();

        assertEquals(1, entries.size());
        HistoricalEntry group = entries.get(0);
        assertFalse(group.isSingleTab());
        assertEquals(1, group.getTabs().size());
        assertEquals(tabGroupId, group.getTabGroupId());
        assertEquals(title, group.getGroupTitle());
        assertEquals(color, group.getGroupColor());
        assertEquals(mockTab, group.getTabs().get(0));
    }

    @Test
    public void testTabGroupWithSingleTab_Undoable() {
        MockTab mockTab = createMockTab(123);
        Token tabGroupId = new Token(1L, 2L);
        String title = "bar";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab});
        when(mTabModel.getTabCountForGroup(tabGroupId)).thenReturn(1);
        when(mTabModel.containsTabGroup(tabGroupId)).thenReturn(false);
        when(mTabModel.isTabInTabGroup(mockTab)).thenReturn(false);

        mObserver.onFinishingMultipleTabClosure(
                Collections.singletonList(mockTab), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();

        assertEquals(1, entries.size());
        HistoricalEntry group = entries.get(0);
        assertFalse(group.isSingleTab());
        assertEquals(1, group.getTabs().size());
        assertEquals(tabGroupId, group.getTabGroupId());
        assertEquals(title, group.getGroupTitle());
        assertEquals(color, group.getGroupColor());
        assertEquals(mockTab, group.getTabs().get(0));
    }

    @Test
    public void testMultipleTabs() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        Tab[] tabList = new Tab[] {mockTab0, mockTab1, mockTab2};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(tabList.length, entries.size());
        for (int i = 0; i < tabList.length; i++) {
            HistoricalEntry entry = entries.get(i);
            assertEquals(1, entry.getTabs().size());
            assertEquals(tabList[i], entry.getTabs().get(0));
        }
    }

    @Test
    public void testSubsetOfGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1, mockTab2};
        Token tabGroupId = new Token(1L, 243L);
        createGroup(tabGroupId, title, color, tabList);

        List<Tab> closingTabList = List.of(mockTab1, mockTab2);
        mObserver.onFinishingMultipleTabClosure(closingTabList, /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();

        assertEquals(2, entries.size());
        HistoricalEntry entryTab1 = entries.get(0);
        HistoricalEntry entryTab2 = entries.get(1);
        assertTrue(entryTab1.isSingleTab());
        assertTrue(entryTab2.isSingleTab());
        assertEquals(mockTab1, entryTab1.getTabs().get(0));
        assertEquals(mockTab2, entryTab2.getTabs().get(0));
    }

    @Test
    public void testSingleGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1, mockTab2};
        Token tabGroupId = new Token(1L, 243L);
        createGroup(tabGroupId, title, color, tabList);

        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single group
        // entry.
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();

        assertEquals(1, entries.size());
        HistoricalEntry group = entries.get(0);
        assertFalse(group.isSingleTab());
        assertEquals(tabList.length, group.getTabs().size());
        assertEquals(tabGroupId, group.getTabGroupId());
        assertEquals(title, group.getGroupTitle());
        assertEquals(color, group.getGroupColor());
        for (int i = 0; i < tabList.length; i++) {
            assertEquals(tabList[i], group.getTabs().get(i));
        }
    }

    @Test
    public void testSkipsCollaboration_WholeGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0, mockTab1});
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));

        mSavedTabGroup.collaborationId = COLLABORATION_ID;

        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testDoesNotSkipCollaborationTabs_PartialGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0, mockTab1, mockTab2});
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(tabGroupId)));

        mSavedTabGroup.collaborationId = COLLABORATION_ID;

        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(2, entries.size());
        HistoricalEntry entryTab1 = entries.get(0);
        HistoricalEntry entryTab2 = entries.get(1);
        assertTrue(entryTab1.isSingleTab());
        assertTrue(entryTab2.isSingleTab());
        assertEquals(mockTab0, entryTab1.getTabs().get(0));
        assertEquals(mockTab1, entryTab2.getTabs().get(0));
    }

    @Test
    public void testDoesNotSkipCollaborationTabs_SingleTabFromGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0, mockTab1});
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(tabGroupId)));

        mSavedTabGroup.collaborationId = COLLABORATION_ID;

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        verify(mHistoricalTabSaver).createHistoricalTab(mockTab0);
        verify(mHistoricalTabSaver, never()).createHistoricalBulkClosure(any());
    }

    @Test
    public void testSkipsCollaborationTabs_EntireSingleTabGroup() {
        MockTab mockTab0 = createMockTab(0);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0});
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));

        mSavedTabGroup.collaborationId = COLLABORATION_ID;

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        verify(mHistoricalTabSaver, never()).createHistoricalTab(any());
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testSingleTabInGroup() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab1});

        MockTab[] tabList = new MockTab[] {mockTab0, mockTab1};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single tab
        // entry.
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(2, entries.size());

        HistoricalEntry tab0 = entries.get(0);
        assertEquals(1, tab0.getTabs().size());
        assertEquals(mockTab0, tab0.getTabs().get(0));

        HistoricalEntry tab1 = entries.get(1);
        assertEquals(1, tab1.getTabs().size());
        assertEquals(tabGroupId, tab1.getTabGroupId());
        assertEquals(mockTab1, tab1.getTabs().get(0));
    }

    @Test
    public void testTabGroupHiding() {
        MockTab mockTab0 = createMockTab(0);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0});
        when(mTabModel.isTabGroupHiding(tabGroupId)).thenReturn(true);
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single tab
        // entry.
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testTabGroupHidingTwoPhases_SingleTabs() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0, mockTab1});
        when(mTabModel.isTabGroupHiding(tabGroupId)).thenReturn(true);
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(tabGroupId)));

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // Close tab 0 first, even though the group is hiding there are still other tabs in the
        // comprehensive model for the group so treat as a separate closure.
        verify(mHistoricalTabSaver).createHistoricalTab(eq(mockTab0));

        // Close tab 1, since it is part of the last event "closing" the tab group and it is hiding
        // the group so no entry should be created.
        tabList = new MockTab[] {mockTab1};
        createGroup(tabGroupId, title, color, tabList);
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testTabGroupHidingTwoPhases_SingleTabThenTwo() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);

        final String title = "foo";
        @TabGroupColorId int color = TabGroupColorId.GREY;
        Token tabGroupId = new Token(3L, 4L);
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab0, mockTab1, mockTab2});
        when(mTabModel.isTabGroupHiding(tabGroupId)).thenReturn(true);
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(tabGroupId)));

        MockTab[] tabList = new MockTab[] {mockTab0};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // Close tab 0 first, even though the group is hiding there are still other tabs in the
        // comprehensive model for the group so treat as a separate closure.
        verify(mHistoricalTabSaver).createHistoricalTab(eq(mockTab0));

        // Close tab 1, and tab 2, since it is part of the last event "closing" the tab group and it
        // is hiding the group so no entry should be created.
        tabList = new MockTab[] {mockTab1, mockTab2};
        createGroup(tabGroupId, title, color, new MockTab[] {mockTab1, mockTab2});
        when(mTabModel.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<>()));
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(0, entries.size());
    }

    @Test
    public void testMultipleTabsAndGroups() {
        MockTab mockTab0 = createMockTab(0);
        MockTab mockTab1 = createMockTab(1);
        MockTab mockTab2 = createMockTab(2);
        MockTab mockTab3 = createMockTab(3);
        MockTab mockTab4 = createMockTab(4);
        MockTab mockTab5 = createMockTab(5);

        final String groupTitle1 = "foo";
        @TabGroupColorId int groupColor1 = TabGroupColorId.GREY;
        MockTab[] groupTabs1 = new MockTab[] {mockTab3, mockTab5};
        Token tabGroupId1 = new Token(3L, 4L);
        createGroup(tabGroupId1, groupTitle1, groupColor1, groupTabs1);

        final String groupTitle2 = "Bar";
        @TabGroupColorId int groupColor2 = TabGroupColorId.BLUE;
        MockTab[] groupTabs2 = new MockTab[] {mockTab1, mockTab2};
        Token tabGroupId2 = new Token(6L, 7L);
        createGroup(tabGroupId2, groupTitle2, groupColor2, groupTabs2);

        Tab[] tabList = new Tab[] {mockTab0, mockTab2, mockTab3, mockTab4, mockTab1, mockTab5};
        mObserver.onFinishingMultipleTabClosure(Arrays.asList(tabList), /* canRestore= */ true);

        // HistoricalTabModelObserver relies on HistoricalTabSaver to simplify to a single group
        // entry.
        verify(mHistoricalTabSaver).createHistoricalBulkClosure(mEntriesCaptor.capture());
        List<HistoricalEntry> entries = mEntriesCaptor.getValue();
        assertEquals(4, entries.size());

        // First tab in list is ungrouped mockTab0.
        HistoricalEntry historicalTab0 = entries.get(0);
        assertEquals(1, historicalTab0.getTabs().size());
        assertEquals(mockTab0, historicalTab0.getTabs().get(0));

        // Second tab in list is mockTab2 for tabGroup2. This grabs mockTab1 to this position.
        HistoricalEntry historicalGroup2 = entries.get(1);
        assertFalse(historicalGroup2.isSingleTab());
        assertEquals(2, historicalGroup2.getTabs().size());
        assertEquals(tabGroupId2, historicalGroup2.getTabGroupId());
        assertEquals(groupTitle2, historicalGroup2.getGroupTitle());
        assertEquals(groupColor2, historicalGroup2.getGroupColor());
        assertEquals(mockTab2, historicalGroup2.getTabs().get(0));
        assertEquals(mockTab1, historicalGroup2.getTabs().get(1));

        // Third tab in list is mockTab3 for tabGroup1. This grabs mockTab5 to this position.
        HistoricalEntry historicalGroup1 = entries.get(2);
        assertFalse(historicalGroup1.isSingleTab());
        assertEquals(2, historicalGroup1.getTabs().size());
        assertEquals(tabGroupId1, historicalGroup1.getTabGroupId());
        assertEquals(groupTitle1, historicalGroup1.getGroupTitle());
        assertEquals(groupColor1, historicalGroup1.getGroupColor());
        assertEquals(mockTab3, historicalGroup1.getTabs().get(0));
        assertEquals(mockTab5, historicalGroup1.getTabs().get(1));

        // Fourth tab in list is ungrouped mockTab4.
        HistoricalEntry historicalTab4 = entries.get(3);
        assertEquals(1, historicalTab4.getTabs().size());
        assertEquals(mockTab4, historicalTab4.getTabs().get(0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testUndoableSingleTab_BuildsSnapshotAndInvokesCallback() {
        MockTab mockTab = createMockTab(10);
        mockTab.setIsPinned(true);
        when(mHistoricalTabSaver.createHistoricalTab(mockTab)).thenReturn(42);
        when(mTabModel.index()).thenReturn(2);
        when(mTabModel.indexOf(mockTab)).thenReturn(2);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        Runnable undoRunnable = mock(Runnable.class);

        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab),
                /* canRestore= */ true,
                TabClosingSource.TABLET_TAB_STRIP,
                undoRunnable,
                /* isAllTabs= */ true,
                /* isUndoable= */ true);

        verify(mHistoricalTabSaver).createHistoricalTab(mockTab);
        verify(mUndoClosureCallback).onResult(mSnapshotCaptor.capture());
        UndoClosureSnapshot snapshot = mSnapshotCaptor.getValue();
        assertEquals(42, snapshot.entryId);
        assertTrue(snapshot.isAllTabs);
        assertTrue(snapshot.isTabGroupSyncEnabled);
        assertEquals(TabClosingSource.TABLET_TAB_STRIP, snapshot.closingSource);
        assertSame(undoRunnable, snapshot.undoRunnable);
        assertEquals(1, snapshot.tabs.size());
        assertEquals(10, snapshot.tabs.get(0).id);
        assertEquals(2, snapshot.tabs.get(0).modelIndex);
        assertTrue(snapshot.tabs.get(0).pinned);
        assertTrue(snapshot.tabs.get(0).wasSelected);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testUndoableBulkClosure_BuildsSnapshotWithGroupDataAndEntryId() {
        MockTab mockTab0 = createMockTab(10);
        MockTab mockTab1 = createMockTab(20);
        Token tabGroupId = new Token(3L, 4L);
        createGroup(
                tabGroupId,
                /* title= */ "Group",
                TabGroupColorId.BLUE,
                new MockTab[] {mockTab0, mockTab1});
        when(mHistoricalTabSaver.createHistoricalBulkClosure(any())).thenReturn(99);
        when(mTabModel.index()).thenReturn(1);
        when(mTabModel.indexOf(mockTab0)).thenReturn(0);
        when(mTabModel.indexOf(mockTab1)).thenReturn(1);
        when(mTabModel.getIndexOfTabInGroup(mockTab0)).thenReturn(0);
        when(mTabModel.getIndexOfTabInGroup(mockTab1)).thenReturn(1);
        when(mTabModel.getLazyAllTabGroupIds(any(), eq(true)))
                .thenReturn(LazyOneshotSupplier.fromValue(Collections.emptySet()));

        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab0, mockTab1),
                /* canRestore= */ true,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(any());
        verify(mUndoClosureCallback).onResult(mSnapshotCaptor.capture());
        UndoClosureSnapshot snapshot = mSnapshotCaptor.getValue();
        assertEquals(99, snapshot.entryId);
        assertEquals(2, snapshot.tabs.size());
        assertEquals(0, snapshot.tabs.get(0).indexInGroup);
        assertFalse(snapshot.tabs.get(0).wasSelected);
        assertEquals(1, snapshot.tabs.get(1).indexInGroup);
        assertTrue(snapshot.tabs.get(1).wasSelected);
        assertNotNull(snapshot.groups.get(tabGroupId));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testUndoableClosure_SkipsCallbackOnGuardConditions() {
        MockTab mockTab = createMockTab(10);

        // 1. Not undoable -> saves to TabRestoreService, skips undo callback.
        when(mHistoricalTabSaver.createHistoricalTab(mockTab)).thenReturn(42);
        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab),
                /* canRestore= */ true,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ false);

        // 2. Invalid entryId -> skips undo callback.
        when(mHistoricalTabSaver.createHistoricalTab(mockTab))
                .thenReturn(HistoricalTabSaver.INVALID_TAB_RESTORE_ENTRY_ID);
        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab),
                /* canRestore= */ true,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ true);

        // 3. Incognito-branded model -> skips undo callback.
        when(mHistoricalTabSaver.createHistoricalTab(mockTab)).thenReturn(42);
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab),
                /* canRestore= */ true,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ true);

        // 4. Null callback -> skips cleanly without error.
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        mObserver.setUndoClosureCallback(null);
        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab),
                /* canRestore= */ true,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ true);

        verify(mUndoClosureCallback, never()).onResult(any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testUndoableClosure_FlagDisabled_SkipsCallback() {
        MockTab mockTab = createMockTab(10);
        when(mHistoricalTabSaver.createHistoricalTab(mockTab)).thenReturn(42);

        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab),
                /* canRestore= */ true,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ true);

        verify(mHistoricalTabSaver).createHistoricalTab(mockTab);
        verify(mUndoClosureCallback, never()).onResult(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testUndoableHiddenGroupOnlyClosure_InvokesCallbackWithSyncId() {
        Token tabGroupId = new Token(3L, 4L);
        closeSyncedGroup(
                tabGroupId,
                /* syncId= */ "sync-hidden-1",
                /* collaborationId= */ null,
                /* isHiding= */ true);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(Collections.emptyList());
        verify(mUndoClosureCallback).onResult(mSnapshotCaptor.capture());
        UndoClosureSnapshot snapshot = mSnapshotCaptor.getValue();
        assertEquals(HistoricalTabSaver.INVALID_TAB_RESTORE_ENTRY_ID, snapshot.entryId);
        assertTrue(snapshot.hasReopenableHiddenGroups());
        UndoClosureSnapshot.GroupData groupData = snapshot.groups.get(tabGroupId);
        assertNotNull(groupData);
        assertTrue(groupData.isHiding);
        assertFalse(groupData.survivesInModel);
        assertEquals("sync-hidden-1", groupData.savedTabGroupSyncId);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testUndoableCollaborationGroupOnlyClosure_SkipsCallbackWhenNotHiding() {
        closeSyncedGroup(
                new Token(3L, 4L),
                /* syncId= */ "sync-collab-1",
                COLLABORATION_ID,
                /* isHiding= */ false);

        verify(mHistoricalTabSaver).createHistoricalBulkClosure(Collections.emptyList());
        verify(mUndoClosureCallback, never()).onResult(any());
    }

    private void closeSyncedGroup(
            Token tabGroupId, String syncId, @Nullable String collaborationId, boolean isHiding) {
        MockTab mockTab0 = createMockTab(10);
        createGroup(tabGroupId, "Group", TabGroupColorId.BLUE, new MockTab[] {mockTab0});
        when(mTabModel.isTabGroupHiding(tabGroupId)).thenReturn(isHiding);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        mSavedTabGroup.syncId = syncId;
        mSavedTabGroup.collaborationId = collaborationId;
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(tabGroupId)))
                .thenReturn(mSavedTabGroup);
        when(mTabModel.getLazyAllTabGroupIds(any(), eq(true)))
                .thenReturn(LazyOneshotSupplier.fromValue(Collections.emptySet()));

        mObserver.onFinishingMultipleTabClosure(
                List.of(mockTab0),
                /* canRestore= */ true,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null,
                /* isAllTabs= */ false,
                /* isUndoable= */ true);
    }

    private MockTab createMockTab(int id) {
        MockTab mockTab = new MockTab(id, mProfile);
        return mockTab;
    }

    /**
     * Creates a group.
     *
     * @param tabGroupId The tab group id.
     * @param title Group title.
     * @param color Group color.
     * @param tabList List of tabs in group.
     */
    private void createGroup(
            Token tabGroupId,
            @Nullable String title,
            @TabGroupColorId int color,
            MockTab[] tabList) {
        assertThat(tabList).isNotEmpty();

        when(mTabModel.getTabsInGroup(tabGroupId)).thenReturn(Arrays.asList(tabList));
        when(mTabModel.getTabCountForGroup(tabGroupId)).thenReturn(tabList.length);
        when(mTabModel.containsTabGroup(tabGroupId)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(tabGroupId)).thenReturn(title);
        when(mTabModel.getTabGroupColorWithFallback(tabGroupId)).thenReturn(color);
        for (MockTab tab : tabList) {
            tab.setTabGroupId(tabGroupId);
            when(mTabModel.isTabInTabGroup(tab)).thenReturn(true);
        }
    }
}
