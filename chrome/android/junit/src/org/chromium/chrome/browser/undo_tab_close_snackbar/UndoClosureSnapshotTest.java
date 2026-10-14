// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.undo_tab_close_snackbar.UndoClosureSnapshot.forSavedGroups;
import static org.chromium.chrome.browser.undo_tab_close_snackbar.UndoClosureSnapshot.forTabModel;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoClosureSnapshot.ClosureKind;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Tests for {@link UndoClosureSnapshot}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoClosureSnapshotTest {
    private static final Token GROUP_1 = new Token(1, 1);
    private static final Token GROUP_2 = new Token(2, 2);

    @Test
    public void testConstruction() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Set.of(GROUP_1),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        assertEquals(ClosureKind.TAB_MODEL, snapshot.kind);
        assertEquals(1, snapshot.tabs.size());
        assertEquals("Tab 1", snapshot.tabs.get(0).title);
        assertEquals(GROUP_1, snapshot.tabs.get(0).groupId);
        assertFalse(snapshot.isEmpty());
        assertEquals(1, snapshot.tabIds().length);
        assertEquals(1, snapshot.tabIds()[0]);
    }

    @Test
    public void testWithoutTabs() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Set.of(GROUP_1),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoClosureSnapshot shrunk = snapshot.withoutTabs(List.of(1));
        assertEquals(1, shrunk.tabs.size());
        assertEquals(2, shrunk.tabs.get(0).id);
        assertFalse(shrunk.isEmpty());
        assertEquals(ClosureKind.TAB_MODEL, shrunk.kind);

        UndoClosureSnapshot empty = shrunk.withoutTabs(List.of(2));
        assertTrue(empty.isEmpty());
    }

    @Test
    public void testWithoutTabs_PreservesSavedGroupsKind() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", /* groupId= */ null)),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1"),
                        /* savedGroupTitle= */ null,
                        /* savedGroupTabCount= */ 0);

        UndoClosureSnapshot shrunk = snapshot.withoutTabs(List.of(1));
        assertEquals(1, shrunk.tabs.size());
        assertEquals(ClosureKind.SAVED_GROUPS, shrunk.kind);
    }

    @Test
    public void testWithGroupRestored() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Set.of(GROUP_1),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoClosureSnapshot restored = snapshot.withGroupRestored(GROUP_1);
        assertFalse(restored.hidingGroupIds.contains(GROUP_1));
        assertTrue(restored.survivingGroupIds.contains(GROUP_1));
        assertEquals(ClosureKind.TAB_MODEL, restored.kind);
    }

    @Test
    public void testWithGroupRestored_UnknownGroup_ReturnsSameInstance() {
        UndoClosureSnapshot snapshot = createSingleGroupSnapshot(/* isHiding= */ true);

        assertSame(snapshot, snapshot.withGroupRestored(GROUP_2));
    }

    @Test
    public void testWithGroupRestored_AlreadyRestored_ReturnsSameInstance() {
        UndoClosureSnapshot restored =
                createSingleGroupSnapshot(/* isHiding= */ true).withGroupRestored(GROUP_1);

        assertSame(restored, restored.withGroupRestored(GROUP_1));
    }

    @Test
    public void testWithGroupRestored_DeletingGroup_AddsSurvival() {
        UndoClosureSnapshot snapshot = createSingleGroupSnapshot(/* isHiding= */ false);

        UndoClosureSnapshot restored = snapshot.withGroupRestored(GROUP_1);
        assertNotSame(snapshot, restored);
        assertTrue(restored.hidingGroupIds.isEmpty());
        assertTrue(restored.survivingGroupIds.contains(GROUP_1));
    }

    @Test
    public void testEmptyInput() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        Collections.emptyList(),
                        Collections.emptyMap(),
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);
        assertTrue(snapshot.isEmpty());
    }

    private static UndoClosureSnapshot createSingleGroupSnapshot(boolean isHiding) {
        return forTabModel(
                List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1)),
                Map.of(GROUP_1, "Group 1"),
                isHiding ? Set.of(GROUP_1) : Collections.emptySet(),
                /* survivingGroupIds= */ Collections.emptySet(),
                /* isTabGroupSyncEnabled= */ true,
                /* isAllTabs= */ false);
    }
}
