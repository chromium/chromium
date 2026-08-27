// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link NestedTabReorderUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NestedTabReorderUtilsUnitTest {
    private static final int TAB_ID_1 = 101;
    private static final int TAB_ID_2 = 102;
    private static final int TAB_ID_3 = 103;
    private static final Token GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mPinnedTab;

    private TabListModel mModelList;

    @Before
    public void setUp() {
        when(mTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabModel.getTabsInGroup(GROUP_ID)).thenReturn(List.of(mTab1, mTab2));
        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTab3.getId()).thenReturn(TAB_ID_3);

        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB_ID_3)).thenReturn(mTab3);

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);

        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(mTab3);

        mModelList = new TabListModel();
    }

    @Test
    @SmallTest
    public void testReorderItem_GroupHeader_MoveDown() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB_ID_2)).thenReturn(List.of(mTab2));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 0, /* toIndex= */ 1));
        verify(mTabModel).moveRelatedTabs(TAB_ID_1, 1);
    }

    @Test
    @SmallTest
    public void testReorderItem_ChildTab_UngroupUp_PastHeader() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 1, /* toIndex= */ 0));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab1), /* trailing= */ false, false);
    }

    @Test
    @SmallTest
    public void testReorderItem_ChildTab_UngroupDown_PastGroupEnd() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_3)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));

        when(mTabModel.getRelatedTabList(TAB_ID_2)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 2, /* toIndex= */ 3));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab2), /* trailing= */ true, false);
    }

    @Test
    @SmallTest
    public void testReorderItem_ChildTab_MoveWithinGroup() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getRelatedTabList(TAB_ID_2)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 1, /* toIndex= */ 2));
        verify(mTabModel).moveTab(TAB_ID_1, 1);
        verify(mTabUngrouper, never()).ungroupTabs(any(), anyBoolean(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testReorderItem_StandaloneTab_MergeDown_IntoExpandedGroup() {
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .build();
        PropertyModel childModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 0, /* toIndex= */ 1));
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(List.of(mTab1)),
                        eq(mTab2),
                        eq(0),
                        eq(TabGroupMergeNotificationType.NOTIFY_ALWAYS));
    }

    @Test
    @SmallTest
    public void testReorderItem_StandaloneTab_MergeUp_IntoExpandedGroup() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .build();
        PropertyModel childModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 2, /* toIndex= */ 1));
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(List.of(mTab2)),
                        eq(mTab1),
                        isNull(),
                        eq(TabGroupMergeNotificationType.NOTIFY_ALWAYS));
    }

    @Test
    @SmallTest
    public void testReorderItem_StandaloneTab_JumpPast_CollapsedGroup_Down() {
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .with(TabProperties.IS_COLLAPSED, true)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));

        when(mTabModel.getRelatedTabList(TAB_ID_2)).thenReturn(List.of(mTab2, mTab3));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 0, /* toIndex= */ 1));
        verify(mTabModel).moveTab(TAB_ID_1, 2);
    }

    @Test
    @SmallTest
    public void testReorderItem_StandaloneTab_JumpPast_CollapsedGroup_Up() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .with(TabProperties.IS_COLLAPSED, true)
                        .build();
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_3)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 1, /* toIndex= */ 0));
        verify(mTabModel).moveTab(TAB_ID_3, 0);
    }

    @Test
    @SmallTest
    public void testReorderItem_StandaloneTab_SwapWithStandaloneTab() {
        PropertyModel model1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        PropertyModel model2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB, model1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, model2));

        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 0, /* toIndex= */ 1));
        verify(mTabModel).moveTab(TAB_ID_1, 1);
    }

    @Test
    @SmallTest
    public void testReorderItem_InvalidIndices_ReturnsFalse() {
        assertFalse(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ -1, /* toIndex= */ 0));
        assertFalse(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 0, /* toIndex= */ 0));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testReorderItemInDirection_ForwardsCorrectly() {
        PropertyModel model1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        PropertyModel model2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB, model1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, model2));

        assertTrue(
                NestedTabReorderUtils.reorderItemInDirection(
                        mTabModel, mModelList, /* pos= */ 0, /* toPrevious= */ false));
        verify(mTabModel).moveTab(TAB_ID_1, 1);

        assertTrue(
                NestedTabReorderUtils.reorderItemInDirection(
                        mTabModel, mModelList, /* pos= */ 1, /* toPrevious= */ true));
        verify(mTabModel).moveTab(TAB_ID_2, 0);

        // Boundary checks
        assertFalse(
                NestedTabReorderUtils.reorderItemInDirection(
                        mTabModel, mModelList, /* pos= */ 0, /* toPrevious= */ true));
        assertFalse(
                NestedTabReorderUtils.reorderItemInDirection(
                        mTabModel, mModelList, /* pos= */ 1, /* toPrevious= */ false));
    }

    @Test
    @SmallTest
    public void testReorderItemInDirection_ChildTabAtEndOfList_UngroupsDown() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));

        // Child 2 is at pos 2 (the very end of modelList). Moving down should ungroup it trailing.
        assertTrue(
                NestedTabReorderUtils.reorderItemInDirection(
                        mTabModel, mModelList, /* pos= */ 2, /* toPrevious= */ false));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab2), /* trailing= */ true, false);
    }

    @Test
    @SmallTest
    public void testReorderItem_GroupHeaderWithoutTabId_ResolvesFromGroup() {
        // Group header using ALL_KEYS_TAB_GROUP_GRID without TAB_ID
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));

        // Child 1 moving up onto the header (which lacks TAB_ID) should resolve representative tab
        // ID and ungroup
        assertTrue(
                NestedTabReorderUtils.reorderItem(
                        mTabModel, mModelList, /* fromIndex= */ 1, /* toIndex= */ 0));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab1), /* trailing= */ false, false);
    }

    @Test
    @SmallTest
    public void testReorderItem_TopChildTab_UngroupUp_HeaderWithTabUiType() {
        // Group header created in nested layout with UiType.TAB and TAB_GROUP_HEADER_ID
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(
                NestedTabReorderUtils.reorderItemInDirection(
                        mTabModel, mModelList, /* pos= */ 1, /* toPrevious= */ true));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab1), /* trailing= */ false, false);
    }

    @Test
    @SmallTest
    public void testReorderItemInDirection_SolitaryChild_MovesEntireGroup() {
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_3)
                        .build();
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel solitaryChildModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, solitaryChildModel));

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB_ID_3)).thenReturn(List.of(mTab3));
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(0);
        when(mTabModel.getTabAt(0)).thenReturn(mTab3);

        // Solitary child tab at pos 2 moving up should move the entire group above standalone tab
        // at index 0.
        assertTrue(
                NestedTabReorderUtils.reorderItemInDirection(
                        mTabModel, mModelList, /* pos= */ 2, /* toPrevious= */ true));
        verify(mTabModel).moveRelatedTabs(TAB_ID_1, 0);
    }

    @Test
    @SmallTest
    public void testReorderTabGroupByToken_ForwardsToReorderTabGroup() {
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getRelatedTabList(TAB_ID_3)).thenReturn(List.of(mTab3));
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.getTabAt(2)).thenReturn(mTab3);

        assertTrue(
                NestedTabReorderUtils.reorderTabGroup(
                        mTabModel, GROUP_ID, /* toPrevious= */ false));
        verify(mTabModel).moveRelatedTabs(TAB_ID_1, 2);

        // Null model or unknown group returns false
        assertFalse(NestedTabReorderUtils.reorderTabGroup(null, GROUP_ID, /* toPrevious= */ false));
        assertFalse(
                NestedTabReorderUtils.reorderTabGroup(
                        mTabModel, new Token(99L, 99L), /* toPrevious= */ false));
    }

    @Test
    @SmallTest
    public void testReorderTabGroup_PrecededByPinnedTab_ReturnsFalse() {
        when(mPinnedTab.getId()).thenReturn(200);
        when(mPinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getTabAt(0)).thenReturn(mPinnedTab);
        when(mTabModel.getTabAt(1)).thenReturn(mTab1);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));

        assertFalse(
                NestedTabReorderUtils.reorderTabGroup(mTabModel, GROUP_ID, /* toPrevious= */ true));
        verify(mTabModel, never()).moveRelatedTabs(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testReorderTabById_UnpinnedAndPinned() {
        TabListModel pinnedModelList = new TabListModel();
        PropertyModel pinnedModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        PropertyModel pinnedModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .build();
        pinnedModelList.add(new ListItem(TabProperties.UiType.PINNED_TAB, pinnedModel1));
        pinnedModelList.add(new ListItem(TabProperties.UiType.PINNED_TAB, pinnedModel2));

        PropertyModel unpinnedModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_3)
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB, unpinnedModel));

        // Reordering pinned tab
        assertTrue(
                NestedTabReorderUtils.reorderTabById(
                        mTabModel, pinnedModelList, mModelList, TAB_ID_1, /* toPrevious= */ false));
        verify(mTabModel).moveTab(TAB_ID_1, 1);

        // Reordering unpinned tab at index 0 up is at boundary -> returns false
        assertFalse(
                NestedTabReorderUtils.reorderTabById(
                        mTabModel, pinnedModelList, mModelList, TAB_ID_3, /* toPrevious= */ true));

        // Unknown tab returns false
        assertFalse(
                NestedTabReorderUtils.reorderTabById(
                        mTabModel, pinnedModelList, mModelList, 9999, /* toPrevious= */ false));

        // Null model returns false
        assertFalse(
                NestedTabReorderUtils.reorderTabById(
                        null, pinnedModelList, mModelList, TAB_ID_1, /* toPrevious= */ false));
    }

    @Test
    @SmallTest
    public void testUngroupTab() {
        NestedTabReorderUtils.ungroupTab(mTabModel, mTab1, /* trailing= */ true);
        verify(mTabUngrouper).ungroupTabs(List.of(mTab1), /* trailing= */ true, false);

        NestedTabReorderUtils.ungroupTab(mTabModel, mTab2, /* trailing= */ false);
        verify(mTabUngrouper).ungroupTabs(List.of(mTab2), /* trailing= */ false, false);
    }

    @Test
    @SmallTest
    public void testGetTabGroupId() {
        assertNull(NestedTabReorderUtils.getTabGroupId(null));

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        assertEquals(GROUP_ID, NestedTabReorderUtils.getTabGroupId(headerModel));

        PropertyModel childModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        assertEquals(GROUP_ID, NestedTabReorderUtils.getTabGroupId(childModel));

        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        assertNull(NestedTabReorderUtils.getTabGroupId(standaloneModel));
    }

    @Test
    @SmallTest
    public void testIsSolitaryChild() {
        assertFalse(NestedTabReorderUtils.isSolitaryChild(null, null));
        assertFalse(NestedTabReorderUtils.isSolitaryChild(mTabModel, null));

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        assertFalse(NestedTabReorderUtils.isSolitaryChild(mTabModel, headerModel));

        PropertyModel childModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();

        // 2 tabs in group -> not solitary
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));
        assertFalse(NestedTabReorderUtils.isSolitaryChild(mTabModel, childModel));

        // 1 tab in group -> solitary child
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1));
        assertTrue(NestedTabReorderUtils.isSolitaryChild(mTabModel, childModel));

        // Standalone tab (no group ID) -> not solitary child in a group
        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        assertFalse(NestedTabReorderUtils.isSolitaryChild(mTabModel, standaloneModel));
    }
}
