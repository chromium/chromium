// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link VerticalTabKeyboardHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabKeyboardHandlerUnitTest {
    private static final int TAB_ID_1 = 101;
    private static final int TAB_ID_2 = 102;
    private static final int PINNED_TAB_ID_1 = 201;
    private static final int PINNED_TAB_ID_2 = 202;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView mPinnedTabsRecyclerView;
    @Mock private View mFocusedView;
    @Mock private View mContainingItemView;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mPinnedTab1;
    @Mock private Tab mPinnedTab2;

    private TabListModel mModelList;
    private TabListModel mPinnedTabsModelList;
    private VerticalTabKeyboardHandler mHandler;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mPinnedTab1.getId()).thenReturn(PINNED_TAB_ID_1);
        when(mPinnedTab2.getId()).thenReturn(PINNED_TAB_ID_2);

        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModel.getTabById(PINNED_TAB_ID_1)).thenReturn(mPinnedTab1);
        when(mTabModel.getTabById(PINNED_TAB_ID_2)).thenReturn(mPinnedTab2);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mPinnedTab1)).thenReturn(0);
        when(mTabModel.indexOf(mPinnedTab2)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);

        mModelList = new TabListModel();
        mPinnedTabsModelList = new TabListModel();

        mHandler =
                new VerticalTabKeyboardHandler(
                        mTabModelSelector,
                        mModelList,
                        mPinnedTabsModelList,
                        mRecyclerView,
                        mPinnedTabsRecyclerView);
    }

    @Test
    @SmallTest
    public void testReorderKeyboardFocusedItem_UnpinnedTab_MoveDown() {
        setupFocusedTab(
                mRecyclerView,
                mModelList,
                TabProperties.UiType.TAB,
                TAB_ID_1,
                TAB_ID_2,
                /* focusedPosition= */ 0);

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel).moveTab(TAB_ID_1, 1);
    }

    @Test
    @SmallTest
    public void testReorderKeyboardFocusedItem_UnpinnedTab_MoveUp() {
        setupFocusedTab(
                mRecyclerView,
                mModelList,
                TabProperties.UiType.TAB,
                TAB_ID_1,
                TAB_ID_2,
                /* focusedPosition= */ 1);

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ true));
        verify(mTabModel).moveTab(TAB_ID_2, 0);
    }

    @Test
    @SmallTest
    public void testReorderKeyboardFocusedItem_Boundary_ReturnsFalse() {
        setupFocusedTab(
                mRecyclerView,
                mModelList,
                TabProperties.UiType.TAB,
                TAB_ID_1,
                TAB_ID_2,
                /* focusedPosition= */ 0);

        // First item moving up should be a no-op
        assertFalse(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ true));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());

        // Last item moving down should be a no-op
        when(mRecyclerView.getChildAdapterPosition(mContainingItemView)).thenReturn(1);
        assertFalse(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testReorderKeyboardFocusedItem_PinnedTab_MoveDown() {
        setupFocusedTab(
                mPinnedTabsRecyclerView,
                mPinnedTabsModelList,
                TabProperties.UiType.PINNED_TAB,
                PINNED_TAB_ID_1,
                PINNED_TAB_ID_2,
                /* focusedPosition= */ 0);

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel).moveTab(PINNED_TAB_ID_1, 1);
    }

    @Test
    @SmallTest
    public void testReorderKeyboardFocusedItem_PinnedTab_MoveUp() {
        setupFocusedTab(
                mPinnedTabsRecyclerView,
                mPinnedTabsModelList,
                TabProperties.UiType.PINNED_TAB,
                PINNED_TAB_ID_1,
                PINNED_TAB_ID_2,
                /* focusedPosition= */ 1);

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ true));
        verify(mTabModel).moveTab(PINNED_TAB_ID_2, 0);
    }

    @Test
    @SmallTest
    public void testReorderKeyboardFocusedItem_TabGroupHeader() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, new Token(1L, 2L))
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        PropertyModel tabModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB, tabModel2));

        when(mRecyclerView.hasFocus()).thenReturn(true);
        when(mRecyclerView.findFocus()).thenReturn(mFocusedView);
        when(mRecyclerView.findContainingItemView(mFocusedView)).thenReturn(mContainingItemView);
        when(mRecyclerView.getChildAdapterPosition(mContainingItemView)).thenReturn(0);

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB_ID_2)).thenReturn(List.of(mTab2));

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel).moveRelatedTabs(TAB_ID_1, 1);
    }

    @Test
    @SmallTest
    public void testReorderKeyboardFocusedItem_NoFocus_ReturnsFalse() {
        when(mRecyclerView.hasFocus()).thenReturn(false);
        when(mPinnedTabsRecyclerView.hasFocus()).thenReturn(false);

        assertFalse(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testOnKeyEvent_CtrlDpadDown_ReordersItem() {
        setupFocusedTab(
                mRecyclerView,
                mModelList,
                TabProperties.UiType.TAB,
                TAB_ID_1,
                TAB_ID_2,
                /* focusedPosition= */ 0);

        KeyEvent event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        0,
                        KeyEvent.META_CTRL_ON);
        assertTrue(mHandler.onKeyEvent(event));
        verify(mTabModel).moveTab(TAB_ID_1, 1);
    }

    @Test
    @SmallTest
    public void testOnKeyEvent_CtrlDpadUp_ReordersItem() {
        setupFocusedTab(
                mRecyclerView,
                mModelList,
                TabProperties.UiType.TAB,
                TAB_ID_1,
                TAB_ID_2,
                /* focusedPosition= */ 1);

        KeyEvent event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_DPAD_UP,
                        0,
                        KeyEvent.META_CTRL_ON);
        assertTrue(mHandler.onKeyEvent(event));
        verify(mTabModel).moveTab(TAB_ID_2, 0);
    }

    @Test
    @SmallTest
    public void testOnKeyEvent_ActionUp_ConsumesEventWhenFocused() {
        when(mRecyclerView.hasFocus()).thenReturn(true);
        KeyEvent event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_UP,
                        KeyEvent.KEYCODE_DPAD_UP,
                        0,
                        KeyEvent.META_CTRL_ON);
        assertTrue(mHandler.onKeyEvent(event));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testOnKeyEvent_ActionUp_IgnoredWhenNotFocusedOnList() {
        when(mRecyclerView.hasFocus()).thenReturn(false);
        when(mPinnedTabsRecyclerView.hasFocus()).thenReturn(false);
        KeyEvent event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_UP,
                        KeyEvent.KEYCODE_DPAD_UP,
                        0,
                        KeyEvent.META_CTRL_ON);
        assertFalse(mHandler.onKeyEvent(event));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testOnKeyEvent_NonCtrlKey_ReturnsFalse() {
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP, 0, 0);
        assertFalse(mHandler.onKeyEvent(event));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testOnKeyEvent_PageUpOrDown_ReturnsFalse() {
        KeyEvent pageUpEvent =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_PAGE_UP,
                        0,
                        KeyEvent.META_CTRL_ON);
        assertFalse(mHandler.onKeyEvent(pageUpEvent));

        KeyEvent pageDownEvent =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_PAGE_DOWN,
                        0,
                        KeyEvent.META_CTRL_ON);
        assertFalse(mHandler.onKeyEvent(pageDownEvent));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    private void setupFocusedTab(
            RecyclerView recyclerView,
            TabListModel modelList,
            int uiType,
            int firstTabId,
            int secondTabId,
            int focusedPosition) {
        PropertyModel model1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, firstTabId)
                        .build();
        modelList.add(new ListItem(uiType, model1));
        if (secondTabId != Tab.INVALID_TAB_ID) {
            PropertyModel model2 =
                    new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                            .with(TabProperties.TAB_ID, secondTabId)
                            .build();
            modelList.add(new ListItem(uiType, model2));
        }

        when(recyclerView.hasFocus()).thenReturn(true);
        when(recyclerView.findFocus()).thenReturn(mFocusedView);
        when(recyclerView.findContainingItemView(mFocusedView)).thenReturn(mContainingItemView);
        when(recyclerView.getChildAdapterPosition(mContainingItemView)).thenReturn(focusedPosition);
    }
}
