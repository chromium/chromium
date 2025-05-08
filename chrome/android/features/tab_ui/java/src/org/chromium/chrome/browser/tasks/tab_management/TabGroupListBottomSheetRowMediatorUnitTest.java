// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabMovedCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowView.TabGroupRowViewTitleData;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabGroupListBottomSheetRowMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupListBottomSheetRowMediatorUnitTest {
    private static final String TEST_SYNC_ID = "testSyncId";
    private static final int TEST_COLOR = 0;
    private static final String TEST_TITLE = "testTitle";
    private static final long TEST_UPDATE_TIME = 123456789L;
    private static final int TEST_LOCAL_ID = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabMovedCallback mTabMovedCallback;
    @Mock private FaviconResolver mFaviconResolver;
    @Mock private Runnable mOnClickRunnable;
    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Tab mTab;

    private final Token mToken = Token.createRandom();
    private List<Tab> mTabs;
    private SavedTabGroup mSavedTabGroup;
    private TabGroupListBottomSheetRowMediator mMediator;

    @Before
    public void setUp() {
        mTabs = new ArrayList<>();
        mTabs.add(mTab);

        SavedTabGroupTab savedTabGroupTab = new SavedTabGroupTab();
        List<SavedTabGroupTab> savedTabs = List.of(savedTabGroupTab);
        SyncedGroupTestHelper helper = new SyncedGroupTestHelper(mTabGroupSyncService);
        mSavedTabGroup = helper.newTabGroup(TEST_SYNC_ID);
        mSavedTabGroup.localId = new LocalTabGroupId(mToken);
        mSavedTabGroup.syncId = TEST_SYNC_ID;
        mSavedTabGroup.color = TEST_COLOR;
        mSavedTabGroup.title = TEST_TITLE;
        mSavedTabGroup.updateTimeMs = TEST_UPDATE_TIME;
        mSavedTabGroup.savedTabs = savedTabs;
        savedTabGroupTab.localId = TEST_LOCAL_ID;

        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabById(TEST_LOCAL_ID)).thenReturn(mTab);

        mMediator =
                new TabGroupListBottomSheetRowMediator(
                        mSavedTabGroup,
                        mTabGroupModelFilter,
                        mFaviconResolver,
                        mTabGroupSyncService,
                        mOnClickRunnable,
                        mTabMovedCallback,
                        mTabs);
    }

    @Test
    public void testConstruction() {
        PropertyModel model = mMediator.getModel();

        assertNotNull(model);
        assertNotNull(model.get(TabGroupRowProperties.CLUSTER_DATA));
        assertEquals(TEST_COLOR, model.get(TabGroupRowProperties.COLOR_INDEX));
        TabGroupRowViewTitleData titleData = model.get(TabGroupRowProperties.TITLE_DATA);
        assertEquals(TEST_TITLE, titleData.title);
        assertEquals(1, titleData.numTabs);
        assertEquals(
                R.plurals.tab_group_bottom_sheet_row_accessibility_text,
                titleData.rowAccessibilityTextResId);
        assertNotNull(model.get(TabGroupRowProperties.TIMESTAMP_EVENT));
    }

    @Test
    public void testClickRow() {
        PropertyModel model = mMediator.getModel();

        Runnable clickRunnable = model.get(TabGroupRowProperties.ROW_CLICK_RUNNABLE);
        clickRunnable.run();

        verify(mTabGroupModelFilter).mergeListOfTabsToGroup(mTabs, mTab, true);
        verify(mTabMovedCallback).onTabMoved();
        verify(mOnClickRunnable).run();
    }

    @Test
    public void testClickRow_tabsAlreadyInGroup() {
        PropertyModel model = mMediator.getModel();
        when(mTab.getTabGroupId()).thenReturn(mToken);

        Runnable clickRunnable = model.get(TabGroupRowProperties.ROW_CLICK_RUNNABLE);
        clickRunnable.run();

        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(mTabs, mTab, true);
        verify(mTabMovedCallback, never()).onTabMoved();
        verify(mOnClickRunnable).run();
    }

    @Test
    public void testClickRow_noLocalId() {
        PropertyModel model = mMediator.getModel();
        mSavedTabGroup.savedTabs.get(0).localId = null;
        Runnable clickRunnable = model.get(TabGroupRowProperties.ROW_CLICK_RUNNABLE);
        clickRunnable.run();

        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(mTabs, mTab, true);
        verify(mTabMovedCallback, never()).onTabMoved();
        verify(mOnClickRunnable).run();
    }

    @Test
    public void testClickRow_groupNoLongerExists() {
        PropertyModel model = mMediator.getModel();
        mSavedTabGroup.savedTabs = new ArrayList<>();
        Runnable clickRunnable = model.get(TabGroupRowProperties.ROW_CLICK_RUNNABLE);
        clickRunnable.run();

        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(mTabs, mTab, true);
        verify(mTabMovedCallback, never()).onTabMoved();
        verify(mOnClickRunnable).run();
    }
}
