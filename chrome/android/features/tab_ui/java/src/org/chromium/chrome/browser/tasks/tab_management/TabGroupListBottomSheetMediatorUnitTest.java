// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ROW_CLICK_RUNNABLE;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason.INTERACTION_COMPLETE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.RowType;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupListBottomSheetCoordinatorDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabMovedCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabGroupListBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupListBottomSheetMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TabGroupListBottomSheetCoordinatorDelegate mDelegate;
    @Mock private TabGroupModelFilter mFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private TabList mTabList;
    @Mock private TabGroupCreationCallback mTabGroupCreationCallback;
    @Mock private TabMovedCallback mTabMovedCallback;
    @Mock private FaviconResolver mFaviconResolver;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private SavedTabGroup mSavedTabGroup1;
    @Mock private SavedTabGroup mSavedTabGroup2;
    @Mock private SavedTabGroup mSavedTabGroup3;
    @Mock private SavedTabGroupTab mSavedTabGroupTab1;
    @Mock private SavedTabGroupTab mSavedTabGroupTab2;
    @Mock private SavedTabGroupTab mSavedTabGroupTab3;
    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private ModelList mModelList;
    private TabGroupListBottomSheetMediator mMediator;
    private Token mToken1;
    private Token mToken2;
    private Token mToken3;

    @Before
    public void setUp() {
        mModelList = spy(new ModelList());
        mMediator =
                new TabGroupListBottomSheetMediator(
                        mModelList,
                        mFilter,
                        mTabGroupCreationCallback,
                        mTabMovedCallback,
                        mFaviconResolver,
                        mTabGroupSyncService,
                        mBottomSheetController,
                        mDelegate,
                        /* supportsShowNewGroup= */ true);
        when(mTabList.getCount()).thenReturn(3);

        when(mTabList.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTabList.getTabAtChecked(1)).thenReturn(mTab2);
        when(mTabList.getTabAtChecked(2)).thenReturn(mTab3);

        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);
        when(mFilter.getTabModel()).thenReturn(mTabModel);
        when(mFilter.getTabUngrouper()).thenReturn(mTabUngrouper);

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);
        when(mTab3.getId()).thenReturn(3);

        mToken1 = Token.createRandom();
        mToken2 = Token.createRandom();
        mToken3 = Token.createRandom();
        when(mFilter.getAllTabGroupIds()).thenReturn(Set.of(mToken1, mToken2));

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);
        when(mTab3.getId()).thenReturn(3);

        when(mTab1.getTabGroupId()).thenReturn(mToken1);
        when(mTab2.getTabGroupId()).thenReturn(mToken2);
        when(mTab3.getTabGroupId()).thenReturn(mToken3);

        when(mTab1.isClosing()).thenReturn(true);
        when(mTab2.isClosing()).thenReturn(true);
        when(mTab3.isClosing()).thenReturn(true);

        mSavedTabGroup1.localId = new LocalTabGroupId(mToken1);
        mSavedTabGroup2.localId = new LocalTabGroupId(mToken2);
        mSavedTabGroup3.localId = null;

        mSavedTabGroup1.updateTimeMs = 1L;
        mSavedTabGroup2.updateTimeMs = 2L;
        mSavedTabGroup3.updateTimeMs = 3L;

        mSavedTabGroup1.savedTabs = List.of(mSavedTabGroupTab1);
        mSavedTabGroup2.savedTabs = List.of(mSavedTabGroupTab2);
        mSavedTabGroup3.savedTabs = List.of(mSavedTabGroupTab3);

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {"1", "2", "3"});
        when(mTabGroupSyncService.getGroup("1")).thenReturn(mSavedTabGroup1);
        when(mTabGroupSyncService.getGroup("2")).thenReturn(mSavedTabGroup2);
        when(mTabGroupSyncService.getGroup("3")).thenReturn(mSavedTabGroup3);
    }

    @Test
    public void testRequestShowContent_delegateRejects() {
        when(mDelegate.requestShowContent()).thenReturn(false);

        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));

        verify(mBottomSheetController, never()).addObserver(any());
    }

    @Test
    public void testRequestShowContent_delegateAccepts() {
        when(mDelegate.requestShowContent()).thenReturn(true);

        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));

        InOrder inOrder = inOrder(mModelList, mDelegate, mBottomSheetController);
        // Verify that model list is populated before requesting show content
        inOrder.verify(mModelList).clear();
        inOrder.verify(mModelList, times(3)).add(any());
        inOrder.verify(mDelegate).requestShowContent();
        inOrder.verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        assertEquals(3, mModelList.size());
        assertEquals(RowType.NEW_GROUP, mModelList.get(0).type);
    }

    @Test
    public void testHide() {
        mMediator.hide(INTERACTION_COMPLETE);
        verify(mDelegate).hide(INTERACTION_COMPLETE);
    }

    @Test
    public void testBottomSheetObserver_onSheetClosed() {
        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mBottomSheetController).removeObserver(observer);
        assertTrue(mModelList.isEmpty());
    }

    @Test
    public void testBottomSheetObserver_onSheetStateChanged() {
        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        verify(mBottomSheetController, never()).removeObserver(any());
        assertFalse(mModelList.isEmpty());
    }

    @Test
    public void testBottomSheetObserver_onSheetStateChanged_hidden() {
        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        BottomSheetObserver observer = mBottomSheetObserverCaptor.getValue();
        observer.onSheetStateChanged(SheetState.HIDDEN, INTERACTION_COMPLETE);

        verify(mBottomSheetController).removeObserver(observer);
        assertTrue(mModelList.isEmpty());
    }

    @Test
    public void testPopulateList_withGroups() {
        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));
        verify(mTabGroupSyncService).getAllGroupIds();

        // New group row, plus two rows representing existing groups.
        assertEquals(3, mModelList.size());
        assertEquals(RowType.NEW_GROUP, mModelList.get(0).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(1).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(2).type);

        assertEquals(
                mSavedTabGroup2.updateTimeMs,
                mModelList.get(1).model.get(TabGroupRowProperties.TIMESTAMP_EVENT).timestampMs);
        assertEquals(
                mSavedTabGroup1.updateTimeMs,
                mModelList.get(2).model.get(TabGroupRowProperties.TIMESTAMP_EVENT).timestampMs);
    }

    @Test
    public void testPopulateList_incognito() {
        mMediator =
                new TabGroupListBottomSheetMediator(
                        mModelList,
                        mFilter,
                        mTabGroupCreationCallback,
                        mTabMovedCallback,
                        mFaviconResolver,
                        /* tabGroupSyncService= */ null,
                        mBottomSheetController,
                        mDelegate,
                        /* supportsShowNewGroup= */ true);

        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));
        verify(mTabGroupSyncService, never()).getAllGroupIds();

        // New group row, plus two rows representing existing groups.
        assertEquals(3, mModelList.size());
        assertEquals(RowType.NEW_GROUP, mModelList.get(0).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(1).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(2).type);

        assertNull(mModelList.get(1).model.get(TabGroupRowProperties.TIMESTAMP_EVENT));
        assertNull(mModelList.get(2).model.get(TabGroupRowProperties.TIMESTAMP_EVENT));
    }

    @Test
    public void testPopulateList_noHiddenGroups() {
        mSavedTabGroup3.localId = new LocalTabGroupId(mToken3);

        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));
        verify(mTabGroupSyncService).getAllGroupIds();

        // New group row, plus three rows representing existing groups.
        assertEquals(4, mModelList.size());
        assertEquals(RowType.NEW_GROUP, mModelList.get(0).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(1).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(2).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(3).type);

        assertEquals(
                mSavedTabGroup3.updateTimeMs,
                mModelList.get(1).model.get(TabGroupRowProperties.TIMESTAMP_EVENT).timestampMs);
        assertEquals(
                mSavedTabGroup2.updateTimeMs,
                mModelList.get(2).model.get(TabGroupRowProperties.TIMESTAMP_EVENT).timestampMs);
        assertEquals(
                mSavedTabGroup1.updateTimeMs,
                mModelList.get(3).model.get(TabGroupRowProperties.TIMESTAMP_EVENT).timestampMs);
    }

    @Test
    public void testPopulateList_tabsAreSubsetOfSameGroup() {
        mSavedTabGroup3.localId = new LocalTabGroupId(mToken3);

        when(mTab1.getTabGroupId()).thenReturn(mToken1);
        when(mTab2.getTabGroupId()).thenReturn(mToken1);

        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));
        verify(mTabGroupSyncService).getAllGroupIds();

        // New group row, plus one row representing an existing group. The rest are filtered out.
        assertEquals(2, mModelList.size());
        assertEquals(RowType.NEW_GROUP, mModelList.get(0).type);
        assertEquals(RowType.EXISTING_GROUP, mModelList.get(1).type);

        assertEquals(
                mSavedTabGroup3.updateTimeMs,
                mModelList.get(1).model.get(TabGroupRowProperties.TIMESTAMP_EVENT).timestampMs);
    }

    @Test
    public void testCreateNewGroup() {
        when(mTab1.getTabGroupId()).thenReturn(Token.createRandom());
        when(mDelegate.requestShowContent()).thenReturn(true);

        List<Tab> tabs = Arrays.asList(mTab1, mTab2);
        mMediator.requestShowContent(tabs);

        // Simulate clicking the "New Group" row.
        mModelList.get(0).model.get(ROW_CLICK_RUNNABLE).run();

        verify(mFilter).mergeListOfTabsToGroup(eq(tabs), eq(mTab1), anyBoolean());
        verify(mDelegate).hide(INTERACTION_COMPLETE);
        verify(mTabGroupCreationCallback).onTabGroupCreated(any());
    }

    @Test
    public void testCreateNewGroup_singleTabInTabGroup() {
        when(mTab1.getTabGroupId()).thenReturn(mToken1);
        when(mDelegate.requestShowContent()).thenReturn(true);

        List<Tab> tabs = Arrays.asList(mTab1);
        mMediator.requestShowContent(tabs);

        // Simulate clicking the "New Group" row.
        mModelList.get(0).model.get(ROW_CLICK_RUNNABLE).run();

        verify(mTabMovedCallback).onTabMoved();
        verify(mTabUngrouper).ungroupTabs(eq(tabs), anyBoolean(), anyBoolean());
        verify(mFilter).createSingleTabGroup(mTab1);
        verify(mDelegate).hide(INTERACTION_COMPLETE);
        verify(mTabGroupCreationCallback).onTabGroupCreated(any());
    }

    @Test(expected = AssertionError.class)
    public void testCreateNewGroup_emptyTabs() {
        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(new ArrayList<>());

        // Simulate clicking the "New Group" row with an empty tab list.
        Runnable onClickRunnable = mModelList.get(0).model.get(ROW_CLICK_RUNNABLE);
        onClickRunnable.run();
    }

    @Test
    public void testPopulateList_noNewGroupRow() {
        mMediator =
                new TabGroupListBottomSheetMediator(
                        mModelList,
                        mFilter,
                        mTabGroupCreationCallback,
                        mTabMovedCallback,
                        mFaviconResolver,
                        mTabGroupSyncService,
                        mBottomSheetController,
                        mDelegate,
                        /* supportsShowNewGroup= */ false);
        when(mDelegate.requestShowContent()).thenReturn(true);
        mMediator.requestShowContent(Arrays.asList(mTab1, mTab2));
        assertEquals(2, mModelList.size());
    }

    @Test
    public void testPopulateList_noNewGroupRow_multipleTabsInSameGroup() {
        mMediator =
                new TabGroupListBottomSheetMediator(
                        mModelList,
                        mFilter,
                        mTabGroupCreationCallback,
                        mTabMovedCallback,
                        mFaviconResolver,
                        mTabGroupSyncService,
                        mBottomSheetController,
                        mDelegate,
                        /* supportsShowNewGroup= */ true);
        when(mDelegate.requestShowContent()).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(mToken1);
        when(mTab2.getTabGroupId()).thenReturn(mToken1);

        List<Tab> list = Arrays.asList(mTab1, mTab2);
        mMediator.requestShowContent(list);
        assertEquals(1, mModelList.size());
    }

    @Test
    public void testPopulateList_showNewGroupRow_singleTabInGroup() {
        mMediator =
                new TabGroupListBottomSheetMediator(
                        mModelList,
                        mFilter,
                        mTabGroupCreationCallback,
                        mTabMovedCallback,
                        mFaviconResolver,
                        mTabGroupSyncService,
                        mBottomSheetController,
                        mDelegate,
                        /* supportsShowNewGroup= */ true);
        when(mDelegate.requestShowContent()).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(mToken1);
        when(mTab2.getTabGroupId()).thenReturn(mToken1);

        List<Tab> list = List.of(mTab1);
        mMediator.requestShowContent(list);
        assertEquals(1, mModelList.size());
        assertEquals(RowType.NEW_GROUP, mModelList.get(0).type);
    }

    @Test
    public void testPopulateList_showNewGroupRow_multipleTabGroups() {
        mMediator =
                new TabGroupListBottomSheetMediator(
                        mModelList,
                        mFilter,
                        mTabGroupCreationCallback,
                        mTabMovedCallback,
                        mFaviconResolver,
                        mTabGroupSyncService,
                        mBottomSheetController,
                        mDelegate,
                        /* supportsShowNewGroup= */ true);
        when(mDelegate.requestShowContent()).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(mToken1);
        when(mTab2.getTabGroupId()).thenReturn(mToken2);

        List<Tab> list = Arrays.asList(mTab1, mTab2);
        mMediator.requestShowContent(list);
        assertEquals(3, mModelList.size());
        assertEquals(RowType.NEW_GROUP, mModelList.get(0).type);
    }
}
