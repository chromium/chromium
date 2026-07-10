// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB_GROUP;

import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link NestedLayoutDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NestedLayoutDelegateUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final int TAB1_ID = 11;
    private static final int TAB2_ID = 22;
    private static final int TAB3_ID = 33;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListMediator mMediator;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private TabModel mTabModel;

    private TabListModel mModelList;
    private NestedLayoutDelegate mDelegate;

    @Before
    public void setUp() {
        mModelList = new TabListModel();
        mDelegate = new NestedLayoutDelegate(mMediator, mModelList);
        when(mMediator.getCurrentTabModelChecked()).thenReturn(mTabModel);
        when(mTabModel.getTabGroupColorWithFallback(any(Token.class)))
                .thenReturn(TabGroupColorId.BLUE);
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab2.getId()).thenReturn(TAB2_ID);
        when(mTab3.getId()).thenReturn(TAB3_ID);
    }

    @Test
    public void testDidChangeTabGroupTitle() {
        mDelegate.didChangeTabGroupTitle(TAB_GROUP_ID, "New Title");
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidChangeTabGroupColor() {
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel child1Model = addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel child2Model = addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        Pair<Integer, Tab> indexAndTab = new Pair<>(0, mTab1);
        when(mMediator.getIndexAndTabForTabGroupId(TAB_GROUP_ID)).thenReturn(indexAndTab);
        when(mTab1.getId()).thenReturn(TAB1_ID);

        mDelegate.didChangeTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        verify(mMediator).updateTabGroupProperties(mTab1, headerModel, TabGroupColorId.BLUE);
        verify(mMediator).updateFaviconForTab(headerModel, mTab1, null, null);
        verify(mMediator).updateDescriptionString(mTab1, headerModel);
        verify(mMediator).updateActionButtonDescriptionString(mTab1, headerModel);
        verify(mMediator).updateThumbnailFetcher(headerModel, TAB1_ID);

        verify(mMediator)
                .updateTabGroupColorViewProvider(any(), eq(child1Model), eq(TabGroupColorId.BLUE));
        verify(mMediator)
                .updateTabGroupColorViewProvider(any(), eq(child2Model), eq(TabGroupColorId.BLUE));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_Collapse() {
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        headerModel.set(TabProperties.IS_COLLAPSED, false);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        assertEquals(3, mModelList.size());

        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2));

        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, true, false);

        assertEquals(true, headerModel.get(TabProperties.IS_COLLAPSED));
        assertEquals(1, mModelList.size());
        assertEquals(TAB_GROUP_ID, mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_Idempotent() {
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        headerModel.set(TabProperties.IS_COLLAPSED, false);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        assertEquals(3, mModelList.size());

        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2));

        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, true, false);
        assertEquals(1, mModelList.size());

        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, true, false);
        assertEquals(1, mModelList.size());
    }

    @Test
    public void testDidChangeTabGroupCollapsed_Expand() {
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        headerModel.set(TabProperties.IS_COLLAPSED, true);

        assertEquals(1, mModelList.size());

        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, false, false);

        assertEquals(false, headerModel.get(TabProperties.IS_COLLAPSED));
        verify(mMediator).insertChildTabs(TAB_GROUP_ID, 0);
    }

    @Test
    public void testDidMoveWithinGroup_Forward() {
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        when(mTabModel.getTabAt(1)).thenReturn(mTab1);

        mDelegate.didMoveWithinGroup(mTab2, 1, 2);

        assertModelListTabIds(TAB1_ID, TAB2_ID, TAB1_ID);
    }

    @Test
    public void testDidMoveWithinGroup_Backward() {
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        when(mTabModel.getTabAt(2)).thenReturn(mTab2);

        mDelegate.didMoveWithinGroup(mTab1, 2, 1);

        assertModelListTabIds(TAB1_ID, TAB2_ID, TAB1_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup() {
        setupTabsInModel(mTab1, mTab3);
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel tab1Model = addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel tab3Model = addTabToModelList(TAB3_ID, TAB_GROUP_ID);

        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));

        mDelegate.didMoveTabOutOfGroup(mTab3, 1);

        verify(mMediator).updateTabGroupHeaderId(TAB_GROUP_ID);
        verify(mMediator).clearTabGroupProperties(tab3Model);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);

        assertModelListTabIds(TAB1_ID, TAB1_ID, TAB3_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup_CollapsedGroup() {
        setupTabsInModel(mTab1, mTab3);
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);

        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));

        mDelegate.didMoveTabOutOfGroup(mTab3, 1);

        verify(mMediator).addTabInfoToModelForTab(mTab3, 1, false);
    }

    @Test
    public void testDidMoveTabOutOfGroup_RepresentativeTab() {
        setupTabsInModel(mTab1, mTab3);
        PropertyModel headerModel = addGroupHeaderToModelList(TAB3_ID, TAB_GROUP_ID);
        PropertyModel tab1Model = addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel tab3Model = addTabToModelList(TAB3_ID, TAB_GROUP_ID);

        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab3);
        when(mTab3.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab3));

        mDelegate.didMoveTabOutOfGroup(mTab1, 1);

        verify(mMediator).updateTabGroupHeaderId(TAB_GROUP_ID);
        verify(mMediator).clearTabGroupProperties(tab1Model);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);

        assertModelListTabIds(TAB1_ID, TAB3_ID, TAB3_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup_LastTab() {
        setupTabsInModel(mTab1);
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel tab1Model = addTabToModelList(TAB1_ID, TAB_GROUP_ID);

        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab1);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of());

        mDelegate.didMoveTabOutOfGroup(mTab1, 1);

        verify(mMediator).updateTabGroupHeaderId(null);
        verify(mMediator).clearTabGroupProperties(tab1Model);
    }

    @Test
    public void testDidMergeTabToGroup() {
        setupTabsInModel(mTab1, mTab2);
        PropertyModel tab1Model = addTabToModelList(TAB1_ID, null);
        PropertyModel tab2Model = addTabToModelList(TAB2_ID, null);

        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2));

        mDelegate.didMergeTabToGroup(mTab1, true);
        mDelegate.didMergeTabToGroup(mTab2, false);

        assertEquals(TAB_GROUP_ID, tab1Model.get(TabProperties.TAB_GROUP_ID));
        assertEquals(TAB_GROUP_ID, tab2Model.get(TabProperties.TAB_GROUP_ID));
        verify(mMediator).updateTabGroupProperties(mTab1, tab1Model, TabGroupColorId.BLUE);
        verify(mMediator).updateTabGroupProperties(mTab2, tab2Model, TabGroupColorId.BLUE);
        verify(mMediator).ensureGroupHeaderExistsInNestedLayout(mTab1, TAB_GROUP_ID, 0);
        verify(mMediator).ensureGroupHeaderExistsInNestedLayout(mTab2, TAB_GROUP_ID, 1);
        verify(mMediator, times(2)).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidMergeTabToGroup_ToExistingGroup() {
        setupTabsInModel(mTab1, mTab2, mTab3);
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);
        PropertyModel tab3Model = addTabToModelList(TAB3_ID, null);

        when(mTab3.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2, mTab3));

        mDelegate.didMergeTabToGroup(mTab3, false);

        assertEquals(TAB_GROUP_ID, tab3Model.get(TabProperties.TAB_GROUP_ID));
        verify(mMediator).updateTabGroupProperties(mTab3, tab3Model, TabGroupColorId.BLUE);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidMergeTabToGroup_CollapsedGroup() {
        setupTabsInModel(mTab1, mTab2, mTab3);
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel tab3Model = addTabToModelList(TAB3_ID, null);

        assertEquals(2, mModelList.size());

        when(mTab3.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2, mTab3));

        mDelegate.didMergeTabToGroup(mTab3, false);

        assertEquals(1, mModelList.size());
        assertEquals(TAB_GROUP_ID, mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID));
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidMoveTabGroup_Forward() {
        addTabToModelList(TAB1_ID, null);
        addGroupHeaderToModelList(TAB2_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);
        addTabToModelList(TAB3_ID, TAB_GROUP_ID);

        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab3.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mMediator.getRelatedTabsForId(TAB2_ID)).thenReturn(List.of(mTab2, mTab3));
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(false);
        setupRepresentativeTab(mTab2, mTab2, 0);
        setupRepresentativeTab(mTab3, mTab2, 0);
        setupRepresentativeTab(mTab1, mTab1, 2);
        when(mTabModel.getTabAt(0)).thenReturn(mTab2);
        when(mTabModel.getTabAt(1)).thenReturn(mTab3);
        when(mTabModel.getTabAt(2)).thenReturn(mTab1);

        mDelegate.didMoveTabGroup(mTab2, 1, 0);

        assertModelListTabIds(TAB2_ID, TAB2_ID, TAB3_ID, TAB1_ID);
    }

    @Test
    public void testDidMoveTabGroup_Backward() {
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB3_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, null);

        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab3.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab3));
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(false);
        setupRepresentativeTab(mTab2, mTab2, 0);
        setupRepresentativeTab(mTab1, mTab1, 1);
        setupRepresentativeTab(mTab3, mTab1, 1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab2);
        when(mTabModel.getTabAt(1)).thenReturn(mTab1);
        when(mTabModel.getTabAt(2)).thenReturn(mTab3);

        mDelegate.didMoveTabGroup(mTab1, 0, 1);

        assertModelListTabIds(TAB2_ID, TAB1_ID, TAB1_ID, TAB3_ID);
    }

    @Test
    public void testDidCreateNewGroup() {
        PropertyModel tab1Model = addTabToModelList(TAB1_ID, null);
        // Add tab1Model a second time. This allows get(destUiIndex + 1) to return tab1Model
        // when destUiIndex is 0, simulating the list shifting by one position after the group
        // header is added.
        mModelList.add(new ListItem(UiType.TAB, tab1Model));

        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mMediator.ensureGroupHeaderExistsInNestedLayout(eq(mTab1), eq(TAB_GROUP_ID), eq(0)))
                .thenReturn(true);

        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        assertEquals(TAB_GROUP_ID, tab1Model.get(TabProperties.TAB_GROUP_ID));
        verify(mMediator).updateTabGroupProperties(mTab1, tab1Model, TabGroupColorId.BLUE);
        assertEquals(2, mModelList.size());
    }

    @Test
    public void testDidCreateNewGroup_AlreadyExists() {
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel tab1Model = addTabToModelList(TAB1_ID, TAB_GROUP_ID);

        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mMediator.ensureGroupHeaderExistsInNestedLayout(eq(mTab1), eq(TAB_GROUP_ID), anyInt()))
                .thenReturn(false);

        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        verify(mMediator, never()).updateTabGroupProperties(any(), any(), anyInt());
        verify(mMediator, never()).clearTabGroupProperties(any());
    }

    @Test
    public void testDidRemoveTabGroup() {
        addGroupHeaderToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);

        assertEquals(2, mModelList.size());

        mDelegate.didRemoveTabGroup(TAB1_ID, TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);

        assertEquals(1, mModelList.size());
        assertEquals(TabProperties.UiType.TAB, mModelList.get(0).type);
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidRemoveTabGroup_HeaderDoesNotExist() {
        addTabToModelList(TAB1_ID, null);

        assertEquals(1, mModelList.size());

        mDelegate.didRemoveTabGroup(TAB1_ID, TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);

        assertEquals(1, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
    }

    private PropertyModel addTabToModelList(int tabId, @Nullable Token tabGroupId) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TAB)
                        .with(TabProperties.TAB_ID, tabId)
                        .with(TabProperties.TAB_GROUP_ID, tabGroupId)
                        .build();
        mModelList.add(new ListItem(UiType.TAB, model));
        return model;
    }

    private PropertyModel addGroupHeaderToModelList(int tabId, Token tabGroupId) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TAB_GROUP)
                        .with(TabProperties.TAB_ID, tabId)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId)
                        .build();
        mModelList.add(new ListItem(UiType.TAB, model));
        return model;
    }

    private void setupTabsInModel(Tab... tabs) {
        when(mTabModel.getCount()).thenReturn(tabs.length);
        for (int i = 0; i < tabs.length; i++) {
            when(mTabModel.getTabAt(i)).thenReturn(tabs[i]);
            when(mTabModel.getTabById(tabs[i].getId())).thenReturn(tabs[i]);
        }
    }

    private void assertModelListTabIds(int... expectedTabIds) {
        assertEquals(expectedTabIds.length, mModelList.size());
        for (int i = 0; i < expectedTabIds.length; i++) {
            assertEquals(expectedTabIds[i], mModelList.get(i).model.get(TabProperties.TAB_ID));
        }
    }

    private void setupRepresentativeTab(Tab tab, Tab representativeTab, int index) {
        when(mTabModel.representativeIndexOf(tab)).thenReturn(index);
        when(mTabModel.getRepresentativeTabAt(index)).thenReturn(representativeTab);
    }
}
