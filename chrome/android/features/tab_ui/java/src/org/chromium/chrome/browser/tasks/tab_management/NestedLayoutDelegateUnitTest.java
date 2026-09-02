// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB_GROUP;

import android.util.Pair;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.FrameLayout;

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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabAlert;
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
    @Mock private View mView;

    private TabListModel mModelList;
    private NestedLayoutDelegate mDelegate;

    @Before
    public void setUp() {
        mModelList = new TabListModel();
        mDelegate = new NestedLayoutDelegate(mMediator, mModelList);
        when(mMediator.getCurrentTabModelChecked()).thenReturn(mTabModel);
        when(mMediator.isShowingTabs()).thenReturn(true);
        when(mMediator.supportsTabLoadingState()).thenReturn(true);
        when(mTabModel.getTabGroupColorWithFallback(any(Token.class)))
                .thenReturn(TabGroupColorId.BLUE);
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab1.isInitialized()).thenReturn(true);
        when(mTab2.getId()).thenReturn(TAB2_ID);
        when(mTab2.isInitialized()).thenReturn(true);
        when(mTab3.getId()).thenReturn(TAB3_ID);
        when(mTab3.isInitialized()).thenReturn(true);
    }

    @Test
    public void testRequiresThumbnailUpdateOnDeselect() {
        assertFalse(mDelegate.requiresThumbnailUpdateOnDeselect());
    }

    @Test
    public void testRequiresThumbnailUpdateOnSelect() {
        assertFalse(mDelegate.requiresThumbnailUpdateOnSelect());
    }

    @Test
    public void testRecordTabSelection_Vertical_StandardTab() {
        when(mMediator.getComponentId()).thenReturn(TabComponentId.VERTICAL_TABS);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTab1.getIsPinned()).thenReturn(false);

        var userActionTester = new UserActionTester();
        mDelegate.recordTabSelection(TAB1_ID);

        assertTrue(userActionTester.getActions().contains("MobileTabSwitched.VerticalTabs"));
        userActionTester.tearDown();
    }

    @Test
    public void testRecordTabSelection_Vertical_PinnedTab() {
        when(mMediator.getComponentId()).thenReturn(TabComponentId.VERTICAL_TABS);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTab1.getIsPinned()).thenReturn(true);

        var userActionTester = new UserActionTester();
        mDelegate.recordTabSelection(TAB1_ID);

        assertTrue(userActionTester.getActions().contains("MobileTabSwitched.VerticalTabsPinned"));
        userActionTester.tearDown();
    }

    @Test
    public void testGetAlertState_Tab() {
        PropertyModel tabModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);
        @TabAlert int state = mDelegate.getAlertState(mTab1, tabModel);
        assertEquals(TabAlert.AUDIO_PLAYING, state);
    }

    @Test
    public void testGetAlertState_GroupHeader() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, new Token(1, 2))
                        .build();
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);
        @TabAlert int state = mDelegate.getAlertState(mTab1, headerModel);
        assertEquals(TabAlert.NONE, state);
    }

    @Test
    public void testOnTabAdded_StandaloneTab() {
        setupTabsInModel(mTab1);

        int index = mDelegate.onTabAdded(mTab1);

        assertEquals(0, index);
        verify(mMediator).addTabInfoToModelForTab(mTab1, 0, /* isSelected= */ true);
    }

    @Test
    public void testOnTabAdded_TabInExpandedGroup() {
        setupTabsInModel(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(false);

        int index = mDelegate.onTabAdded(mTab1);

        assertEquals(1, index);
        verify(mMediator).addTabInfoToModelForGroup(mTab1, TAB_GROUP_ID, 0);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
        verify(mMediator).addTabInfoToModelForTab(mTab1, 1, /* isSelected= */ true);
    }

    @Test
    public void testOnTabAdded_TabInCollapsedGroup_NewHeader() {
        setupTabsInModel(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(true);

        int index = mDelegate.onTabAdded(mTab1);

        assertEquals(1, index);
        verify(mMediator).addTabInfoToModelForGroup(mTab1, TAB_GROUP_ID, 0);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
        verify(mMediator, never()).addTabInfoToModelForTab(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testOnTabAdded_TabInCollapsedGroup_HeaderAlreadyExists() {
        addGroupHeaderToModelList(TAB1_ID);
        setupTabsInModel(mTab1, mTab2);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mMediator.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(true);

        int index = mDelegate.onTabAdded(mTab2);

        assertEquals(TabModel.INVALID_TAB_INDEX, index);
        verify(mMediator, never()).addTabInfoToModelForGroup(any(), any(), anyInt());
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
        verify(mMediator, never()).addTabInfoToModelForTab(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testOnTabAdded_AlreadyInModel() {
        addTabToModelList(TAB1_ID, null);

        int index = mDelegate.onTabAdded(mTab1);

        assertEquals(0, index);
        verify(mMediator, never()).addTabInfoToModelForTab(any(), anyInt(), anyBoolean());
    }

    @Test
    public void testDidAddTab_NormalLaunch() {
        setupTabsInModel(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(null);

        mDelegate.didAddTab(mTab1, TabLaunchType.FROM_CHROME_UI);

        verify(mMediator).addTabInfoToModelForTab(eq(mTab1), eq(0), anyBoolean());
        verify(mMediator, never()).updateTab(anyInt(), any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testDidAddTab_FromRestore() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);

        mDelegate.didAddTab(mTab1, TabLaunchType.FROM_RESTORE);

        verify(mMediator).updateTab(0, mTab1, false, false);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testTabClosureUndone() {
        setupTabsInModel(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(null);

        mDelegate.tabClosureUndone(mTab1);

        verify(mMediator).addTabInfoToModelForTab(eq(mTab1), eq(0), anyBoolean());
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_NullGroupId() {
        assertNull(mDelegate.getIndexAndTabForTabGroupId(null));
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_HeaderNotFound() {
        assertNull(mDelegate.getIndexAndTabForTabGroupId(TAB_GROUP_ID));
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_EmptyTabsInGroup() {
        addGroupHeaderToModelList(TAB1_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of());

        assertNull(mDelegate.getIndexAndTabForTabGroupId(TAB_GROUP_ID));
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_Success() {
        addGroupHeaderToModelList(TAB1_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));

        Pair<Integer, Tab> result = mDelegate.getIndexAndTabForTabGroupId(TAB_GROUP_ID);
        assertNotNull(result);
        assertEquals(0, result.first.intValue());
        assertEquals(mTab1, result.second);
    }

    @Test
    public void testOnFaviconUpdated() {
        PropertyModel model = addTabToModelList(TAB1_ID, null);

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
        PropertyModel model = addTabToModelList(TAB1_ID, null);
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
        PropertyModel model = addTabToModelList(TAB1_ID, null);
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);

        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);

        assertEquals(TabAlert.AUDIO_PLAYING, model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testOnAlertStateChanged_UseShrinkCloseAnimation() {
        PropertyModel model = addTabToModelList(TAB1_ID, null);
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
    public void testOnTabClose_InGroup() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);

        mDelegate.onTabClose(mTab1);

        verify(mMediator).updateTabGroupHeaderId(TAB_GROUP_ID);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testOnTabClose_NotInGroup() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        addTabToModelList(TAB1_ID, null);

        mDelegate.onTabClose(mTab1);

        verify(mMediator, never()).updateTabGroupHeaderId(any());
        verify(mMediator, never()).updateTabGroupTitle(any());
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testOnTabClose_NotFound() {
        when(mTab1.getTabGroupId()).thenReturn(null);

        mDelegate.onTabClose(mTab1);

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testSupportsTabGroups() {
        assertTrue(mDelegate.supportsTabGroups());
    }

    @Test
    public void testIsChildTabRepresentedByGroupCard() {
        assertFalse(mDelegate.isChildTabRepresentedByGroupCard(mTab1));
    }

    @Test
    public void testPrepareTabCloseAnimation_LastTab() {
        addTabToModelList(TAB1_ID, null);
        addTabToModelList(TAB2_ID, null);

        FrameLayout parentView = new FrameLayout(ApplicationProvider.getApplicationContext());
        View closeButton = new View(ApplicationProvider.getApplicationContext());
        parentView.addView(closeButton);

        mDelegate.prepareTabCloseAnimation(closeButton, 1);

        assertEquals(true, parentView.getTag(R.id.tab_clip_from_top));
    }

    @Test
    public void testPrepareTabCloseAnimation_NotLastTab() {
        addTabToModelList(TAB1_ID, null);
        addTabToModelList(TAB2_ID, null);

        FrameLayout parentView = new FrameLayout(ApplicationProvider.getApplicationContext());
        View closeButton = new View(ApplicationProvider.getApplicationContext());
        parentView.addView(closeButton);

        mDelegate.prepareTabCloseAnimation(closeButton, 0);

        assertEquals(false, parentView.getTag(R.id.tab_clip_from_top));
    }

    @Test
    public void testPrepareTabCloseAnimation_NullView() {
        mDelegate.prepareTabCloseAnimation(null, 0);
        // Verify no crash on null view.
    }

    @Test
    public void testDidMoveTab_Standalone() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        addTabToModelList(TAB1_ID, null);
        addTabToModelList(TAB2_ID, null);
        setupTabsInModel(mTab2, mTab1);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidMoveTab_InGroup_NoOp() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidMoveTab_ModelHasGroupMetadata_NoOp() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, null);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidMoveTab_NotInModel_NoOp() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        addTabToModelList(TAB2_ID, null);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(1, mModelList.size());
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidChangeTabGroupTitle() {
        mDelegate.didChangeTabGroupTitle(TAB_GROUP_ID, "New Title");
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidChangeTabGroupColor() {
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID);
        PropertyModel child1Model = addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel child2Model = addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        mDelegate.didChangeTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        verify(mMediator)
                .updateTabGroupColorViewProvider(any(), eq(headerModel), eq(TabGroupColorId.BLUE));
        verify(mMediator)
                .updateTabGroupColorViewProvider(any(), eq(child1Model), eq(TabGroupColorId.BLUE));
        verify(mMediator)
                .updateTabGroupColorViewProvider(any(), eq(child2Model), eq(TabGroupColorId.BLUE));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_Collapse() {
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID);
        headerModel.set(TabProperties.IS_COLLAPSED, false);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        assertEquals(3, mModelList.size());

        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2));

        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, true, false);

        assertTrue(headerModel.get(TabProperties.IS_COLLAPSED));
        assertEquals(1, mModelList.size());
        assertEquals(TAB_GROUP_ID, mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID));
    }

    @Test
    public void testDidChangeTabGroupCollapsed_Idempotent() {
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID);
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
        PropertyModel headerModel = addGroupHeaderToModelList(TAB1_ID);
        headerModel.set(TabProperties.IS_COLLAPSED, true);

        assertEquals(1, mModelList.size());

        mDelegate.didChangeTabGroupCollapsed(TAB_GROUP_ID, false, false);

        assertFalse(headerModel.get(TabProperties.IS_COLLAPSED));
        verify(mMediator).insertChildTabs(TAB_GROUP_ID, 0);
    }

    @Test
    public void testDidMoveWithinGroup_Forward() {
        addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        when(mTabModel.getTabAt(1)).thenReturn(mTab1);

        mDelegate.didMoveWithinGroup(mTab2, 1, 2);

        assertModelListTabIds(TAB1_ID, TAB2_ID, TAB1_ID);
    }

    @Test
    public void testDidMoveWithinGroup_Backward() {
        addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);

        when(mTabModel.getTabAt(2)).thenReturn(mTab2);

        mDelegate.didMoveWithinGroup(mTab1, 2, 1);

        assertModelListTabIds(TAB1_ID, TAB2_ID, TAB1_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup() {
        setupTabsInModel(mTab1, mTab3);
        addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        PropertyModel tab1Model = addTabToModelList(TAB3_ID, TAB_GROUP_ID);

        when(mTabModel.getRepresentativeTabAt(1)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));

        mDelegate.didMoveTabOutOfGroup(mTab3, 1);

        verify(mMediator).updateTabGroupHeaderId(TAB_GROUP_ID);
        verify(mMediator).clearTabGroupProperties(tab1Model);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);

        assertModelListTabIds(TAB1_ID, TAB1_ID, TAB3_ID);
    }

    @Test
    public void testDidMoveTabOutOfGroup_CollapsedGroup() {
        setupTabsInModel(mTab1, mTab3);
        addGroupHeaderToModelList(TAB1_ID);

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
        addGroupHeaderToModelList(TAB3_ID);
        PropertyModel tab1Model = addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB3_ID, TAB_GROUP_ID);

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
        addGroupHeaderToModelList(TAB1_ID);
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
        verify(mMediator).addTabInfoToModelForGroup(mTab1, TAB_GROUP_ID, 0);
        verify(mMediator, times(2)).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidMergeTabToGroup_ToExistingGroup() {
        setupTabsInModel(mTab1, mTab2, mTab3);
        addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);
        addTabToModelList(TAB2_ID, TAB_GROUP_ID);
        PropertyModel tab1Model = addTabToModelList(TAB3_ID, null);

        when(mTab3.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, mTab2, mTab3));

        mDelegate.didMergeTabToGroup(mTab3, false);

        assertEquals(TAB_GROUP_ID, tab1Model.get(TabProperties.TAB_GROUP_ID));
        verify(mMediator).updateTabGroupProperties(mTab3, tab1Model, TabGroupColorId.BLUE);
        verify(mMediator).updateTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void testDidMergeTabToGroup_CollapsedGroup() {
        setupTabsInModel(mTab1, mTab2, mTab3);
        addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB3_ID, null);

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
        addGroupHeaderToModelList(TAB2_ID);
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
        addGroupHeaderToModelList(TAB1_ID);
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

        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        assertEquals(TAB_GROUP_ID, tab1Model.get(TabProperties.TAB_GROUP_ID));
        verify(mMediator).addTabInfoToModelForGroup(mTab1, TAB_GROUP_ID, 0);
        verify(mMediator).updateTabGroupProperties(mTab1, tab1Model, TabGroupColorId.BLUE);
        assertEquals(2, mModelList.size());
    }

    @Test
    public void testDidCreateNewGroup_AlreadyExists() {
        addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB1_ID, TAB_GROUP_ID);

        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        mDelegate.didCreateNewGroup(mTab1, mTabModel);

        verify(mMediator, never()).addTabInfoToModelForGroup(any(), any(), anyInt());
        verify(mMediator, never()).updateTabGroupProperties(any(), any(), anyInt());
        verify(mMediator, never()).clearTabGroupProperties(any());
    }

    @Test
    public void testDidRemoveTabGroup() {
        addGroupHeaderToModelList(TAB1_ID);
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

    @Test
    public void testEnsureGroupHeaderExists_NewHeader() {
        boolean created = mDelegate.ensureGroupHeaderExists(mTab1, TAB_GROUP_ID, 0);

        assertTrue(created);
        verify(mMediator).addTabInfoToModelForGroup(mTab1, TAB_GROUP_ID, 0);
    }

    @Test
    public void testEnsureGroupHeaderExists_AlreadyExists() {
        addGroupHeaderToModelList(TAB1_ID);

        boolean created = mDelegate.ensureGroupHeaderExists(mTab1, TAB_GROUP_ID, 0);

        assertFalse(created);
        verify(mMediator, never()).addTabInfoToModelForGroup(any(), any(), anyInt());
    }

    @Test
    public void testEnsureGroupHeaderExists_InvalidInputs() {
        assertFalse(mDelegate.ensureGroupHeaderExists(mTab1, null, 0));
        assertFalse(
                mDelegate.ensureGroupHeaderExists(mTab1, TAB_GROUP_ID, TabModel.INVALID_TAB_INDEX));
        verify(mMediator, never()).addTabInfoToModelForGroup(any(), any(), anyInt());
    }

    @Test
    public void testDidSelectTab() {
        addTabToModelList(TAB1_ID, null);
        addTabToModelList(TAB2_ID, null);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        verify(mMediator).setLastSelectedTabListModelIndex(0);
        verify(mMediator).selectTab(0, 1);
    }

    @Test
    public void testDidSelectTab_TabDelayed() {
        addTabToModelList(TAB1_ID, null);
        addTabToModelList(TAB2_ID, null);
        when(mMediator.isTabDelayed(mTab2)).thenReturn(true);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        verify(mMediator).setLastSelectedTabListModelIndex(0);
        verify(mMediator, never()).selectTab(anyInt(), anyInt());
    }

    @Test
    public void testGetUiIndexForTab() {
        addTabToModelList(TAB1_ID, null);
        addTabToModelList(TAB2_ID, null);
        assertEquals(0, mDelegate.getUiIndexForTab(TAB1_ID));
        assertEquals(1, mDelegate.getUiIndexForTab(TAB2_ID));
        assertEquals(TabModel.INVALID_TAB_INDEX, mDelegate.getUiIndexForTab(3));
    }

    @Test
    public void testGetGroupCardTypeAndIsGroupCollapsed() {
        assertEquals(TAB_GROUP, mDelegate.getGroupCardType());

        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(true);
        assertTrue(mDelegate.isGroupCollapsed(TAB_GROUP_ID));

        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(false);
        assertFalse(mDelegate.isGroupCollapsed(TAB_GROUP_ID));
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
    public void testPopulateAccessibilityNodeInfo_FirstTab_OnlyHasMoveDown() {
        PropertyModel firstModel = addTabToModelList(TAB1_ID, null);
        addTabToModelList(TAB2_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, firstModel);

        assertFalse(hasAction(info, R.id.move_tab_up));
        assertTrue(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_MiddleTab_HasMoveUpAndDown() {
        addTabToModelList(TAB1_ID, null);
        PropertyModel middleModel = addTabToModelList(TAB2_ID, null);
        addTabToModelList(TAB3_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, middleModel);

        assertTrue(hasAction(info, R.id.move_tab_up));
        assertTrue(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_LastTab_OnlyHasMoveUp() {
        addTabToModelList(TAB1_ID, null);
        PropertyModel lastModel = addTabToModelList(TAB2_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, lastModel);

        assertTrue(hasAction(info, R.id.move_tab_up));
        assertFalse(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_SingleTab_HasNoMoveActions() {
        PropertyModel singleModel = addTabToModelList(TAB1_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, singleModel);

        assertFalse(hasAction(info, R.id.move_tab_up));
        assertFalse(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_GroupHeader_HasExpandCollapseAndMove() {
        PropertyModel groupHeaderModel = addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB2_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, groupHeaderModel);

        assertTrue(hasAction(info, AccessibilityAction.ACTION_COLLAPSE.getId()));
        assertFalse(hasAction(info, R.id.move_tab_up));
        assertTrue(
                hasActionWithLabel(
                        info,
                        R.id.move_tab_down,
                        view.getContext().getString(R.string.move_tab_group_down)));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_TopmostUnpinnedTabWithPinnedAbove_CannotMoveUp() {
        addPinnedTabToModelList(TAB1_ID);
        PropertyModel firstUnpinnedModel = addTabToModelList(TAB2_ID, null);
        addTabToModelList(TAB3_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, firstUnpinnedModel);

        assertFalse(hasAction(info, R.id.move_tab_up));
        assertTrue(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_SinglePinnedTab_HasNoMoveActions() {
        PropertyModel pinnedModel = addPinnedTabToModelList(TAB1_ID);
        addTabToModelList(TAB2_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, pinnedModel);

        assertFalse(hasAction(info, R.id.move_tab_up));
        assertFalse(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_FirstPinnedTab_OnlyHasMoveDown() {
        PropertyModel firstPinnedModel = addPinnedTabToModelList(TAB1_ID);
        addPinnedTabToModelList(TAB2_ID);
        addTabToModelList(TAB3_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, firstPinnedModel);

        assertFalse(hasAction(info, R.id.move_tab_up));
        assertTrue(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_MiddlePinnedTab_HasMoveUpAndDown() {
        addPinnedTabToModelList(TAB1_ID);
        PropertyModel middlePinnedModel = addPinnedTabToModelList(TAB2_ID);
        addPinnedTabToModelList(TAB3_ID);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, middlePinnedModel);

        assertTrue(hasAction(info, R.id.move_tab_up));
        assertTrue(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void
            testPopulateAccessibilityNodeInfo_LastPinnedTabWithUnpinnedTabsBelow_OnlyHasMoveUp() {
        addPinnedTabToModelList(TAB1_ID);
        PropertyModel lastPinnedModel = addPinnedTabToModelList(TAB2_ID);
        addTabToModelList(TAB3_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, lastPinnedModel);

        assertTrue(hasAction(info, R.id.move_tab_up));
        assertFalse(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPerformAccessibilityAction_ReorderTab() {
        setupTabsInModel(mTab1, mTab2);
        addTabToModelList(TAB1_ID, null);
        PropertyModel tab2Model = addTabToModelList(TAB2_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        var userActionTester = new UserActionTester();
        assertTrue(
                mDelegate.performAccessibilityAction(
                        view, R.id.move_tab_up, /* args= */ null, tab2Model));
        verify(mTabModel).moveTab(TAB2_ID, 0);
        assertTrue(
                userActionTester.getActions().contains("TabGrid.AccessibilityDelegate.Reordered"));
    }

    @Test
    public void testPerformAccessibilityAction_ReorderPinnedTab() {
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);
        setupTabsInModel(mTab1, mTab2);
        addPinnedTabToModelList(TAB1_ID);
        PropertyModel pinned2Model = addPinnedTabToModelList(TAB2_ID);

        View view = new View(ApplicationProvider.getApplicationContext());
        var userActionTester = new UserActionTester();
        assertTrue(
                mDelegate.performAccessibilityAction(
                        view, R.id.move_tab_up, /* args= */ null, pinned2Model));
        verify(mTabModel).moveTab(TAB2_ID, 0);
        assertTrue(
                userActionTester.getActions().contains("TabGrid.AccessibilityDelegate.Reordered"));
    }

    @Test
    public void testPerformAccessibilityAction_ReorderTabGroup() {
        setupTabsInModel(mTab1, mTab2);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        PropertyModel groupHeaderModel = addGroupHeaderToModelList(TAB1_ID);
        addTabToModelList(TAB2_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        var userActionTester = new UserActionTester();
        assertTrue(
                mDelegate.performAccessibilityAction(
                        view, R.id.move_tab_down, /* args= */ null, groupHeaderModel));
        verify(mTabModel).moveRelatedTabs(TAB1_ID, 1);
        assertTrue(
                userActionTester.getActions().contains("TabGrid.AccessibilityDelegate.Reordered"));
    }

    @Test
    public void testPopulateAccessibilityNodeInfo_NonTabItem_HasNoMoveActions() {
        addTabToModelList(TAB1_ID, null);
        PropertyModel messageModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TabListModel.CardProperties.ModelType.MESSAGE)
                        .build();
        mModelList.add(new ListItem(UiType.ARCHIVED_TABS_MESSAGE, messageModel));
        addTabToModelList(TAB2_ID, null);

        View view = new View(ApplicationProvider.getApplicationContext());
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();

        mDelegate.populateAccessibilityNodeInfo(view, info, messageModel);

        assertFalse(hasAction(info, R.id.move_tab_up));
        assertFalse(hasAction(info, R.id.move_tab_down));
    }

    @Test
    public void testPerformAccessibilityAction_ExpandCollapse() {
        PropertyModel model = addGroupHeaderToModelList(TAB1_ID);

        assertTrue(
                mDelegate.performAccessibilityAction(
                        mView, AccessibilityAction.ACTION_EXPAND.getId(), /* args= */ null, model));
        verify(mView).performClick();

        assertTrue(
                mDelegate.performAccessibilityAction(
                        mView,
                        AccessibilityAction.ACTION_COLLAPSE.getId(),
                        /* args= */ null,
                        model));
        verify(mView, times(2)).performClick();
    }

    @Test
    public void testPerformAccessibilityAction_UnhandledAction_ReturnsFalse() {
        View view = new View(ApplicationProvider.getApplicationContext());
        PropertyModel model = addTabToModelList(TAB1_ID, null);

        assertFalse(
                mDelegate.performAccessibilityAction(
                        view, AccessibilityAction.ACTION_CLICK.getId(), /* args= */ null, model));
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

    private PropertyModel addGroupHeaderToModelList(int tabId) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TAB_GROUP)
                        .with(TabProperties.TAB_ID, tabId)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID)
                        .build();
        mModelList.add(new ListItem(UiType.TAB, model));
        return model;
    }

    private PropertyModel addPinnedTabToModelList(int tabId) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TAB)
                        .with(TabProperties.TAB_ID, tabId)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        mModelList.add(new ListItem(UiType.PINNED_TAB, model));
        return model;
    }

    private void setupTabsInModel(Tab... tabs) {
        when(mTabModel.getCount()).thenReturn(tabs.length);
        for (int i = 0; i < tabs.length; i++) {
            when(mTabModel.getTabAt(i)).thenReturn(tabs[i]);
            when(mTabModel.getTabById(tabs[i].getId())).thenReturn(tabs[i]);
            when(mTabModel.indexOf(tabs[i])).thenReturn(i);
            when(mTabModel.getRelatedTabList(tabs[i].getId())).thenReturn(List.of(tabs[i]));
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

    private static boolean hasAction(AccessibilityNodeInfo info, int actionId) {
        for (AccessibilityAction action : info.getActionList()) {
            if (action.getId() == actionId) return true;
        }
        return false;
    }

    private static boolean hasActionWithLabel(
            AccessibilityNodeInfo info, int actionId, String label) {
        for (AccessibilityAction action : info.getActionList()) {
            if (action.getId() == actionId && label.contentEquals(action.getLabel())) {
                return true;
            }
        }
        return false;
    }
}
