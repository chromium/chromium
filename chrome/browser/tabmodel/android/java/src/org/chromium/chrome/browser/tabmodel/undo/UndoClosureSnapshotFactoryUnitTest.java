// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.undo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosureKind;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Tests for {@link UndoClosureSnapshotFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoClosureSnapshotFactoryUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final Token GROUP_1 = new Token(1, 1);
    private static final Token GROUP_2 = new Token(2, 2);

    @Mock private LazyOneshotSupplier<Set<Token>> mLazySupplier;

    @Test
    public void testNullLazyGroupIdSet() {
        TabModel tabModel = mock(TabModel.class);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getTitle()).thenReturn("Tab 1");
        when(tab.getTabGroupId()).thenReturn(GROUP_1);
        when(tab.getIsPinned()).thenReturn(true);

        when(mLazySupplier.get()).thenReturn(null);
        when(tabModel.getLazyAllTabGroupIds(any(), eq(true))).thenReturn(mLazySupplier);
        when(tabModel.getTabGroupColorWithFallback(GROUP_1)).thenReturn(3);
        when(tabModel.getTabGroupCollapsed(GROUP_1)).thenReturn(false);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab),
                        /* isAllTabs= */ false,
                        /* isTabGroupSyncEnabled= */ true,
                        /* tabGroupSyncService= */ null,
                        TabClosingSource.TABLET_TAB_STRIP,
                        /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                        /* undoRunnable= */ null);

        assertEquals(ClosureKind.TAB_MODEL, snapshot.kind);
        GroupData groupData = snapshot.groups.get(GROUP_1);
        assertNotNull(groupData);
        assertFalse(groupData.survivesInModel);
        assertEquals(3, groupData.color);
        assertFalse(groupData.collapsed);
        assertNull(groupData.savedTabGroupSyncId);
        assertTrue(snapshot.tabs.get(0).pinned);
        assertEquals(TabClosingSource.TABLET_TAB_STRIP, snapshot.closingSource);
        assertNull(snapshot.undoRunnable);
    }

    @Test
    public void testNullServiceEmptyTitle() {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getTitle()).thenReturn("Tab 1");

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureArchived(
                        List.of(tab), List.of("sync-id-1"), /* tabGroupSyncService= */ null);

        assertEquals(ClosureKind.ARCHIVED, snapshot.kind);
        assertEquals(1, snapshot.savedTabGroupSyncIds.size());
        assertNull(snapshot.savedGroupTitle);
        assertEquals(0, snapshot.savedGroupTabCount);
        assertEquals(TabList.INVALID_TAB_INDEX, snapshot.tabs.get(0).modelIndex);
    }

    @Test
    public void testSavedGroupNotFound() {
        TabGroupSyncService service = mock(TabGroupSyncService.class);
        when(service.getGroup("sync-id-1")).thenReturn(null);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureArchived(
                        Collections.emptyList(), List.of("sync-id-1"), service);

        assertNull(snapshot.savedGroupTitle);
        assertEquals(0, snapshot.savedGroupTabCount);
    }

    @Test
    public void testSavedGroupFoundWithNullTitle() {
        SavedTabGroup group = new SavedTabGroup();
        group.title = null;
        group.savedTabs.add(new SavedTabGroupTab());
        group.savedTabs.add(new SavedTabGroupTab());
        TabGroupSyncService service = mock(TabGroupSyncService.class);
        when(service.getGroup("sync-id-1")).thenReturn(group);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureArchived(
                        Collections.emptyList(), List.of("sync-id-1"), service);

        assertEquals("", snapshot.savedGroupTitle);
        assertEquals(2, snapshot.savedGroupTabCount);
    }

    @Test
    public void testUngroupedTabsNeverCallSupplier() {
        TabModel tabModel = mock(TabModel.class);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getTitle()).thenReturn("Tab 1");
        when(tab.getTabGroupId()).thenReturn(null);

        when(tabModel.getLazyAllTabGroupIds(any(), eq(true))).thenReturn(mLazySupplier);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab),
                        /* isAllTabs= */ false,
                        /* isTabGroupSyncEnabled= */ true,
                        /* tabGroupSyncService= */ null,
                        /* closingSource= */ TabClosingSource.UNKNOWN,
                        /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                        /* undoRunnable= */ null);

        verify(mLazySupplier, never()).get();
        assertTrue(snapshot.groups.isEmpty());
    }

    @Test
    public void testMultipleGroups() {
        TabModel tabModel = mock(TabModel.class);
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab1.getTitle()).thenReturn("Tab 1");
        when(tab1.getTabGroupId()).thenReturn(GROUP_1);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.getTitle()).thenReturn("Tab 2");
        when(tab2.getTabGroupId()).thenReturn(GROUP_2);

        when(mLazySupplier.get()).thenReturn(Set.of(GROUP_1, GROUP_2));
        when(tabModel.getLazyAllTabGroupIds(any(), eq(true))).thenReturn(mLazySupplier);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab1, tab2),
                        /* isAllTabs= */ false,
                        /* isTabGroupSyncEnabled= */ true,
                        /* tabGroupSyncService= */ null,
                        /* closingSource= */ TabClosingSource.UNKNOWN,
                        /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                        /* undoRunnable= */ null);

        assertEquals(2, snapshot.groups.size());
        GroupData group1 = snapshot.groups.get(GROUP_1);
        assertNotNull(group1);
        assertTrue(group1.survivesInModel);
        GroupData group2 = snapshot.groups.get(GROUP_2);
        assertNotNull(group2);
        assertTrue(group2.survivesInModel);
    }

    @Test
    public void testHidingGroupAndTitlesCaptured() {
        TabModel tabModel = mock(TabModel.class);
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab1.getTitle()).thenReturn("Tab 1");
        when(tab1.getTabGroupId()).thenReturn(GROUP_1);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.getTitle()).thenReturn("Tab 2");
        when(tab2.getTabGroupId()).thenReturn(GROUP_2);

        when(tabModel.getTabGroupTitle(GROUP_1)).thenReturn("Group 1");
        when(tabModel.getTabGroupTitle(GROUP_2)).thenReturn(null);
        when(tabModel.isTabGroupHiding(GROUP_1)).thenReturn(true);
        when(tabModel.isTabGroupHiding(GROUP_2)).thenReturn(false);
        when(mLazySupplier.get()).thenReturn(Collections.emptySet());
        when(tabModel.getLazyAllTabGroupIds(any(), eq(true))).thenReturn(mLazySupplier);

        SavedTabGroup savedGroup1 = new SavedTabGroup();
        savedGroup1.syncId = "sync-group-1";
        TabGroupSyncService syncService = mock(TabGroupSyncService.class);
        when(syncService.getGroup(new LocalTabGroupId(GROUP_1))).thenReturn(savedGroup1);
        when(syncService.getGroup(new LocalTabGroupId(GROUP_2))).thenReturn(null);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab1, tab2),
                        /* isAllTabs= */ false,
                        /* isTabGroupSyncEnabled= */ true,
                        syncService,
                        /* closingSource= */ TabClosingSource.UNKNOWN,
                        /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                        /* undoRunnable= */ null);

        assertEquals(2, snapshot.groups.size());
        GroupData group1 = snapshot.groups.get(GROUP_1);
        assertNotNull(group1);
        GroupData group2 = snapshot.groups.get(GROUP_2);
        assertNotNull(group2);

        assertTrue(group1.isHiding);
        assertEquals("Group 1", group1.title);
        assertFalse(group1.survivesInModel);
        assertEquals("sync-group-1", group1.savedTabGroupSyncId);
        assertTrue(snapshot.hasReopenableHiddenGroups());

        assertFalse(group2.isHiding);
        assertNull(group2.title);
        assertFalse(group2.survivesInModel);
        assertNull(group2.savedTabGroupSyncId);
    }

    @Test
    public void testMultipleSavedGroupsSkipsServiceLookup() {
        TabGroupSyncService service = mock(TabGroupSyncService.class);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureArchived(
                        Collections.emptyList(), List.of("sync-id-1", "sync-id-2"), service);

        verify(service, never()).getGroup(anyString());
        assertEquals(ClosureKind.ARCHIVED, snapshot.kind);
        assertEquals(List.of("sync-id-1", "sync-id-2"), snapshot.savedTabGroupSyncIds);
        assertNull(snapshot.savedGroupTitle);
        assertEquals(0, snapshot.savedGroupTabCount);
    }

    @Test
    public void testCaptureFromTabModel_PreRemovalIndicesAndSelection() {
        TabModel tabModel = mock(TabModel.class);
        Tab tab1 = mock(Tab.class);
        when(tab1.getId()).thenReturn(10);
        when(tab1.getTitle()).thenReturn("Tab 1");
        when(tab1.getTabGroupId()).thenReturn(GROUP_1);
        when(tab1.getIsPinned()).thenReturn(true);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(20);
        when(tab2.getTitle()).thenReturn("Tab 2");
        when(tab2.getTabGroupId()).thenReturn(null);
        when(tab2.getIsPinned()).thenReturn(false);

        when(tabModel.index()).thenReturn(4);
        when(tabModel.indexOf(tab1)).thenReturn(4);
        when(tabModel.indexOf(tab2)).thenReturn(7);
        when(tabModel.getIndexOfTabInGroup(tab1)).thenReturn(2);
        when(tabModel.getTabGroupTitle(GROUP_1)).thenReturn("Group 1");
        when(tabModel.getTabGroupColorWithFallback(GROUP_1)).thenReturn(5);
        when(tabModel.getTabGroupCollapsed(GROUP_1)).thenReturn(true);
        when(tabModel.isTabGroupHiding(GROUP_1)).thenReturn(false);
        when(mLazySupplier.get()).thenReturn(Set.of(GROUP_1));
        when(tabModel.getLazyAllTabGroupIds(any(), eq(true))).thenReturn(mLazySupplier);

        Runnable undoRunnable = mock(Runnable.class);
        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab1, tab2),
                        /* isAllTabs= */ true,
                        /* isTabGroupSyncEnabled= */ true,
                        /* tabGroupSyncService= */ null,
                        TabClosingSource.TABLET_TAB_STRIP,
                        /* entryId= */ 42,
                        undoRunnable);

        verify(tabModel, never()).getIndexOfTabInGroup(tab2);
        assertEquals(ClosureKind.TAB_MODEL, snapshot.kind);
        assertTrue(snapshot.isAllTabs);
        assertTrue(snapshot.isTabGroupSyncEnabled);
        assertEquals(42, snapshot.entryId);
        assertEquals(TabClosingSource.TABLET_TAB_STRIP, snapshot.closingSource);
        assertSame(undoRunnable, snapshot.undoRunnable);
        assertEquals(2, snapshot.tabs.size());

        ClosedTab closed1 = snapshot.tabs.get(0);
        assertEquals(10, closed1.id);
        assertEquals("Tab 1", closed1.title);
        assertEquals(GROUP_1, closed1.groupId);
        assertEquals(2, closed1.indexInGroup);
        assertEquals(4, closed1.modelIndex);
        assertTrue(closed1.pinned);
        assertTrue(closed1.wasSelected);

        ClosedTab closed2 = snapshot.tabs.get(1);
        assertEquals(20, closed2.id);
        assertEquals("Tab 2", closed2.title);
        assertNull(closed2.groupId);
        assertEquals(TabList.INVALID_TAB_INDEX, closed2.indexInGroup);
        assertEquals(7, closed2.modelIndex);
        assertFalse(closed2.pinned);
        assertFalse(closed2.wasSelected);

        GroupData group1 = snapshot.groups.get(GROUP_1);
        assertNotNull(group1);
        assertEquals("Group 1", group1.title);
        assertEquals(5, group1.color);
        assertTrue(group1.collapsed);
        assertFalse(group1.isHiding);
        assertTrue(group1.survivesInModel);
    }

    @Test
    public void testCaptureFromTabModel_PostRemovalInvalidIndices() {
        TabModel tabModel = mock(TabModel.class);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(10);
        when(tab.getTitle()).thenReturn("Tab 1");
        when(tab.getTabGroupId()).thenReturn(GROUP_1);

        when(tabModel.index()).thenReturn(TabList.INVALID_TAB_INDEX);
        when(tabModel.indexOf(tab)).thenReturn(TabList.INVALID_TAB_INDEX);
        when(mLazySupplier.get()).thenReturn(Collections.emptySet());
        when(tabModel.getLazyAllTabGroupIds(any(), eq(true))).thenReturn(mLazySupplier);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab),
                        /* isAllTabs= */ false,
                        /* isTabGroupSyncEnabled= */ false,
                        /* tabGroupSyncService= */ null,
                        TabClosingSource.UNKNOWN,
                        /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                        /* undoRunnable= */ null);

        verify(tabModel, never()).getIndexOfTabInGroup(any());
        ClosedTab closed = snapshot.tabs.get(0);
        assertEquals(TabList.INVALID_TAB_INDEX, closed.modelIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, closed.indexInGroup);
        assertFalse(closed.wasSelected);
    }
}
