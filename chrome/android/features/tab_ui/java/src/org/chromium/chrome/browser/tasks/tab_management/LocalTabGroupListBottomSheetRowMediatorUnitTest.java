// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabGroupListBottomSheetRowMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LocalTabGroupListBottomSheetRowMediatorUnitTest {
    private static final int TEST_COLOR = 0;
    private static final String TEST_TITLE = "testTitle";
    private static final int TEST_TAB_ID1 = 1;
    private static final int TEST_TAB_ID2 = 2;
    private static final int TEST_ROOT_ID = 123;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private FaviconResolver mFaviconResolver;
    @Mock private Runnable mOnClickRunnable;
    @Mock private TabMovedCallback mTabMovedCallback;
    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private Token mGroupId;
    private List<Tab> mTabs;
    private LocalTabGroupListBottomSheetRowMediator mMediator;

    @Before
    public void setUp() {
        mGroupId = Token.createRandom();
        mTabs = new ArrayList<>();
        mTabs.add(mTab1);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabGroupModelFilter.getTabsInGroup(mGroupId)).thenReturn(mTabs);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(mGroupId)).thenReturn(TEST_ROOT_ID);
        when(mTabGroupModelFilter.tabGroupExists(mGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getGroupLastShownTabId(mGroupId)).thenReturn(TEST_TAB_ID1);
        when(mTabGroupModelFilter.getTabCountForGroup(mGroupId)).thenReturn(1);
        when(mTabGroupModelFilter.getTabGroupColor(TEST_ROOT_ID)).thenReturn(TEST_COLOR);
        when(mTabGroupModelFilter.getTabGroupTitle(TEST_ROOT_ID)).thenReturn(TEST_TITLE);
        when(mTabModel.getTabById(TEST_TAB_ID1)).thenReturn(mTab1);
        when(mTabModel.getTabById(TEST_TAB_ID2)).thenReturn(mTab2);

        mMediator =
                new LocalTabGroupListBottomSheetRowMediator(
                        mGroupId,
                        mTabGroupModelFilter,
                        mFaviconResolver,
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
        assertNull(model.get(TabGroupRowProperties.TIMESTAMP_EVENT));
    }

    @Test
    public void testClickRow() {
        List<Tab> tabList = List.of(mTab2);
        mMediator =
                new LocalTabGroupListBottomSheetRowMediator(
                        mGroupId,
                        mTabGroupModelFilter,
                        mFaviconResolver,
                        mOnClickRunnable,
                        mTabMovedCallback,
                        tabList);
        PropertyModel model = mMediator.getModel();

        Runnable clickRunnable = model.get(TabGroupRowProperties.ROW_CLICK_RUNNABLE);
        clickRunnable.run();

        verify(mTabGroupModelFilter).mergeListOfTabsToGroup(tabList, mTab1, true);
        verify(mTabMovedCallback).onTabMoved();
        verify(mOnClickRunnable).run();
    }

    @Test
    public void testClickRow_tabsAlreadyInGroup() {
        PropertyModel model = mMediator.getModel();
        when(mTab1.getTabGroupId()).thenReturn(mGroupId);

        Runnable clickRunnable = model.get(TabGroupRowProperties.ROW_CLICK_RUNNABLE);
        clickRunnable.run();

        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(mTabs, mTab1, true);
        verify(mTabMovedCallback, never()).onTabMoved();
        verify(mOnClickRunnable).run();
    }

    @Test
    public void testClickRow_groupNoLongerExists() {
        PropertyModel model = mMediator.getModel();
        when(mTabGroupModelFilter.tabGroupExists(mGroupId)).thenReturn(false);
        when(mTabModel.getTabById(TEST_TAB_ID1)).thenReturn(mTab1);

        Runnable clickRunnable = model.get(TabGroupRowProperties.ROW_CLICK_RUNNABLE);
        clickRunnable.run();

        verify(mTabGroupModelFilter, never()).mergeListOfTabsToGroup(mTabs, mTab1, true);
        verify(mTabMovedCallback, never()).onTabMoved();
        verify(mOnClickRunnable).run();
    }
}
