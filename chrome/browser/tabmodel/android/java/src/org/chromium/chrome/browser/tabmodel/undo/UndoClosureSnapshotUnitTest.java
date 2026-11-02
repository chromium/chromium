// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.undo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.forSavedGroups;
import static org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.forTabModel;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosureKind;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for {@link UndoClosureSnapshot}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoClosureSnapshotUnitTest {
    private static final Token GROUP_1 = new Token(1, 1);

    @Test
    public void testConstruction() {
        Runnable runnable = () -> {};
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new ClosedTab(
                                        /* id= */ 1,
                                        "Tab 1",
                                        GROUP_1,
                                        /* modelIndex= */ TabList.INVALID_TAB_INDEX,
                                        /* pinned= */ true,
                                        /* wasSelected= */ false)),
                        Map.of(
                                GROUP_1,
                                new GroupData(
                                        "Group 1",
                                        /* color= */ 2,
                                        /* collapsed= */ true,
                                        /* isHiding= */ true,
                                        /* survivesInModel= */ false)),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false,
                        /* entryId= */ 42,
                        /* closingSource= */ TabClosingSource.TABLET_TAB_STRIP,
                        runnable);

        assertEquals(ClosureKind.TAB_MODEL, snapshot.kind);
        assertEquals(1, snapshot.tabs.size());
        assertEquals("Tab 1", snapshot.tabs.get(0).title);
        assertEquals(GROUP_1, snapshot.tabs.get(0).groupId);
        assertTrue(snapshot.tabs.get(0).pinned);
        assertFalse(snapshot.tabs.get(0).wasSelected);
        assertEquals(TabList.INVALID_TAB_INDEX, snapshot.tabs.get(0).modelIndex);

        GroupData groupData = snapshot.groups.get(GROUP_1);
        assertNotNull(groupData);
        assertEquals("Group 1", groupData.title);
        assertEquals(2, groupData.color);
        assertTrue(groupData.collapsed);
        assertTrue(groupData.isHiding);
        assertFalse(groupData.survivesInModel);

        assertEquals(42, snapshot.entryId);
        assertEquals(TabClosingSource.TABLET_TAB_STRIP, snapshot.closingSource);
        assertSame(runnable, snapshot.undoRunnable);
    }

    @Test
    public void testForSavedGroups() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        List.of(
                                new ClosedTab(
                                        /* id= */ 1,
                                        "Tab 1",
                                        /* groupId= */ null,
                                        /* modelIndex= */ TabList.INVALID_TAB_INDEX,
                                        /* pinned= */ false,
                                        /* wasSelected= */ false)),
                        /* savedTabGroupSyncIds= */ List.of("sync-1"),
                        /* savedGroupTitle= */ "Saved 1",
                        /* savedGroupTabCount= */ 3);

        assertEquals(ClosureKind.SAVED_GROUPS, snapshot.kind);
        assertTrue(snapshot.groups.isEmpty());
        assertFalse(snapshot.isTabGroupSyncEnabled);
        assertFalse(snapshot.isAllTabs);
        assertEquals(List.of("sync-1"), snapshot.savedTabGroupSyncIds);
        assertEquals("Saved 1", snapshot.savedGroupTitle);
        assertEquals(3, snapshot.savedGroupTabCount);
        assertEquals(TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID, snapshot.entryId);
        assertEquals(TabClosingSource.UNKNOWN, snapshot.closingSource);
        assertNull(snapshot.undoRunnable);
    }

    @Test
    public void testCollectionsAreUnmodifiable() {
        List<ClosedTab> tabs = new ArrayList<>();
        tabs.add(
                new ClosedTab(
                        /* id= */ 1,
                        "Tab 1",
                        /* groupId= */ null,
                        /* modelIndex= */ TabList.INVALID_TAB_INDEX,
                        /* pinned= */ false,
                        /* wasSelected= */ false));

        Map<Token, GroupData> groups = new HashMap<>();
        groups.put(
                GROUP_1,
                new GroupData(
                        "Group 1",
                        /* color= */ 0,
                        /* collapsed= */ false,
                        /* isHiding= */ false,
                        /* survivesInModel= */ true));

        UndoClosureSnapshot snapshot =
                forTabModel(
                        tabs,
                        groups,
                        /* isTabGroupSyncEnabled= */ false,
                        /* isAllTabs= */ false,
                        /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                        /* closingSource= */ TabClosingSource.UNKNOWN,
                        /* undoRunnable= */ null);

        // Mutate original collections after construction.
        tabs.clear();
        groups.clear();

        assertEquals(1, snapshot.tabs.size());
        assertEquals(1, snapshot.groups.size());
        assertTrue(snapshot.groups.containsKey(GROUP_1));

        // Verify snapshot collections are unmodifiable.
        assertThrows(UnsupportedOperationException.class, () -> snapshot.tabs.clear());
        assertThrows(UnsupportedOperationException.class, () -> snapshot.groups.clear());
    }
}
