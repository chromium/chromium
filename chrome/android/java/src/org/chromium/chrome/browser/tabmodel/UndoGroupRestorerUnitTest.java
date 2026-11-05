// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;
import static org.chromium.chrome.browser.tabmodel.TabGroupColorUtils.INVALID_COLOR_ID;
import static org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType.DONT_NOTIFY;
import static org.chromium.chrome.browser.tabmodel.TabList.INVALID_TAB_INDEX;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Collections;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link UndoGroupRestorer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoGroupRestorerUnitTest {
    private static final Token GROUP_A = new Token(1L, 1L);
    private static final Token GROUP_B = new Token(2L, 2L);
    private static final String GROUP_TITLE = "Group";
    private static final @TabGroupColorId int GROUP_COLOR = 3;

    private static final @TabId int TAB_ID_1 = 11;
    private static final @TabId int TAB_ID_2 = 12;
    private static final @TabId int TAB_ID_3 = 13;
    private static final @TabId int TAB_ID_4 = 14;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mTab4;

    @Before
    public void setUp() {
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB_ID_3)).thenReturn(mTab3);
        when(mTabModel.getTabById(TAB_ID_4)).thenReturn(mTab4);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);
        when(mTabModel.indexOf(mTab4)).thenReturn(3);
    }

    @Test
    public void testWholeGroupRecreated() {
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(false);
        when(mTabModel.indexOf(mTab1)).thenReturn(5);
        when(mTabModel.indexOf(mTab2)).thenReturn(3);
        when(mTabModel.indexOf(mTab3)).thenReturn(6);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(
                                createClosedTab(TAB_ID_1, GROUP_A, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_2, GROUP_A, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_3, GROUP_A, /* indexInGroup= */ 0)),
                        Map.of(
                                GROUP_A,
                                createGroupData(
                                        GROUP_TITLE,
                                        /* collapsed= */ true,
                                        /* isHiding= */ false)));

        UndoGroupRestorer.apply(mTabModel, new int[] {TAB_ID_1, TAB_ID_2, TAB_ID_3}, snapshot);

        InOrder inOrder = inOrder(mTabModel);
        inOrder.verify(mTabModel)
                .createTabGroupForTabGroupSync(List.of(mTab3, mTab2, mTab1), GROUP_A);
        inOrder.verify(mTabModel).setTabGroupTitle(GROUP_A, GROUP_TITLE);
        inOrder.verify(mTabModel).setTabGroupColor(GROUP_A, GROUP_COLOR);
        inOrder.verify(mTabModel)
                .setTabGroupCollapsed(GROUP_A, /* isCollapsed= */ true, /* animate= */ false);
        verify(mTabModel, never()).mergeListOfTabsToExistingGroup(any(), any(), any(), anyInt());
    }

    @Test
    public void testEmptyTitleAndInvalidColorNotSet() {
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(false);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(createClosedTab(TAB_ID_1, GROUP_A, INVALID_TAB_INDEX)),
                        Map.of(
                                GROUP_A,
                                new GroupData(
                                        /* title= */ "",
                                        INVALID_COLOR_ID,
                                        /* collapsed= */ false,
                                        /* isHiding= */ false,
                                        /* survivesInModel= */ false)));

        UndoGroupRestorer.apply(mTabModel, new int[] {TAB_ID_1}, snapshot);

        verify(mTabModel).createTabGroupForTabGroupSync(List.of(mTab1), GROUP_A);
        verify(mTabModel, never()).setTabGroupTitle(any(), anyString());
        verify(mTabModel, never()).setTabGroupColor(any(), anyInt());
        verify(mTabModel, never()).setTabGroupCollapsed(any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testPartialGroupMergedAtIndexInGroup() {
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(true);
        // The group is collapsed before the merge and expanded by it.
        when(mTabModel.getTabGroupCollapsed(GROUP_A)).thenReturn(true, false);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(
                                createClosedTab(TAB_ID_1, GROUP_A, /* indexInGroup= */ 2),
                                createClosedTab(TAB_ID_2, GROUP_A, /* indexInGroup= */ 0)),
                        Map.of(
                                GROUP_A,
                                createGroupData(
                                        GROUP_TITLE,
                                        /* collapsed= */ true,
                                        /* isHiding= */ false)));

        UndoGroupRestorer.apply(mTabModel, new int[] {TAB_ID_1, TAB_ID_2}, snapshot);

        InOrder inOrder = inOrder(mTabModel);
        inOrder.verify(mTabModel).getTabGroupCollapsed(GROUP_A);
        inOrder.verify(mTabModel)
                .mergeListOfTabsToExistingGroup(
                        Collections.singletonList(mTab2),
                        GROUP_A,
                        /* indexInGroup= */ 0,
                        DONT_NOTIFY);
        inOrder.verify(mTabModel)
                .mergeListOfTabsToExistingGroup(
                        Collections.singletonList(mTab1),
                        GROUP_A,
                        /* indexInGroup= */ 2,
                        DONT_NOTIFY);
        inOrder.verify(mTabModel)
                .setTabGroupCollapsed(GROUP_A, /* isCollapsed= */ true, /* animate= */ false);
        verify(mTabModel, never()).createTabGroupForTabGroupSync(any(), any());
        verify(mTabModel, never()).setTabGroupTitle(any(), anyString());
        verify(mTabModel, never()).setTabGroupColor(any(), anyInt());
    }

    @Test
    public void testPartialGroupInvalidIndexInGroupAppendsInModelOrder() {
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(true);
        when(mTabModel.getTabGroupCollapsed(GROUP_A)).thenReturn(false);
        when(mTabModel.indexOf(mTab1)).thenReturn(5);
        when(mTabModel.indexOf(mTab2)).thenReturn(3);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(
                                createClosedTab(TAB_ID_1, GROUP_A, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_2, GROUP_A, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_3, GROUP_A, /* indexInGroup= */ 1)),
                        Map.of(
                                GROUP_A,
                                createGroupData(
                                        GROUP_TITLE,
                                        /* collapsed= */ false,
                                        /* isHiding= */ false)));

        UndoGroupRestorer.apply(mTabModel, new int[] {TAB_ID_1, TAB_ID_2, TAB_ID_3}, snapshot);

        InOrder inOrder = inOrder(mTabModel);
        inOrder.verify(mTabModel)
                .mergeListOfTabsToExistingGroup(
                        Collections.singletonList(mTab3),
                        GROUP_A,
                        /* indexInGroup= */ 1,
                        DONT_NOTIFY);
        inOrder.verify(mTabModel)
                .mergeListOfTabsToExistingGroup(
                        eq(Collections.singletonList(mTab2)),
                        eq(GROUP_A),
                        isNull(),
                        eq(DONT_NOTIFY));
        inOrder.verify(mTabModel)
                .mergeListOfTabsToExistingGroup(
                        eq(Collections.singletonList(mTab1)),
                        eq(GROUP_A),
                        isNull(),
                        eq(DONT_NOTIFY));
        verify(mTabModel, never()).setTabGroupCollapsed(any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testHiddenGroupSkipped() {
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(createClosedTab(TAB_ID_1, GROUP_A, INVALID_TAB_INDEX)),
                        Map.of(
                                GROUP_A,
                                createGroupData(
                                        GROUP_TITLE,
                                        /* collapsed= */ false,
                                        /* isHiding= */ true)));

        UndoGroupRestorer.apply(mTabModel, new int[] {TAB_ID_1}, snapshot);

        verifyNoGroupMutations();
    }

    @Test
    public void testUnrestoredTabsSkipped() {
        when(mTabModel.getTabById(TAB_ID_2)).thenReturn(null);
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(false);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(
                                createClosedTab(TAB_ID_1, GROUP_A, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_2, GROUP_A, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_3, GROUP_A, INVALID_TAB_INDEX)),
                        Collections.emptyMap());

        UndoGroupRestorer.apply(
                mTabModel, new int[] {INVALID_TAB_ID, TAB_ID_2, TAB_ID_3}, snapshot);

        verify(mTabModel).createTabGroupForTabGroupSync(List.of(mTab3), GROUP_A);
        verify(mTabModel, never()).getTabById(INVALID_TAB_ID);
    }

    @Test
    public void testGroupWithNothingRestoredUntouched() {
        when(mTabModel.getTabById(TAB_ID_2)).thenReturn(null);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(
                                createClosedTab(TAB_ID_1, GROUP_A, /* indexInGroup= */ 0),
                                createClosedTab(TAB_ID_2, GROUP_A, /* indexInGroup= */ 1)),
                        Map.of(
                                GROUP_A,
                                createGroupData(
                                        GROUP_TITLE,
                                        /* collapsed= */ false,
                                        /* isHiding= */ false)));

        UndoGroupRestorer.apply(mTabModel, new int[] {INVALID_TAB_ID, TAB_ID_2}, snapshot);

        verify(mTabModel, never()).containsTabGroup(any());
        verifyNoGroupMutations();
    }

    @Test
    public void testMultipleGroupsProcessedInFirstAppearanceOrder() {
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(false);
        when(mTabModel.containsTabGroup(GROUP_B)).thenReturn(false);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(
                                createClosedTab(TAB_ID_1, /* groupId= */ null, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_2, GROUP_B, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_3, GROUP_A, INVALID_TAB_INDEX),
                                createClosedTab(TAB_ID_4, GROUP_B, INVALID_TAB_INDEX)),
                        Map.of(
                                GROUP_A,
                                createGroupData(
                                        GROUP_TITLE, /* collapsed= */ false, /* isHiding= */ false),
                                GROUP_B,
                                createGroupData(
                                        GROUP_TITLE,
                                        /* collapsed= */ true,
                                        /* isHiding= */ false)));

        UndoGroupRestorer.apply(
                mTabModel, new int[] {TAB_ID_1, TAB_ID_2, TAB_ID_3, TAB_ID_4}, snapshot);

        InOrder inOrder = inOrder(mTabModel);
        inOrder.verify(mTabModel).createTabGroupForTabGroupSync(List.of(mTab2, mTab4), GROUP_B);
        inOrder.verify(mTabModel)
                .setTabGroupCollapsed(GROUP_B, /* isCollapsed= */ true, /* animate= */ false);
        inOrder.verify(mTabModel).createTabGroupForTabGroupSync(List.of(mTab3), GROUP_A);
        verify(mTabModel, never()).setTabGroupCollapsed(eq(GROUP_A), anyBoolean(), anyBoolean());
        verify(mTabModel, never()).getTabById(TAB_ID_1);
    }

    @Test
    public void testMissingGroupDataRecreatedWithoutVisuals() {
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(false);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(createClosedTab(TAB_ID_1, GROUP_A, INVALID_TAB_INDEX)),
                        Collections.emptyMap());

        UndoGroupRestorer.apply(mTabModel, new int[] {TAB_ID_1}, snapshot);

        verify(mTabModel).createTabGroupForTabGroupSync(List.of(mTab1), GROUP_A);
        verify(mTabModel, never()).setTabGroupTitle(any(), anyString());
        verify(mTabModel, never()).setTabGroupColor(any(), anyInt());
        verify(mTabModel, never()).setTabGroupCollapsed(any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testPinnedTabSkipped() {
        when(mTabModel.containsTabGroup(GROUP_A)).thenReturn(false);
        ClosedTab pinnedTab =
                new ClosedTab(
                        TAB_ID_1,
                        /* title= */ "Tab",
                        GROUP_A,
                        /* indexInGroup= */ 0,
                        /* modelIndex= */ INVALID_TAB_INDEX,
                        /* pinned= */ true,
                        /* wasSelected= */ false);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(pinnedTab),
                        Map.of(
                                GROUP_A,
                                createGroupData(
                                        GROUP_TITLE,
                                        /* collapsed= */ false,
                                        /* isHiding= */ false)));

        UndoGroupRestorer.apply(mTabModel, new int[] {TAB_ID_1}, snapshot);

        verify(mTabModel, never()).containsTabGroup(any());
        verifyNoGroupMutations();
    }

    private void verifyNoGroupMutations() {
        verify(mTabModel, never()).createTabGroupForTabGroupSync(any(), any());
        verify(mTabModel, never()).mergeListOfTabsToExistingGroup(any(), any(), any(), anyInt());
        verify(mTabModel, never()).setTabGroupTitle(any(), anyString());
        verify(mTabModel, never()).setTabGroupColor(any(), anyInt());
        verify(mTabModel, never()).setTabGroupCollapsed(any(), anyBoolean(), anyBoolean());
    }

    private static ClosedTab createClosedTab(
            @TabId int id, @Nullable Token groupId, int indexInGroup) {
        return new ClosedTab(
                id,
                /* title= */ "Tab",
                groupId,
                indexInGroup,
                /* modelIndex= */ INVALID_TAB_INDEX,
                /* pinned= */ false,
                /* wasSelected= */ false);
    }

    private static GroupData createGroupData(String title, boolean collapsed, boolean isHiding) {
        return new GroupData(title, GROUP_COLOR, collapsed, isHiding, /* survivesInModel= */ false);
    }

    private static UndoClosureSnapshot createSnapshot(
            List<ClosedTab> tabs, Map<Token, GroupData> groups) {
        return UndoClosureSnapshot.forTabModel(
                tabs,
                groups,
                /* isTabGroupSyncEnabled= */ false,
                /* isAllTabs= */ false,
                /* entryId= */ 1,
                /* closingSource= */ TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null);
    }
}
