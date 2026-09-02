// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.util.Pair;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGridAccessibilityHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tabs.TabAlert;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link FlatLayoutDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FlatLayoutDelegateUnitTest {
    private static final int TAB1_ID = 1;
    private static final int TAB2_ID = 2;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final Token TAB_GROUP_ID_2 = new Token(3L, 4L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListMediator mMediator;
    @Mock private TabGridAccessibilityHelper mAccessibilityHelper;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private NavigationHandle mNavigationHandle;
    @Mock private TabFaviconFetcher mFaviconFetcher;

    private TabListModel mModelList;
    private FlatLayoutDelegate mDelegate;

    @Before
    public void setUp() {
        mModelList = new TabListModel();
        mDelegate = new FlatLayoutDelegate(mMediator, mModelList);

        when(mMediator.getCurrentTabModelChecked()).thenReturn(mTabModel);
        when(mMediator.isShowingTabs()).thenReturn(true);
        when(mMediator.supportsTabLoadingState()).thenReturn(true);
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab1.isInitialized()).thenReturn(true);
        when(mTab2.getId()).thenReturn(TAB2_ID);
        when(mTab2.isInitialized()).thenReturn(true);
    }

    @Test
    public void testRequiresThumbnailUpdateOnDeselect() {
        assertFalse(mDelegate.requiresThumbnailUpdateOnDeselect());
    }

    @Test
    public void testRequiresThumbnailUpdateOnSelect() {
        assertTrue(mDelegate.requiresThumbnailUpdateOnSelect());
    }

    @Test
    public void testRecordTabSelection() {
        when(mMediator.getComponentId()).thenReturn(TabComponentId.TAB_GRID_DIALOG_FROM_STRIP);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);

        var userActionTester = new UserActionTester();
        mDelegate.recordTabSelection(TAB1_ID);

        assertTrue(
                userActionTester.getActions().contains("MobileTabSwitched.TabGridDialogFromStrip"));
        userActionTester.tearDown();
    }

    @Test
    public void testGetAlertState() {
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);
        PropertyModel model = new PropertyModel(TabProperties.ALL_KEYS_TAB_GRID);
        @TabAlert int state = mDelegate.getAlertState(mTab1, model);
        assertEquals(TabAlert.AUDIO_PLAYING, state);
    }

    @Test
    public void testGetInsertionIndexOfTab() {
        addTabsToModelList(TAB1_ID);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab2));

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

    @Test
    public void testOnTabAdded_NewTab() {
        addTabsToModelList(TAB1_ID);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab2));

        int index = mDelegate.onTabAdded(mTab2);

        assertEquals(1, index);
        verify(mMediator).addTabCardToModel(mTab2, 1);
    }

    @Test
    public void testOnTabAdded_AlreadyInModel() {
        addTabsToModelList(TAB1_ID, TAB2_ID);

        int index = mDelegate.onTabAdded(mTab2);

        assertEquals(1, index);
        verify(mMediator, never()).addTabCardToModel(any(), anyInt());
    }

    @Test
    public void testDidAddTab() {
        addTabsToModelList(TAB1_ID);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab2));

        mDelegate.didAddTab(mTab2, TabLaunchType.FROM_CHROME_UI);

        verify(mMediator).addTabCardToModel(mTab2, 1);
    }

    @Test
    public void testTabClosureUndone() {
        addTabsToModelList(TAB1_ID);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab2));

        mDelegate.tabClosureUndone(mTab2);

        verify(mMediator).addTabCardToModel(mTab2, 1);
    }

    @Test
    public void testOnFaviconUpdated() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;

        mDelegate.onFaviconUpdated(mTab1, null, null);

        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
    }

    @Test
    public void testOnFaviconUpdated_NotFound() {
        mDelegate.onFaviconUpdated(mTab1, null, null);

        verify(mMediator, never()).updateFaviconForTab(any(), any(), any(), any());
    }

    @Test
    public void testOnUrlUpdated() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        when(mMediator.getDomainForTab(mTab1, model)).thenReturn("example.com");

        mDelegate.onUrlUpdated(mTab1);

        assertEquals("example.com", model.get(TabProperties.URL_DOMAIN));
        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
    }

    @Test
    public void testOnUrlUpdated_NotFound() {
        mDelegate.onUrlUpdated(mTab1);

        verify(mMediator, never()).getDomainForTab(any(), any());
        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
        verify(mMediator, never()).updateFaviconForTab(any(), any(), any(), any());
    }

    @Test
    public void testOnAlertStateChanged() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);

        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);

        assertEquals(TabAlert.AUDIO_PLAYING, model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testOnAlertStateChanged_UseShrinkCloseAnimation() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        model.set(TabProperties.ALERT_STATE, TabAlert.NONE);
        model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);

        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);

        assertEquals(TabAlert.NONE, model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testOnAlertStateChanged_NotFound() {
        // Verify no exception is thrown when the tab ID is not found in the model list.
        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);
    }

    @Test
    public void testOnDidStartNavigationInPrimaryMainFrame() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        GURL tabUrl = JUnitTestGURLs.URL_1;
        GURL navUrl = JUnitTestGURLs.URL_2;
        when(mTab1.getUrl()).thenReturn(tabUrl);
        when(mTab1.isIncognito()).thenReturn(false);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.getUrl()).thenReturn(navUrl);
        when(mMediator.getDefaultFaviconFetcher(/* isIncognito= */ false))
                .thenReturn(mFaviconFetcher);

        mDelegate.onDidStartNavigationInPrimaryMainFrame(mTab1, mNavigationHandle);

        assertEquals(mFaviconFetcher, model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testOnDidStartNavigationInPrimaryMainFrame_SameDocument() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mNavigationHandle.isSameDocument()).thenReturn(true);

        mDelegate.onDidStartNavigationInPrimaryMainFrame(mTab1, mNavigationHandle);

        assertNull(model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testOnTitleUpdated() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mMediator.getLatestTitleForTabOrGroup(mTab1, model, /* useDefault= */ true))
                .thenReturn("New Title");

        mDelegate.onTitleUpdated(mTab1);

        assertEquals("New Title", model.get(TabProperties.TITLE));
    }

    @Test
    public void testOnTitleUpdated_TabNotFoundInTabModel() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(null);

        mDelegate.onTitleUpdated(mTab1);

        assertNull(model.get(TabProperties.TITLE));
    }

    @Test
    public void testOnLoadStarted() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        mDelegate.onLoadStarted(mTab1, /* toDifferentDocument= */ true);

        assertTrue(model.get(TabProperties.IS_LOADING));
    }

    @Test
    public void testOnLoadStarted_SameDocument_NoOp() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        mDelegate.onLoadStarted(mTab1, /* toDifferentDocument= */ false);

        assertFalse(model.get(TabProperties.IS_LOADING));
    }

    @Test
    public void testOnLoadStopped() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        model.set(TabProperties.IS_LOADING, true);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        mDelegate.onLoadStopped(mTab1, /* toDifferentDocument= */ true);

        assertFalse(model.get(TabProperties.IS_LOADING));
    }

    @Test
    public void testOnLoadStopped_SameDocument_NoOp() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        model.set(TabProperties.IS_LOADING, true);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        mDelegate.onLoadStopped(mTab1, /* toDifferentDocument= */ false);

        assertTrue(model.get(TabProperties.IS_LOADING));
    }

    @Test
    public void testOnCrash() {
        addTabsToModelList(TAB1_ID);
        PropertyModel model = mModelList.get(0).model;
        model.set(TabProperties.IS_LOADING, true);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        mDelegate.onCrash(mTab1);

        assertFalse(model.get(TabProperties.IS_LOADING));
    }

    @Test
    public void testOnTabPinnedStateChanged() {
        addTabsToModelList(TAB1_ID);

        mDelegate.onTabPinnedStateChanged(mTab1, /* isPinned= */ true);

        verify(mMediator).updateTab(0, mTab1, /* isUpdatingId= */ false, /* quickMode= */ false);
    }

    @Test
    public void testOnTabClose() {
        addTabsToModelList(TAB1_ID, TAB2_ID);

        mDelegate.onTabClose(mTab1);

        assertModelListTabIds(TAB2_ID);
    }

    @Test
    public void testOnTabClose_NotFound() {
        addTabsToModelList(TAB1_ID);

        mDelegate.onTabClose(mTab2);

        assertModelListTabIds(TAB1_ID);
    }

    @Test
    public void testSupportsTabGroups() {
        assertFalse(mDelegate.supportsTabGroups());
    }

    @Test
    public void testIsChildTabRepresentedByGroupCard() {
        assertFalse(mDelegate.isChildTabRepresentedByGroupCard(mTab1));
    }

    @Test
    public void testDidMoveTab_NoOp() {
        addTabsToModelList(TAB1_ID, TAB2_ID);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertModelListTabIds(TAB1_ID, TAB2_ID);
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testDidChangeTabGroupTitle_NoOp() {
        mDelegate.didChangeTabGroupTitle(TAB_GROUP_ID, "New Title");

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testDidChangeTabGroupColor_NoOp() {
        mDelegate.didChangeTabGroupColor(TAB_GROUP_ID, 1);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testDidChangeTabGroupCollapsed_NoOp() {
        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, true, false);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_ReturnsNull() {
        assertNull(mDelegate.getIndexAndTabForTabGroupId(TAB_GROUP_ID));
        assertNull(mDelegate.getIndexAndTabForTabGroupId(null));
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
    public void testDidMoveTabOutOfGroup() {
        addTabsToModelList(TAB1_ID, TAB2_ID);

        // Execute moving mTab1 out.
        mDelegate.didMoveTabOutOfGroup(mTab1, 0);

        assertModelListTabIds(TAB2_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup_LastTab() {
        addTabsToModelList(TAB1_ID);

        // Execute moving mTab1 (last tab) out.
        mDelegate.didMoveTabOutOfGroup(mTab1, 0);

        assertModelListTabIds();
    }

    @Test
    public void testDidMoveTabOutOfGroup_NotInModelList() {
        addTabsToModelList(TAB2_ID);

        mDelegate.didMoveTabOutOfGroup(mTab1, 0);

        // Verify no-op when tab is not in model list.
        assertModelListTabIds(TAB2_ID);
    }

    @Test
    public void testDidMergeTabToGroup() {
        // Setup mModelList with a tab that belongs to the same group as mTab2.
        addTabsToModelList(TAB1_ID);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);

        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab2));

        // Execute merging mTab2.
        mDelegate.didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        verify(mMediator).addObserversForTab(mTab2);
        verify(mMediator).addTabCardToModel(mTab2, 1);
    }

    @Test
    public void testDidMergeTabToGroup_DifferentGroup() {
        // Setup mModelList with a tab that belongs to a different group.
        addTabsToModelList(TAB1_ID);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID_2);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);

        // Execute merging mTab2.
        mDelegate.didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        verify(mMediator).getCurrentTabModelChecked();
        verifyNoMoreInteractions(mMediator);
    }

    @Test
    public void testDidMergeTabToGroup_EmptyModelList() {
        // Empty model list.
        mDelegate.didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        verify(mMediator).getCurrentTabModelChecked();
        verifyNoMoreInteractions(mMediator);
    }

    @Test
    public void testDidMoveTabGroup_NoOp() {
        mDelegate.didMoveTabGroup(mTab1, 0, 1);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testDidCreateNewGroup_NoOp() {
        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testDidRemoveTabGroup_NoOp() {
        mDelegate.didRemoveTabGroup(1, null, TabGroupObserver.DidRemoveTabGroupReason.MERGE);

        // Flat layout does not display tab group headers, so no updates should occur.
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testDidSelectTab() {
        addTabsToModelList(TAB1_ID, TAB2_ID);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        verify(mMediator).setLastSelectedTabListModelIndex(0);
        verify(mMediator).selectTab(0, 1);
    }

    @Test
    public void testDidSelectTab_TabDelayed() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        when(mMediator.isTabDelayed(mTab2)).thenReturn(true);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        verify(mMediator).setLastSelectedTabListModelIndex(0);
        verify(mMediator, never()).selectTab(anyInt(), anyInt());
    }

    @Test
    public void testGetUiIndexForTab() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        assertEquals(0, mDelegate.getUiIndexForTab(TAB1_ID));
        assertEquals(1, mDelegate.getUiIndexForTab(TAB2_ID));
        assertEquals(TabModel.INVALID_TAB_INDEX, mDelegate.getUiIndexForTab(3));
    }

    @Test
    public void testGetGroupCardTypeAndIsGroupCollapsed() {
        assertEquals(ModelType.TAB, mDelegate.getGroupCardType());
        assertTrue(mDelegate.isGroupCollapsed(TAB_GROUP_ID));
    }

    @Test
    public void testOnTabSelectionToggled_NoOp() {
        PropertyModel model = new PropertyModel(TabProperties.ALL_KEYS_TAB_GRID);
        mDelegate.onTabSelectionToggled(model, TAB1_ID, /* wasSelected= */ false);
        verifyNoInteractions(mMediator);
    }

    @Test
    public void testAreTabsInSameGroup_ReturnsFalse() {
        assertFalse(mDelegate.areTabsInSameGroup(TAB1_ID, mTab2));
    }

    @Test
    public void testPerformReorderAction() {
        addTabsToModelList(TAB1_ID, TAB2_ID);

        View view = new View(ApplicationProvider.getApplicationContext());
        when(mAccessibilityHelper.getPositionsOfReorderAction(view, R.id.move_tab_up))
                .thenReturn(new Pair<>(1, 0));
        when(mAccessibilityHelper.isReorderAction(R.id.move_tab_up)).thenReturn(true);
        mDelegate.setAccessibilityHelper(mAccessibilityHelper);

        var userActionTester = new UserActionTester();
        assertTrue(
                mDelegate.performAccessibilityAction(
                        view, R.id.move_tab_up, /* args= */ null, /* model= */ null));
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertTrue(
                userActionTester.getActions().contains("TabGrid.AccessibilityDelegate.Reordered"));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_CallsHelper() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        PropertyModel model = mModelList.get(0).model;

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        AccessibilityAction action = new AccessibilityAction(R.id.move_tab_down, "Move Down");
        when(mAccessibilityHelper.getPotentialActionsForView(view)).thenReturn(List.of(action));
        when(mAccessibilityHelper.getPositionsOfReorderAction(view, R.id.move_tab_down))
                .thenReturn(new Pair<>(0, 1));
        mDelegate.setAccessibilityHelper(mAccessibilityHelper);

        mDelegate.populateAccessibilityNodeInfo(view, info, model);

        assertTrue(info.getActionList().contains(action));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_PinnedTabCannotMoveToUnpinned() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        PropertyModel pinnedModel = mModelList.get(0).model;
        pinnedModel.set(TabProperties.IS_PINNED, true);
        PropertyModel unpinnedModel = mModelList.get(1).model;
        unpinnedModel.set(TabProperties.IS_PINNED, false);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        AccessibilityAction action = new AccessibilityAction(R.id.move_tab_down, "Move Down");
        when(mAccessibilityHelper.getPotentialActionsForView(view)).thenReturn(List.of(action));
        when(mAccessibilityHelper.getPositionsOfReorderAction(view, R.id.move_tab_down))
                .thenReturn(new Pair<>(0, 1));
        mDelegate.setAccessibilityHelper(mAccessibilityHelper);

        mDelegate.populateAccessibilityNodeInfo(view, info, pinnedModel);

        assertFalse(info.getActionList().contains(action));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_UnpinnedTabCannotMoveToPinned() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        PropertyModel pinnedModel = mModelList.get(0).model;
        pinnedModel.set(TabProperties.IS_PINNED, true);
        PropertyModel unpinnedModel = mModelList.get(1).model;
        unpinnedModel.set(TabProperties.IS_PINNED, false);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        AccessibilityAction action = new AccessibilityAction(R.id.move_tab_up, "Move Up");
        when(mAccessibilityHelper.getPotentialActionsForView(view)).thenReturn(List.of(action));
        when(mAccessibilityHelper.getPositionsOfReorderAction(view, R.id.move_tab_up))
                .thenReturn(new Pair<>(1, 0));
        mDelegate.setAccessibilityHelper(mAccessibilityHelper);

        mDelegate.populateAccessibilityNodeInfo(view, info, unpinnedModel);

        assertFalse(info.getActionList().contains(action));
    }

    @Test
    public void testPerformReorderAction_BlockedAcrossPinnedBoundary() {
        addTabsToModelList(TAB1_ID, TAB2_ID);
        mModelList.get(0).model.set(TabProperties.IS_PINNED, true);
        mModelList.get(1).model.set(TabProperties.IS_PINNED, false);

        View view = new View(ApplicationProvider.getApplicationContext());
        when(mAccessibilityHelper.getPositionsOfReorderAction(view, R.id.move_tab_down))
                .thenReturn(new Pair<>(0, 1));
        when(mAccessibilityHelper.isReorderAction(R.id.move_tab_down)).thenReturn(true);
        mDelegate.setAccessibilityHelper(mAccessibilityHelper);

        assertFalse(
                mDelegate.performAccessibilityAction(
                        view, R.id.move_tab_down, /* args= */ null, /* model= */ null));
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
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
