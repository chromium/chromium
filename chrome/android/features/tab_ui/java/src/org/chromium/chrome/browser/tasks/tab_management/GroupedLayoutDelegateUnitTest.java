// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.ARCHIVED_TAB_GROUP;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

/** Unit tests for {@link GroupedLayoutDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupedLayoutDelegateUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListMediator mMediator;
    @Mock private ThumbnailProvider mThumbnailProvider;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private TabModel mTabModel;

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    private TabListModel mModelList;
    private GroupedLayoutDelegate mDelegate;

    @Before
    public void setUp() {
        mModelList = new TabListModel();
        mDelegate = new GroupedLayoutDelegate(mMediator, mModelList, mThumbnailProvider);
        when(mMediator.getCurrentTabModelChecked()).thenReturn(mTabModel);
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab2.getId()).thenReturn(TAB2_ID);
    }

    @Test
    public void testDidChangeTabGroupTitle() {
        String newTitle = "New Title";
        mDelegate.didChangeTabGroupTitle(TAB_GROUP_ID, newTitle);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidChangeTabGroupColor() {
        int index = 0;
        PropertyModel model = createAndAddPropertyModel(Tab.INVALID_TAB_ID);
        when(mMediator.getIndexAndTabForTabGroupId(TAB_GROUP_ID))
                .thenReturn(new Pair<>(index, mTab1));

        mDelegate.didChangeTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        verify(mMediator).updateTabGroupProperties(mTab1, model, TabGroupColorId.BLUE);
        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
        verify(mMediator).updateDescriptionString(mTab1, model);
        verify(mMediator).updateActionButtonDescriptionString(mTab1, model);
        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
    }

    @Test
    public void testDidChangeTabGroupColor_NotFound() {
        when(mMediator.getIndexAndTabForTabGroupId(TAB_GROUP_ID)).thenReturn(null);

        mDelegate.didChangeTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        verify(mMediator, never()).updateTabGroupProperties(any(), any(), anyInt());
    }

    @Test
    public void testDidChangeTabGroupCollapsed_NoOp() {
        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, true, false);
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testDidMoveWithinGroup() {
        int index = 0;
        PropertyModel model = createAndAddPropertyModel(Tab.INVALID_TAB_ID);
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB1_ID)).thenReturn(index);
        setupRepresentativeTab(mTab1, mTab1, 1);

        mDelegate.didMoveWithinGroup(mTab1, 0, 1);

        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
    }

    @Test
    public void testDidMoveWithinGroup_NotFound() {
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB1_ID))
                .thenReturn(TabModel.INVALID_TAB_INDEX);

        mDelegate.didMoveWithinGroup(mTab1, 0, 1);

        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
    }

    @Test
    public void testDidMoveTabOutOfGroup_NewCard() {
        when(mTabModel.getTabCountForGroup(null)).thenReturn(1);
        setupRepresentativeTab(mTab1, mTab1, 2);
        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab2);

        mDelegate.didMoveTabOutOfGroup(mTab1, 1);

        // indexOfNthTabCard returns 0 for an empty list.
        verify(mMediator).addTabCardToModel(mTab1, 0);
        verify(mMediator).updateTab(0, mTab2, true, false);
    }

    @Test
    public void testDidMoveTabOutOfGroup_LastTab_RemovesCard() {
        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab2);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(2);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        // Add to mModelList so indexFromTabId finds it.
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);

        mDelegate.didMoveTabOutOfGroup(mTab1, 1);

        assertEquals(0, mModelList.size());
        verify(mMediator, never()).updateTab(anyInt(), any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testDidMoveTabOutOfGroup_Fallback() {
        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab2);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(2);

        Token differentGroupId = new Token(3L, 4L);
        when(mTab2.getTabGroupId()).thenReturn(differentGroupId);

        mDelegate.didMoveTabOutOfGroup(mTab1, 1);

        // indexOfNthTabCard returns 0 for an empty list.
        verify(mMediator).updateTab(0, mTab2, true, false);
        verify(mMediator, never()).addTabCardToModel(any(), anyInt());
    }

    @Test
    public void testDidMergeTabToGroup() {
        setupTabsInModel(mTab1, mTab2);
        setupRepresentativeTab(mTab1, mTab1, 0);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(Arrays.asList(mTab1, mTab2));

        PropertyModel model1 = createAndAddPropertyModel(TAB1_ID);
        model1.set(TabProperties.TITLE, "Tab 1");
        PropertyModel model2 = createAndAddPropertyModel(TAB2_ID);
        model2.set(TabProperties.TITLE, "Tab 2");

        mDelegate.didMergeTabToGroup(mTab1, true);

        assertEquals(1, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));

        verify(mMediator).updateTab(0, mTab1, true, false);
    }

    @Test
    public void testDidMergeTabToGroup_UpdatesCards() {
        setupTabsInModel(mTab1, mTab2);
        setupRepresentativeTab(mTab2, mTab2, 0);
        when(mMediator.getRelatedTabsForId(TAB2_ID)).thenReturn(Arrays.asList(mTab1, mTab2));
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB2_ID);
        when(mTabModel.getTabById(TAB2_ID)).thenReturn(mTab2);

        // Only mTab1 is in the model list
        PropertyModel model1 = createAndAddPropertyModel(TAB1_ID);

        mDelegate.didMergeTabToGroup(mTab2, false);

        assertEquals(1, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));

        verify(mMediator).updateTab(0, mTab2, true, false);
    }

    @Test
    public void testDidMoveTabGroup() {
        // Setup mModelList: [TAB2, TAB1].
        PropertyModel model2 = createAndAddPropertyModel(TAB2_ID);
        PropertyModel model1 = createAndAddPropertyModel(TAB1_ID);

        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));

        // After move, mTab1 is at 0, mTab2 is at 1. We mock the destination tab for calculating new
        // position.
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);

        setupRepresentativeTab(mTab1, mTab1, 0);
        setupRepresentativeTab(mTab2, mTab2, 1);

        mDelegate.didMoveTabGroup(mTab1, 1, 0);

        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidMoveTabGroup_NonExistentTab() {
        Tab newTab = org.mockito.Mockito.mock(Tab.class);
        when(newTab.getId()).thenReturn(999);
        when(newTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        when(mMediator.getRelatedTabsForId(999)).thenReturn(Arrays.asList(newTab));
        when(mTabModel.getRelatedTabList(999)).thenReturn(Arrays.asList(newTab));
        when(mTabModel.getTabAt(2)).thenReturn(newTab);

        setupRepresentativeTab(newTab, newTab, 1);
        when(mTabModel.getRepresentativeTabAt(2)).thenReturn(newTab);
        when(mTabModel.representativeIndexOf(newTab)).thenReturn(2);

        // mModelList is empty at this point, so the tab is non-existent.
        mDelegate.didMoveTabGroup(newTab, 2, 1);

        // Verify it doesn't crash and we don't try to update a tab.
        verify(mMediator, never()).updateTab(anyInt(), any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testDidCreateNewGroup() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        setupRepresentativeTab(mTab1, mTab1, 1);
        when(mTabModel.getTabGroupColorWithFallback(TAB_GROUP_ID)).thenReturn(TabGroupColorId.BLUE);

        PropertyModel model1 = createAndAddPropertyModel(TAB1_ID);

        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        verify(mMediator).updateTabGroupProperties(mTab1, model1, TabGroupColorId.BLUE);
        verify(mMediator).updateFaviconForTab(model1, mTab1, null, null);
    }

    @Test
    public void testDidCreateNewGroup_ModelNotFound() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        setupRepresentativeTab(mTab1, mTab1, 1);

        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        verify(mMediator, never()).updateTabGroupProperties(any(), any(), anyInt());
        verify(mMediator, never()).updateFaviconForTab(any(), any(), any(), any());
    }

    @Test
    public void testGetInsertionIndexOfTab() {
        createAndAddPropertyModel(TAB1_ID);

        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(2);
        setupRepresentativeTab(mTab1, mTab1, 0);
        setupRepresentativeTab(mTab2, mTab2, 1);

        int insertionIndex1 = mDelegate.getInsertionIndexOfTab(mTab1);
        int insertionIndex2 = mDelegate.getInsertionIndexOfTab(mTab2);

        assertEquals(0, insertionIndex1);
        assertEquals(1, insertionIndex2);
    }

    @Test
    public void testGetInsertionIndexOfTab_WithArchivedTabGroup() {
        // Add an archived group card at index 0.
        PropertyModel archivedModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, ARCHIVED_TAB_GROUP)
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB, archivedModel));

        // Add a regular tab model card at index 1.
        createAndAddPropertyModel(TAB1_ID);

        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(1);
        setupRepresentativeTab(mTab1, mTab1, 0);

        int insertionIndex = mDelegate.getInsertionIndexOfTab(mTab1);

        // Insertion index should be offset by 1 (due to archived card) and return 1.
        assertEquals(1, insertionIndex);
    }

    @Test
    public void testGetInsertionIndexOfTab_NullTab() {
        int insertionIndex = mDelegate.getInsertionIndexOfTab(null);
        assertEquals(TabModel.INVALID_TAB_INDEX, insertionIndex);
    }

    private PropertyModel createAndAddPropertyModel(int tabId) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TAB)
                        .with(TabProperties.TAB_ID, tabId)
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB, model));
        return model;
    }

    private void setupTabsInModel(Tab... tabs) {
        for (int i = 0; i < tabs.length; i++) {
            when(mTabModel.indexOf(tabs[i])).thenReturn(i);
            when(mTabModel.getTabAtChecked(i)).thenReturn(tabs[i]);
            when(mTabModel.getTabAt(i)).thenReturn(tabs[i]);
        }
    }

    private void setupRepresentativeTab(Tab tab, Tab representativeTab, int index) {
        when(mTabModel.representativeIndexOf(tab)).thenReturn(index);
        when(mTabModel.getRepresentativeTabAt(index)).thenReturn(representativeTab);
    }
}
