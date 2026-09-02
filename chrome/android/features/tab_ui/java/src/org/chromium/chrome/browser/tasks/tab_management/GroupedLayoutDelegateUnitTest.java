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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.ARCHIVED_TAB_GROUP;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

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
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGridAccessibilityHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link GroupedLayoutDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupedLayoutDelegateUnitTest {
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListMediator mMediator;
    @Mock private ThumbnailProvider mThumbnailProvider;
    @Mock private TabGridAccessibilityHelper mAccessibilityHelper;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private TabModel mTabModel;

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 999;

    private TabListModel mModelList;
    private GroupedLayoutDelegate mDelegate;

    @Before
    public void setUp() {
        mModelList = new TabListModel();
        mDelegate = new GroupedLayoutDelegate(mMediator, mModelList, mThumbnailProvider);
        when(mMediator.getCurrentTabModelChecked()).thenReturn(mTabModel);
        when(mMediator.isShowingTabs()).thenReturn(true);
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab1.isInitialized()).thenReturn(true);
        when(mTab2.getId()).thenReturn(TAB2_ID);
        when(mTab2.isInitialized()).thenReturn(true);
        when(mTab3.getId()).thenReturn(TAB3_ID);
        when(mTab3.isInitialized()).thenReturn(true);
        when(mMediator.getIndexForTabIdWithRelatedTabs(anyInt()))
                .thenReturn(TabModel.INVALID_TAB_INDEX);
    }

    @Test
    public void testRequiresThumbnailUpdateOnDeselect() {
        assertTrue(mDelegate.requiresThumbnailUpdateOnDeselect());
    }

    @Test
    public void testRequiresThumbnailUpdateOnSelect() {
        assertTrue(mDelegate.requiresThumbnailUpdateOnSelect());
    }

    @Test
    public void testRecordTabSelection_NoOp() {
        when(mMediator.getComponentId()).thenReturn(TabComponentId.GRID_TAB_SWITCHER);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);

        var userActionTester = new UserActionTester();
        mDelegate.recordTabSelection(TAB1_ID);

        assertTrue(userActionTester.getActions().isEmpty());
        userActionTester.tearDown();
    }

    @Test
    public void testGetAlertState_NotInGroup() {
        when(mTab1.getId()).thenReturn(1);
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(false);

        PropertyModel model = new PropertyModel(TabProperties.ALL_KEYS_TAB_GRID);
        @TabAlert int state = mDelegate.getAlertState(mTab1, model);
        assertEquals(TabAlert.AUDIO_PLAYING, state);
    }

    @Test
    public void testGetAlertState_InGroup() {
        when(mTab1.getId()).thenReturn(1);
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);
        when(mTab2.getAlertState()).thenReturn(TabAlert.MEDIA_RECORDING);

        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mMediator.getRelatedTabsForId(1)).thenReturn(List.of(mTab1, mTab2));

        PropertyModel model = new PropertyModel(TabProperties.ALL_KEYS_TAB_GRID);
        @TabAlert int state = mDelegate.getAlertState(mTab1, model);
        assertEquals(TabAlert.MEDIA_RECORDING, state);
    }

    @Test
    public void testGetAlertState_InGroup_RepTabHasMaxPriority() {
        when(mTab1.getId()).thenReturn(1);
        when(mTab1.getAlertState()).thenReturn(TabAlert.DESKTOP_CAPTURING);
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);

        PropertyModel model = new PropertyModel(TabProperties.ALL_KEYS_TAB_GRID);
        @TabAlert int state = mDelegate.getAlertState(mTab1, model);
        assertEquals(TabAlert.DESKTOP_CAPTURING, state);

        // Fast exit should mean getRelatedTabsForId is never called.
        verify(mMediator, never()).getRelatedTabsForId(1);
    }

    @Test
    public void testGetAlertState_InGroup_RepTabHasHigherPriority() {
        when(mTab1.getId()).thenReturn(1);
        when(mTab1.getAlertState()).thenReturn(TabAlert.MEDIA_RECORDING);
        when(mTab2.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);

        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mMediator.getRelatedTabsForId(1)).thenReturn(List.of(mTab1, mTab2));

        PropertyModel model = new PropertyModel(TabProperties.ALL_KEYS_TAB_GRID);
        @TabAlert int state = mDelegate.getAlertState(mTab1, model);
        assertEquals(TabAlert.MEDIA_RECORDING, state);
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

    @Test
    public void testOnTabAdded_NewTab_Standalone() {
        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(1);
        setupRepresentativeTab(mTab1, mTab1, 0);

        int index = mDelegate.onTabAdded(mTab1);

        assertEquals(0, index);
        verify(mMediator).addTabCardToModel(mTab1, 0);
    }

    @Test
    public void testOnTabAdded_NewTabInGroup_RepresentativeTab() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(1);
        setupRepresentativeTab(mTab1, mTab1, 0);

        int index = mDelegate.onTabAdded(mTab1);

        assertEquals(0, index);
        verify(mMediator).addTabCardToModel(mTab1, 0);
    }

    @Test
    public void testOnTabAdded_NewTabInGroup_NonRepresentativeTab() {
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(1);
        setupRepresentativeTab(mTab2, mTab1, 0);

        int index = mDelegate.onTabAdded(mTab2);

        assertEquals(TabList.INVALID_TAB_INDEX, index);
        verify(mMediator, never()).addTabCardToModel(any(), anyInt());
    }

    @Test
    public void testOnTabAdded_AlreadyInModel() {
        createAndAddPropertyModel(TAB1_ID);

        int index = mDelegate.onTabAdded(mTab1);

        assertEquals(0, index);
        verify(mMediator, never()).addTabCardToModel(any(), anyInt());
    }

    @Test
    public void testDidAddTab_NormalLaunch() {
        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(1);
        setupRepresentativeTab(mTab1, mTab1, 0);

        mDelegate.didAddTab(mTab1, TabLaunchType.FROM_CHROME_UI);

        verify(mMediator).addTabCardToModel(mTab1, 0);
        verify(mMediator, never()).updateTab(anyInt(), any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testDidAddTab_FromRestore_UpdatesGroupCard() {
        createAndAddPropertyModel(TAB1_ID);
        when(mTabModel.representativeIndexOf(mTab2)).thenReturn(0);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab1);

        mDelegate.didAddTab(mTab2, TabLaunchType.FROM_RESTORE);

        verify(mMediator).updateTab(0, mTab1, false, false);
    }

    @Test
    public void testTabClosureUndone_StandaloneTab() {
        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(1);
        setupRepresentativeTab(mTab1, mTab1, 0);

        mDelegate.tabClosureUndone(mTab1);

        verify(mMediator).addTabCardToModel(mTab1, 0);
        verify(mMediator, never()).updateTab(anyInt(), any(), anyBoolean(), anyBoolean());
    }

    @Test
    public void testTabClosureUndone_InTabGroup_UpdatesGroupCard() {
        createAndAddPropertyModel(TAB1_ID);
        when(mMediator.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabModel.representativeIndexOf(mTab2)).thenReturn(0);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab1);

        mDelegate.tabClosureUndone(mTab2);

        verify(mMediator).updateTab(0, mTab1, false, false);
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_NullGroupId() {
        assertNull(mDelegate.getIndexAndTabForTabGroupId(null));
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_InvalidIndex() {
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB1_ID))
                .thenReturn(TabModel.INVALID_TAB_INDEX);

        assertNull(mDelegate.getIndexAndTabForTabGroupId(TAB_GROUP_ID));
    }

    @Test
    public void testGetIndexAndTabForTabGroupId_Success() {
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB1_ID)).thenReturn(0);
        when(mMediator.getTabForIndex(0)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);

        Pair<Integer, Tab> result = mDelegate.getIndexAndTabForTabGroupId(TAB_GROUP_ID);
        assertNotNull(result);
        assertEquals(0, result.first.intValue());
        assertEquals(mTab1, result.second);
    }

    @Test
    public void testOnFaviconUpdated_InTabGroup() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);
        setupGetIndexAndTabForTabGroupId(TAB_GROUP_ID, 0, mTab1);

        mDelegate.onFaviconUpdated(mTab1, null, null);

        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
    }

    @Test
    public void testOnFaviconUpdated_InTabGroup_NotFound() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        setupGetIndexAndTabForTabGroupIdNotFound(TAB_GROUP_ID);

        mDelegate.onFaviconUpdated(mTab1, null, null);

        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
        verify(mMediator, never()).updateFaviconForTab(any(), any(), any(), any());
    }

    @Test
    public void testOnFaviconUpdated_NotInTabGroup() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(false);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);

        mDelegate.onFaviconUpdated(mTab1, null, null);

        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
    }

    @Test
    public void testOnFaviconUpdated_NotInTabGroup_NotFound() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(false);

        mDelegate.onFaviconUpdated(mTab1, null, null);

        verify(mMediator, never()).updateFaviconForTab(any(), any(), any(), any());
    }

    @Test
    public void testOnUrlUpdated_InTabGroup() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);
        setupGetIndexAndTabForTabGroupId(TAB_GROUP_ID, 0, mTab1);
        when(mMediator.getDomainForTab(mTab1, model)).thenReturn("example.com");

        mDelegate.onUrlUpdated(mTab1);

        assertEquals("example.com", model.get(TabProperties.URL_DOMAIN));
        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
    }

    @Test
    public void testOnUrlUpdated_InTabGroup_NotFound() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        setupGetIndexAndTabForTabGroupIdNotFound(TAB_GROUP_ID);

        mDelegate.onUrlUpdated(mTab1);

        verify(mMediator, never()).getDomainForTab(any(), any());
        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
        verify(mMediator, never()).updateFaviconForTab(any(), any(), any(), any());
    }

    @Test
    public void testOnUrlUpdated_NotInTabGroup() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(false);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);
        when(mMediator.getDomainForTab(mTab1, model)).thenReturn("example.com");

        mDelegate.onUrlUpdated(mTab1);

        assertEquals("example.com", model.get(TabProperties.URL_DOMAIN));
        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
    }

    @Test
    public void testOnUrlUpdated_NotInTabGroup_NotFound() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(false);

        mDelegate.onUrlUpdated(mTab1);

        verify(mMediator, never()).getDomainForTab(any(), any());
        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
        verify(mMediator, never()).updateFaviconForTab(any(), any(), any(), any());
    }

    @Test
    public void testOnAlertStateChanged_InTabGroup() {
        when(mTab1.getId()).thenReturn(TAB1_ID);
        when(mTab2.getId()).thenReturn(TAB2_ID);
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);
        when(mTab2.getAlertState()).thenReturn(TabAlert.MEDIA_RECORDING);
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab2));
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);
        setupGetIndexAndTabForTabGroupId(TAB_GROUP_ID, 0, mTab1);

        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);

        assertEquals(TabAlert.MEDIA_RECORDING, model.get(TabProperties.ALERT_STATE));
        assertEquals(MediaState.RECORDING, model.get(TabProperties.MEDIA_INDICATOR));
        verify(mMediator).updateDescriptionString(model);
    }

    @Test
    public void testOnAlertStateChanged_InTabGroup_UseShrinkCloseAnimation() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);
        model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        setupGetIndexAndTabForTabGroupId(TAB_GROUP_ID, 0, mTab1);

        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);

        verify(mMediator, never()).updateDescriptionString(any());
    }

    @Test
    public void testOnAlertStateChanged_InTabGroup_NotFound() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        setupGetIndexAndTabForTabGroupIdNotFound(TAB_GROUP_ID);

        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);

        verify(mMediator, never()).updateDescriptionString(any());
    }

    @Test
    public void testOnAlertStateChanged_NotInTabGroup() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);

        mDelegate.onAlertStateChanged(mTab1, TabAlert.AUDIO_PLAYING);

        assertEquals(TabAlert.AUDIO_PLAYING, model.get(TabProperties.ALERT_STATE));
        assertEquals(MediaState.AUDIBLE, model.get(TabProperties.MEDIA_INDICATOR));
        verify(mMediator, never()).updateDescriptionString(any());
    }

    @Test
    public void testOnUiTabStateChanged_InTabGroup() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB1_ID)).thenReturn(0);
        PropertyModel groupModel = createAndAddPropertyModel(TAB1_ID);
        UiTabState state = new UiTabState(TAB1_ID, null, null, TabIndicatorStatus.DYNAMIC, false);

        mDelegate.onUiTabStateChanged(mTab1, state);
        verify(mMediator).updateThumbnailFetcher(groupModel, TAB1_ID);
    }

    @Test
    public void testOnUiTabStateChanged_NotInTabGroup() {
        when(mMediator.isTabInTabGroup(mTab1)).thenReturn(false);
        createAndAddPropertyModel(TAB1_ID);
        UiTabState state = new UiTabState(TAB1_ID, null, null, TabIndicatorStatus.DYNAMIC, false);

        mDelegate.onUiTabStateChanged(mTab1, state);
        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
    }

    @Test
    public void testOnTabClose_InGroup_NotClosing() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.representativeIndexOf(mTab1)).thenReturn(0);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab2);
        when(mTab2.isClosing()).thenReturn(false);
        createAndAddPropertyModel(TAB2_ID);

        mDelegate.onTabClose(mTab1);

        verify(mMediator).updateTab(0, mTab2, true, false);
        assertEquals(1, mModelList.size());
    }

    @Test
    public void testOnTabClose_InGroup_Closing() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.representativeIndexOf(mTab1)).thenReturn(0);
        when(mTabModel.getRepresentativeTabAt(0)).thenReturn(mTab2);
        when(mTab2.isClosing()).thenReturn(true);
        createAndAddPropertyModel(TAB1_ID);

        mDelegate.onTabClose(mTab1);

        verify(mMediator, never()).updateTab(anyInt(), any(), anyBoolean(), anyBoolean());
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testOnTabClose_NotInGroup() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        createAndAddPropertyModel(TAB1_ID);

        mDelegate.onTabClose(mTab1);

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testOnTabClose_NotFound() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        createAndAddPropertyModel(TAB2_ID);

        mDelegate.onTabClose(mTab1);

        assertEquals(1, mModelList.size());
    }

    @Test
    public void testSupportsTabGroups() {
        assertTrue(mDelegate.supportsTabGroups());
    }

    @Test
    public void testIsChildTabRepresentedByGroupCard() {
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);
        assertTrue(mDelegate.isChildTabRepresentedByGroupCard(mTab1));

        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(false);
        assertFalse(mDelegate.isChildTabRepresentedByGroupCard(mTab1));
    }

    @Test
    public void testDidMoveTab_Standalone() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        createAndAddPropertyModel(TAB1_ID);
        createAndAddPropertyModel(TAB2_ID);
        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(2);
        setupRepresentativeTab(mTab2, mTab2, 0);
        setupRepresentativeTab(mTab1, mTab1, 1);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidMoveTab_InGroup_NoOp() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        createAndAddPropertyModel(TAB1_ID);
        createAndAddPropertyModel(TAB2_ID);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidMoveTab_ModelHasGroupMetadata_NoOp() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        PropertyModel model1 = createAndAddPropertyModel(TAB1_ID);
        model1.set(TabProperties.TAB_GROUP_ID, TAB_GROUP_ID);
        createAndAddPropertyModel(TAB2_ID);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testDidMoveTab_NotInModel_NoOp() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        createAndAddPropertyModel(TAB2_ID);

        mDelegate.didMoveTab(mTab1, 1, 0);

        assertEquals(1, mModelList.size());
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
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
        setupGetIndexAndTabForTabGroupId(TAB_GROUP_ID, index, mTab1);

        mDelegate.didChangeTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);

        verify(mMediator).updateTabGroupProperties(mTab1, model, TabGroupColorId.BLUE);
        verify(mMediator).updateFaviconForTab(model, mTab1, null, null);
        verify(mMediator).updateDescriptionString(model);
        verify(mMediator).updateActionButtonDescriptionString(mTab1, model);
        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
    }

    @Test
    public void testDidChangeTabGroupColor_NotFound() {
        setupGetIndexAndTabForTabGroupIdNotFound(TAB_GROUP_ID);

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

        createAndAddPropertyModel(TAB1_ID);

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
        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1, mTab2));

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
        when(mMediator.getRelatedTabsForId(TAB2_ID)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB2_ID);
        when(mTabModel.getTabById(TAB2_ID)).thenReturn(mTab2);

        // Only TAB1_ID is in the model list
        createAndAddPropertyModel(TAB1_ID);

        mDelegate.didMergeTabToGroup(mTab2, false);

        assertEquals(1, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));

        verify(mMediator).updateTab(0, mTab2, true, false);
    }

    @Test
    public void testDidMoveTabGroup() {
        // Setup mModelList: [TAB2_ID, TAB1_ID].
        createAndAddPropertyModel(TAB2_ID);
        createAndAddPropertyModel(TAB1_ID);

        when(mMediator.getRelatedTabsForId(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));

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
        when(mTab3.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        when(mMediator.getRelatedTabsForId(TAB3_ID)).thenReturn(List.of(mTab3));
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(List.of(mTab3));
        when(mTabModel.getTabAt(2)).thenReturn(mTab3);

        setupRepresentativeTab(mTab3, mTab3, 1);
        when(mTabModel.getRepresentativeTabAt(2)).thenReturn(mTab3);
        when(mTabModel.representativeIndexOf(mTab3)).thenReturn(2);

        // mModelList is empty at this point, so the tab is non-existent.
        mDelegate.didMoveTabGroup(mTab3, 2, 1);

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
    public void testDidSelectTab_DirectModelMatch() {
        createAndAddPropertyModel(TAB1_ID);
        createAndAddPropertyModel(TAB2_ID);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        verify(mMediator).setLastSelectedTabListModelIndex(0);
        verify(mMediator).selectTab(0, 1);
    }

    @Test
    public void testDidSelectTab_RelatedTabsLookup() {
        createAndAddPropertyModel(TAB1_ID);
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB2_ID)).thenReturn(0);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB3_ID);

        verify(mMediator).setLastSelectedTabListModelIndex(TabModel.INVALID_TAB_INDEX);
        verify(mMediator).selectTab(TabModel.INVALID_TAB_INDEX, 0);
    }

    @Test
    public void testDidSelectTab_FromUndo_UpdatesGroupRepresentativeTab() {
        createAndAddPropertyModel(TAB1_ID);
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB2_ID)).thenReturn(0);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_UNDO, TAB3_ID);

        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        verify(mMediator).setLastSelectedTabListModelIndex(TabModel.INVALID_TAB_INDEX);
        verify(mMediator).selectTab(TabModel.INVALID_TAB_INDEX, 0);
    }

    @Test
    public void testDidSelectTab_TabDelayed_DoesNotSelect() {
        createAndAddPropertyModel(TAB1_ID);
        createAndAddPropertyModel(TAB2_ID);
        when(mMediator.isTabDelayed(mTab2)).thenReturn(true);

        mDelegate.didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        verify(mMediator).setLastSelectedTabListModelIndex(0);
        verify(mMediator, never()).selectTab(anyInt(), anyInt());
    }

    @Test
    public void testGetUiIndexForTab_DirectMatch() {
        createAndAddPropertyModel(TAB1_ID);
        assertEquals(0, mDelegate.getUiIndexForTab(TAB1_ID));
    }

    @Test
    public void testGetUiIndexForTab_Fallback() {
        createAndAddPropertyModel(TAB1_ID);
        when(mMediator.getIndexForTabIdWithRelatedTabs(TAB2_ID)).thenReturn(0);
        assertEquals(0, mDelegate.getUiIndexForTab(TAB2_ID));
    }

    @Test
    public void testGetGroupCardTypeAndIsGroupCollapsed() {
        assertEquals(TAB, mDelegate.getGroupCardType());
        assertTrue(mDelegate.isGroupCollapsed(TAB_GROUP_ID));
    }

    @Test
    public void testOnTabSelectionToggled_TabInGroup() {
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);

        mDelegate.onTabSelectionToggled(model, TAB1_ID, /* wasSelected= */ false);

        verify(mMediator).updateThumbnailFetcher(model, TAB1_ID);
    }

    @Test
    public void testOnTabSelectionToggled_TabNotInGroup() {
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(false);
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);

        mDelegate.onTabSelectionToggled(model, TAB1_ID, /* wasSelected= */ false);

        verify(mMediator, never()).updateThumbnailFetcher(any(), anyInt());
    }

    @Test
    public void testAreTabsInSameGroup() {
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        assertTrue(mDelegate.areTabsInSameGroup(TAB1_ID, mTab2));

        Token otherGroupId = new Token(3L, 4L);
        when(mTab2.getTabGroupId()).thenReturn(otherGroupId);
        assertFalse(mDelegate.areTabsInSameGroup(TAB1_ID, mTab2));

        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTab2.getTabGroupId()).thenReturn(null);
        assertFalse(mDelegate.areTabsInSameGroup(TAB1_ID, mTab2));

        when(mTabModel.getTabById(TAB1_ID)).thenReturn(null);
        assertFalse(mDelegate.areTabsInSameGroup(TAB1_ID, mTab2));
    }

    @Test
    public void testPerformReorderAction() {
        createAndAddPropertyModel(TAB1_ID);
        createAndAddPropertyModel(TAB2_ID);

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
        PropertyModel model = createAndAddPropertyModel(TAB1_ID);
        createAndAddPropertyModel(TAB2_ID);

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
        PropertyModel pinnedModel = createAndAddPropertyModel(TAB1_ID);
        pinnedModel.set(TabProperties.IS_PINNED, true);
        PropertyModel unpinnedModel = createAndAddPropertyModel(TAB2_ID);
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
        PropertyModel pinnedModel = createAndAddPropertyModel(TAB1_ID);
        pinnedModel.set(TabProperties.IS_PINNED, true);
        PropertyModel unpinnedModel = createAndAddPropertyModel(TAB2_ID);
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
        PropertyModel pinnedModel = createAndAddPropertyModel(TAB1_ID);
        pinnedModel.set(TabProperties.IS_PINNED, true);
        PropertyModel unpinnedModel = createAndAddPropertyModel(TAB2_ID);
        unpinnedModel.set(TabProperties.IS_PINNED, false);

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

    private void setupGetIndexAndTabForTabGroupId(Token tabGroupId, int index, Tab tab) {
        int tabId = tab.getId();
        when(mTabModel.getGroupLastShownTabId(tabGroupId)).thenReturn(tabId);
        when(mMediator.getIndexForTabIdWithRelatedTabs(tabId)).thenReturn(index);
        when(mMediator.getTabForIndex(index)).thenReturn(tab);
        when(tab.getTabGroupId()).thenReturn(tabGroupId);
        when(mTabModel.isTabInTabGroup(tab)).thenReturn(true);
    }

    private void setupGetIndexAndTabForTabGroupIdNotFound(Token tabGroupId) {
        when(mTabModel.getGroupLastShownTabId(tabGroupId)).thenReturn(Tab.INVALID_TAB_ID);
        when(mMediator.getIndexForTabIdWithRelatedTabs(Tab.INVALID_TAB_ID))
                .thenReturn(TabModel.INVALID_TAB_INDEX);
    }
}
