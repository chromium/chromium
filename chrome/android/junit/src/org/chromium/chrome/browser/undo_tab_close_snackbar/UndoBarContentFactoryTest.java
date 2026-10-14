// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import static org.chromium.chrome.browser.undo_tab_close_snackbar.UndoClosureSnapshot.forSavedGroups;
import static org.chromium.chrome.browser.undo_tab_close_snackbar.UndoClosureSnapshot.forTabModel;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Tests for {@link UndoBarContentFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoBarContentFactoryTest {
    private static final Token GROUP_1 = new Token(1, 1);
    private static final Token GROUP_2 = new Token(2, 2);

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testRow1_SingleTab() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null)),
                        Collections.emptyMap(),
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertSame(snapshot, content.snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_close_message), content.template);
        assertEquals("Tab 1", content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, content.umaType);
    }

    @Test
    public void testRow2_MultipleTabs() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", /* groupId= */ null)),
                        Collections.emptyMap(),
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_close_all_message), content.template);
        assertEquals("2", content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO, content.umaType);
    }

    @Test
    public void testRow2_AllTabs() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", /* groupId= */ null)),
                        Collections.emptyMap(),
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ true);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_close_all_message), content.template);
        assertEquals("2", content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_ALL_UNDO, content.umaType);
    }

    @Test
    public void testRow3_PartialGroupClosure() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Collections.emptySet(),
                        Set.of(GROUP_1),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_close_message), content.template);
        assertEquals("Tab 1", content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, content.umaType);
    }

    @Test
    public void testRow4_OneGroup_Deleted() {
        // G==1, U==0, isDel=true (sync=true, hiding=false)
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Collections.emptySet(), // not hiding => deleted
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_deleted_message), content.template);
        assertEquals("Group 1", content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testRow4_OneGroup_Deleted_EmptyTitlePlaceholder() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_1)),
                        Map.of(GROUP_1, ""), // empty title -> placeholder
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_deleted_message), content.template);
        assertEquals(
                mContext.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 2, 2),
                content.content);
    }

    @Test
    public void testRow4_OneGroup_Deleted_NullTitlePlaceholder() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_1)),
                        // Defensive: map values are @Nullable; production uses "".
                        Collections.singletonMap(GROUP_1, null),
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_deleted_message), content.template);
        assertEquals(
                mContext.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 2, 2),
                content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testRow4_OneSingleTabGroup_Deleted_SingleGroupDeleteUma() {
        // G==1, U==0, one tab, isDel=true (sync=true, hiding=false)
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Collections.emptySet(), // not hiding => deleted
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_deleted_message), content.template);
        assertEquals("Group 1", content.content);
        assertEquals(Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testRow5_OneGroup_ClosedAndSaved() {
        // G==1, U==0, closed_and_saved (sync=true, hiding=true)
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Set.of(GROUP_1), // hiding
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_closed_and_saved_message),
                content.template);
        assertEquals("Group 1", content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, content.umaType);
    }

    @Test
    public void testRow6_OneGroup_Closed() {
        // G==1, U==0, closed (sync=false)
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Set.of(GROUP_1),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ false,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_closed_message), content.template);
        assertEquals("Group 1", content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, content.umaType);
    }

    @Test
    public void testRow7_MultipleGroups_Deleted() {
        // G==2, isDel=true (sync=true, hiding=false)
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_2)),
                        Map.of(GROUP_1, "Group 1", GROUP_2, "Group 2"),
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_deleted_message), content.template);
        assertEquals(
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 2, 2),
                content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testRow8_MultipleGroups_ClosedAndSaved() {
        // G==2, sync=true, hiding=true
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_2)),
                        Map.of(GROUP_1, "Group 1", GROUP_2, "Group 2"),
                        Set.of(GROUP_1, GROUP_2),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_closed_and_saved_message), content.template);
        assertEquals(
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 2, 2),
                content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO, content.umaType);
    }

    @Test
    public void testRow9_MultipleGroups_Closed() {
        // G==2, sync=false
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", GROUP_1),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_2)),
                        Map.of(GROUP_1, "Group 1", GROUP_2, "Group 2"),
                        Set.of(GROUP_1, GROUP_2),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ false,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_closed_message), content.template);
        assertEquals(
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 2, 2),
                content.content);
        assertEquals(Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO, content.umaType);
    }

    @Test
    public void testSavedGroups_QuirkDeleteUmaWithClosedText() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        Collections.emptyList(),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1"),
                        /* savedGroupTitle= */ "Group 1",
                        /* savedGroupTabCount= */ 2);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_closed_message), content.template);
        assertEquals("Group 1", content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testMixedGroupAndUngroupedTabs() {
        UndoClosureSnapshot snapshot =
                forTabModel(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", GROUP_1)),
                        Map.of(GROUP_1, "Group 1"),
                        Collections.emptySet(),
                        Collections.emptySet(),
                        /* isTabGroupSyncEnabled= */ true,
                        /* isAllTabs= */ false);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_deleted_message), content.template);
        String expectedGroupsPart =
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 1, 1);
        String expectedContent =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.undo_bar_tab_groups_and_tabs_part,
                                1,
                                expectedGroupsPart,
                                1);
        assertEquals(expectedContent, content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testSavedGroups_EmptyTitle_UsesPlaceholder() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        Collections.emptyList(),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1"),
                        /* savedGroupTitle= */ "",
                        /* savedGroupTabCount= */ 3);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(
                mContext.getString(R.string.undo_bar_tab_group_closed_message), content.template);
        assertEquals(
                mContext.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 3, 3),
                content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testSavedGroups_GroupNotFound_ShowsGenericGroupCount() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        Collections.emptyList(),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1"),
                        /* savedGroupTitle= */ null,
                        /* savedGroupTabCount= */ 0);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_closed_message), content.template);
        assertEquals(
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 1, 1),
                content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    /** Guards that a missing group title with closed tabs keeps the generic text. */
    @Test
    public void testSavedGroups_GroupNotFoundWithTab_ShowsGenericText() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null)),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1"),
                        /* savedGroupTitle= */ null,
                        /* savedGroupTabCount= */ 0);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_closed_message), content.template);
        String expectedGroupsPart =
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 1, 1);
        assertEquals(
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.undo_bar_tab_groups_and_tabs_part,
                                1,
                                expectedGroupsPart,
                                1),
                content.content);
        assertEquals(Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testSavedGroups_MultipleGroups() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        Collections.emptyList(),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1", "sync-id-2"),
                        /* savedGroupTitle= */ null,
                        /* savedGroupTabCount= */ 0);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_closed_message), content.template);
        assertEquals(
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 2, 2),
                content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testSavedGroups_GroupAndTab() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        List.of(new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null)),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1"),
                        /* savedGroupTitle= */ "Group 1",
                        /* savedGroupTabCount= */ 2);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_closed_message), content.template);
        String expectedGroupsPart =
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 1, 1);
        assertEquals(
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.undo_bar_tab_groups_and_tabs_part,
                                1,
                                expectedGroupsPart,
                                1),
                content.content);
        assertEquals(Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO, content.umaType);
    }

    @Test
    public void testSavedGroups_MultipleGroupsAndTabs() {
        UndoClosureSnapshot snapshot =
                forSavedGroups(
                        List.of(
                                new UndoClosureSnapshot.ClosedTab(1, "Tab 1", /* groupId= */ null),
                                new UndoClosureSnapshot.ClosedTab(2, "Tab 2", /* groupId= */ null)),
                        /* savedTabGroupSyncIds= */ List.of("sync-id-1", "sync-id-2"),
                        /* savedGroupTitle= */ null,
                        /* savedGroupTabCount= */ 0);

        UndoBarContent content = UndoBarContentFactory.create(mContext, snapshot);
        assertEquals(mContext.getString(R.string.undo_bar_closed_message), content.template);
        String expectedGroupsPart =
                mContext.getResources().getQuantityString(R.plurals.undo_bar_tab_groups_part, 2, 2);
        assertEquals(
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.undo_bar_tab_groups_and_tabs_part,
                                2,
                                expectedGroupsPart,
                                2),
                content.content);
        assertEquals(Snackbar.UMA_TAB_GROUP_DELETE_UNDO, content.umaType);
    }
}
