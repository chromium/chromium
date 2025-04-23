// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;

/** Unit tests for {@link TabSwitcherUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSwitcherUtilsUnitTest {
    private static final int TAB_ID_1 = 9;
    private static final Token TAB_GROUP_ID_1 = new Token(12, 34);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;
    @Mock private LayoutManager mLayoutManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private Callback<Integer> mRequestOpenTabGroupDialog;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mTab);
        when(mTab.getId()).thenReturn(TAB_ID_1);

        when(mLayoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
    }

    @Test
    public void testFocusTab() {
        TabSwitcherUtils.hideTabSwitcherAndShowTab(TAB_ID_1, mTabModelSelector, mLayoutManager);
        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(eq(0), eq(TabSelectionType.FROM_USER));
        verify(mLayoutManager).showLayout(eq(LayoutType.BROWSING), eq(false));
    }

    @Test
    public void testOpenTabGroupDialog_nullGroup() {
        TabSwitcherUtils.openTabGroupDialog(
                SYNC_GROUP_ID1,
                mTabGroupSyncService,
                mTabGroupUiActionHandler,
                mTabGroupModelFilter,
                mRequestOpenTabGroupDialog);

        verifyNoInteractions(mRequestOpenTabGroupDialog);
    }

    @Test
    public void testOpenTabGroupDialog_currentlyHidden() {
        SavedTabGroup syncGroup1 = new SavedTabGroup();
        syncGroup1.syncId = SYNC_GROUP_ID1;
        SavedTabGroup syncGroup2 = new SavedTabGroup();
        syncGroup2.localId = new LocalTabGroupId(TAB_GROUP_ID_1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(syncGroup1);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(TAB_GROUP_ID_1)).thenReturn(TAB_ID_1);
        doAnswer(
                        invocation -> {
                            Mockito.reset(mTabGroupSyncService);
                            when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1))
                                    .thenReturn(syncGroup2);
                            return null;
                        })
                .when(mTabGroupUiActionHandler)
                .openTabGroup(SYNC_GROUP_ID1);

        TabSwitcherUtils.openTabGroupDialog(
                SYNC_GROUP_ID1,
                mTabGroupSyncService,
                mTabGroupUiActionHandler,
                mTabGroupModelFilter,
                mRequestOpenTabGroupDialog);

        verify(mRequestOpenTabGroupDialog).onResult(TAB_ID_1);
    }

    @Test
    public void testOpenTabGroupDialog_invalidRoot() {
        SavedTabGroup syncGroup = new SavedTabGroup();
        syncGroup.localId = new LocalTabGroupId(TAB_GROUP_ID_1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(syncGroup);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(TAB_GROUP_ID_1))
                .thenReturn(INVALID_TAB_ID);

        TabSwitcherUtils.openTabGroupDialog(
                SYNC_GROUP_ID1,
                mTabGroupSyncService,
                mTabGroupUiActionHandler,
                mTabGroupModelFilter,
                mRequestOpenTabGroupDialog);

        verifyNoInteractions(mTabGroupUiActionHandler);
        verifyNoInteractions(mRequestOpenTabGroupDialog);
    }

    @Test
    public void testOpenTabGroupDialog_alreadyOpen() {
        SavedTabGroup syncGroup = new SavedTabGroup();
        syncGroup.localId = new LocalTabGroupId(TAB_GROUP_ID_1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(syncGroup);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(TAB_GROUP_ID_1)).thenReturn(TAB_ID_1);

        TabSwitcherUtils.openTabGroupDialog(
                SYNC_GROUP_ID1,
                mTabGroupSyncService,
                mTabGroupUiActionHandler,
                mTabGroupModelFilter,
                mRequestOpenTabGroupDialog);

        verifyNoInteractions(mTabGroupUiActionHandler);
        verify(mRequestOpenTabGroupDialog).onResult(TAB_ID_1);
    }
}
