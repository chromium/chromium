// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoClosureSnapshot.ClosureKind;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Tests for {@link UndoClosureSnapshotFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoClosureSnapshotFactoryTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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

        when(mLazySupplier.get()).thenReturn(null);
        when(tabModel.getLazyAllTabGroupIds(any(), eq(true))).thenReturn(mLazySupplier);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab),
                        /* isAllTabs= */ false,
                        /* isTabGroupSyncEnabled= */ true);

        assertEquals(ClosureKind.TAB_MODEL, snapshot.kind);
        assertTrue(snapshot.survivingGroupIds.isEmpty());
    }

    @Test
    public void testNullServiceEmptyTitle() {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getTitle()).thenReturn("Tab 1");

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureSavedGroups(
                        List.of(tab), List.of("sync-id-1"), /* tabGroupSyncService= */ null);

        assertEquals(ClosureKind.SAVED_GROUPS, snapshot.kind);
        assertFalse(snapshot.isEmpty());
        assertEquals(1, snapshot.savedTabGroupSyncIds.size());
        assertNull(snapshot.savedGroupTitle);
        assertEquals(0, snapshot.savedGroupTabCount);
    }

    @Test
    public void testSavedGroupNotFound() {
        TabGroupSyncService service = mock(TabGroupSyncService.class);
        when(service.getGroup("sync-id-1")).thenReturn(null);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureSavedGroups(
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
                UndoClosureSnapshotFactory.captureSavedGroups(
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
                        /* isTabGroupSyncEnabled= */ true);

        verify(mLazySupplier, never()).get();
        assertTrue(snapshot.survivingGroupIds.isEmpty());
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
                        /* isTabGroupSyncEnabled= */ true);

        assertEquals(2, snapshot.survivingGroupIds.size());
        assertTrue(snapshot.survivingGroupIds.contains(GROUP_1));
        assertTrue(snapshot.survivingGroupIds.contains(GROUP_2));
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

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureFromTabModel(
                        tabModel,
                        List.of(tab1, tab2),
                        /* isAllTabs= */ false,
                        /* isTabGroupSyncEnabled= */ true);

        assertEquals(Set.of(GROUP_1), snapshot.hidingGroupIds);
        assertEquals(2, snapshot.groupTitles.size());
        assertEquals("Group 1", snapshot.groupTitles.get(GROUP_1));
        assertTrue(snapshot.groupTitles.containsKey(GROUP_2));
        assertNull(snapshot.groupTitles.get(GROUP_2));
        assertTrue(snapshot.survivingGroupIds.isEmpty());
    }

    @Test
    public void testMultipleSavedGroupsSkipsServiceLookup() {
        TabGroupSyncService service = mock(TabGroupSyncService.class);

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshotFactory.captureSavedGroups(
                        Collections.emptyList(), List.of("sync-id-1", "sync-id-2"), service);

        verify(service, never()).getGroup(anyString());
        assertEquals(ClosureKind.SAVED_GROUPS, snapshot.kind);
        assertEquals(List.of("sync-id-1", "sync-id-2"), snapshot.savedTabGroupSyncIds);
        assertNull(snapshot.savedGroupTitle);
        assertEquals(0, snapshot.savedGroupTabCount);
    }
}
