// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGridDialogHandler;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

/** Unit tests for {@link FlatLayoutDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FlatLayoutDelegateUnitTest {
    private static final int TAB1_ID = 1;
    private static final int TAB2_ID = 2;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final Token TAB_GROUP_ID_2 = new Token(3L, 4L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListMediator mMediator;
    @Mock private TabGridDialogHandler mTabGridDialogHandler;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private TabListModel mModelList;
    private FlatLayoutDelegate mDelegate;

    @Before
    public void setUp() {
        mModelList = new TabListModel();
        mDelegate = new FlatLayoutDelegate(mMediator, mModelList, mTabGridDialogHandler);

        when(mMediator.getCurrentTabModelChecked()).thenReturn(mTabModel);
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab2.getId()).thenReturn(TAB2_ID);
    }

    @Test
    public void testDidChangeTabGroupTitle_NoOp() {
        mDelegate.didChangeTabGroupTitle(TAB_GROUP_ID, "New Title");

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidChangeTabGroupColor_NoOp() {
        mDelegate.didChangeTabGroupColor(TAB_GROUP_ID, 1);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidChangeTabGroupCollapsed_NoOp() {
        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, true, false);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidMoveWithinGroup_Forward() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        when(mTabModel.getTabAt(0)).thenReturn(mTab2);

        // Execute moving mTab1 from position 0 to position 1.
        mDelegate.didMoveWithinGroup(mTab1, 0, 1);

        assertModelListTabIds(TAB2_ID, TAB1_ID);
    }

    @Test
    public void testDidMoveWithinGroup_Backward() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        when(mTabModel.getTabAt(1)).thenReturn(mTab1);

        // Execute moving mTab2 from position 1 to position 0.
        mDelegate.didMoveWithinGroup(mTab2, 1, 0);

        assertModelListTabIds(TAB2_ID, TAB1_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup_Dialog() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab2);

        // Execute moving mTab1 out.
        mDelegate.didMoveTabOutOfGroup(mTab1, 0);

        assertModelListTabIds(TAB2_ID);
        verify(mTabGridDialogHandler).updateDialogContent(TAB2_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup_Dialog_LastTab() {
        addTabsToModelList(TAB1_ID);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab1);

        // Execute moving mTab1 (last tab) out.
        mDelegate.didMoveTabOutOfGroup(mTab1, 0);

        assertModelListTabIds();
        verify(mTabGridDialogHandler).updateDialogContent(Tab.INVALID_TAB_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup_Strip() {
        // Recreate delegate without dialog handler to simulate Strip.
        mDelegate = new FlatLayoutDelegate(mMediator, mModelList, null);
        addTabsToModelList(1, 2);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab2);

        mDelegate.didMoveTabOutOfGroup(mTab1, 0);

        assertModelListTabIds(2);
    }

    @Test
    public void testDidMoveTabOutOfGroup_Strip_Undo() {
        // Recreate delegate without dialog handler to simulate Strip.
        mDelegate = new FlatLayoutDelegate(mMediator, mModelList, null);
        addTabsToModelList(TAB2_ID);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab2);
        mDelegate.didMoveTabOutOfGroup(mTab1, 0);

        // Verify no-op.
        assertModelListTabIds(TAB2_ID);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidMergeTabToGroup() {
        // Setup mModelList with a tab that belongs to the same group as mTab2.
        addTabsToModelList(TAB1_ID);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);

        // Execute merging mTab2.
        mDelegate.didMergeTabToGroup(mTab2, false);

        verify(mMediator).addObserversForTab(mTab2);
        verify(mMediator).onTabAdded(mTab2);
        verify(mTabGridDialogHandler).updateDialogContent(TAB1_ID);
    }

    @Test
    public void testDidMergeTabToGroup_DifferentGroup() {
        // Setup mModelList with a tab that belongs to a different group.
        addTabsToModelList(TAB1_ID);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID_2);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);

        // Execute merging mTab2.
        mDelegate.didMergeTabToGroup(mTab2, false);

        verify(mMediator).getCurrentTabModelChecked();
        verifyNoMoreInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidMergeTabToGroup_EmptyModelList() {
        // Empty model list.
        mDelegate.didMergeTabToGroup(mTab2, false);

        verify(mMediator).getCurrentTabModelChecked();
        verifyNoMoreInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidMoveTabGroup_NoOp() {
        mDelegate.didMoveTabGroup(mTab1, 0, 1);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidCreateNewGroup_NoOp() {
        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testDidRemoveTabGroup_NoOp() {
        mDelegate.didRemoveTabGroup(1, null, TabGroupObserver.DidRemoveTabGroupReason.MERGE);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
        verifyNoInteractions(mTabGridDialogHandler);
    }

    @Test
    public void testGetInsertionIndexOfTab() {
        addTabsToModelList(TAB1_ID);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(Arrays.asList(mTab1, mTab2));

        int insertionIndex = mDelegate.getInsertionIndexOfTab(mTab2);

        assertEquals(1, insertionIndex);
    }

    @Test
    public void testGetInsertionIndexOfTab_NullTab() {
        int insertionIndex = mDelegate.getInsertionIndexOfTab(null);
        assertEquals(TabModel.INVALID_TAB_INDEX, insertionIndex);
    }

    @Test
    public void testGetInsertionIndexOfTab_EmptyModelList() {
        int insertionIndex = mDelegate.getInsertionIndexOfTab(mTab2);
        assertEquals(TabModel.INVALID_TAB_INDEX, insertionIndex);
    }

    private void addTabsToModelList(int... tabIds) {
        for (int tabId : tabIds) {
            PropertyModel model = new PropertyModel(TabProperties.ALL_KEYS_TAB_GRID);
            model.set(TabProperties.TAB_ID, tabId);
            mModelList.add(new ListItem(TabProperties.UiType.TAB, model));
        }
    }

    private void assertModelListTabIds(int... expectedTabIds) {
        assertEquals(expectedTabIds.length, mModelList.size());
        for (int i = 0; i < expectedTabIds.length; i++) {
            assertEquals(expectedTabIds[i], mModelList.get(i).model.get(TabProperties.TAB_ID));
        }
    }
}
