// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link VerticalTabKeyboardHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabKeyboardHandlerUnitTest {
    private static final int ITEM_COUNT = 4;
    private static final int ITEM_HEIGHT = 10;
    private static final int TAB_ID_1 = 101;
    private static final int TAB_ID_2 = 102;
    private static final int TAB_ID_3 = 103;
    private static final int PINNED_TAB_ID_1 = 201;
    private static final int PINNED_TAB_ID_2 = 202;
    private static final Token GROUP_ID = new Token(1L, 2L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private Tab mPinnedTab1;
    @Mock private Tab mPinnedTab2;
    @Mock private VerticalTabHoverController mHoverController;

    private RecyclerView mRecyclerView;
    private RecyclerView mPinnedTabsRecyclerView;

    private TabListModel mModelList;
    private TabListModel mPinnedTabsModelList;
    private VerticalTabKeyboardHandler mHandler;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mRecyclerView = createRecyclerView(activity);
        mPinnedTabsRecyclerView = createRecyclerView(activity);
        LinearLayout contentView = new LinearLayout(activity);
        contentView.setOrientation(LinearLayout.VERTICAL);
        contentView.addView(
                mPinnedTabsRecyclerView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ITEM_COUNT * ITEM_HEIGHT));
        contentView.addView(
                mRecyclerView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ITEM_COUNT * ITEM_HEIGHT));
        activity.setContentView(contentView);
        ShadowLooper.idleMainLooper();

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mPinnedTab1.getId()).thenReturn(PINNED_TAB_ID_1);
        when(mPinnedTab2.getId()).thenReturn(PINNED_TAB_ID_2);

        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB_ID_3)).thenReturn(mTab3);
        when(mTabModel.getTabById(PINNED_TAB_ID_1)).thenReturn(mPinnedTab1);
        when(mTabModel.getTabById(PINNED_TAB_ID_2)).thenReturn(mPinnedTab2);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);
        when(mTabModel.indexOf(mPinnedTab1)).thenReturn(0);
        when(mTabModel.indexOf(mPinnedTab2)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(mTab3);

        mModelList = new TabListModel();
        mPinnedTabsModelList = new TabListModel();

        mHandler =
                new VerticalTabKeyboardHandler(
                        mTabModelSelector,
                        mModelList,
                        mPinnedTabsModelList,
                        mRecyclerView,
                        mPinnedTabsRecyclerView,
                        mHoverController);
    }

    @Test
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
        focusItem(mRecyclerView, 1);
        assertFalse(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
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
    public void testReorderKeyboardFocusedItem_TabGroupHeader() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        PropertyModel tabModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB, tabModel2));

        focusItem(mRecyclerView, 0);

        when(mTabModel.getTabsInGroup(GROUP_ID)).thenReturn(List.of(mTab1));

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel).moveGroupToIndex(GROUP_ID, 1);
    }

    @Test
    public void testReorderKeyboardFocusedItem_ChildTab_UngroupUp() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));

        focusItem(mRecyclerView, 1);

        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ true));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab1), /* trailing= */ false, false);
    }

    @Test
    public void testReorderKeyboardFocusedItem_ChildTab_UngroupDown() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));

        PropertyModel standaloneModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_3)
                        .build();
        mModelList.add(new ListItem(TabProperties.UiType.TAB, standaloneModel));

        focusItem(mRecyclerView, 2);

        when(mTabModel.getRelatedTabList(TAB_ID_2)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab2), /* trailing= */ true, false);
    }

    @Test
    public void testReorderKeyboardFocusedItem_ChildTabAtEndOfList_UngroupsDown() {
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, GROUP_ID)
                        .build();
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID_2)
                        .with(TabProperties.TAB_GROUP_ID, GROUP_ID)
                        .build();

        mModelList.add(new ListItem(TabProperties.UiType.TAB_GROUP, headerModel));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel1));
        mModelList.add(new ListItem(TabProperties.UiType.TAB, childModel2));

        focusItem(mRecyclerView, 2);

        when(mTabModel.getRelatedTabList(TAB_ID_2)).thenReturn(List.of(mTab1, mTab2));

        assertTrue(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab2), /* trailing= */ true, false);
    }

    @Test
    public void testReorderKeyboardFocusedItem_NoFocus_ReturnsFalse() {
        assertFalse(mRecyclerView.hasFocus());
        assertFalse(mPinnedTabsRecyclerView.hasFocus());

        assertFalse(mHandler.reorderKeyboardFocusedItem(/* toPrevious= */ false));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
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
    public void testOnKeyEvent_ActionUp_ConsumesEventWhenFocused() {
        focusItem(mRecyclerView, 0);
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
    public void testOnKeyEvent_ActionUp_IgnoredWhenNotFocusedOnList() {
        assertFalse(mRecyclerView.hasFocus());
        assertFalse(mPinnedTabsRecyclerView.hasFocus());
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
    public void testOnKeyEvent_NonCtrlKey_ReturnsFalse() {
        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP, 0, 0);
        assertFalse(mHandler.onKeyEvent(event));
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
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

    @Test
    public void testOnKeyEvent_Escape_DismissesShowingHoverCard() {
        when(mHoverController.isHoverCardShowing()).thenReturn(true);

        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE, 0, 0);
        assertTrue(mHandler.onKeyEvent(event));
        verify(mHoverController).hideHoverCard();
    }

    @Test
    public void testOnKeyEvent_Escape_ActionUp_ReturnsTrueWhenHoverCardShowing() {
        when(mHoverController.isHoverCardShowing()).thenReturn(true);

        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ESCAPE, 0, 0);
        assertTrue(mHandler.onKeyEvent(event));
        verify(mHoverController, never()).hideHoverCard();
    }

    @Test
    public void testOnKeyEvent_Escape_HoverCardNotShowing_ReturnsFalse() {
        when(mHoverController.isHoverCardShowing()).thenReturn(false);

        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ESCAPE, 0, 0);
        assertFalse(mHandler.onKeyEvent(event));
        verify(mHoverController, never()).hideHoverCard();
    }

    @Test
    public void testOnKeyEvent_EscapeWithModifier_ReturnsFalse() {
        when(mHoverController.isHoverCardShowing()).thenReturn(true);

        KeyEvent event =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_ESCAPE,
                        0,
                        KeyEvent.META_CTRL_ON);
        assertFalse(mHandler.onKeyEvent(event));
        verify(mHoverController, never()).hideHoverCard();
    }

    private void setupFocusedTab(
            RecyclerView recyclerView,
            TabListModel modelList,
            @TabProperties.UiType int uiType,
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

        focusItem(recyclerView, focusedPosition);
    }

    private static RecyclerView createRecyclerView(Context context) {
        RecyclerView recyclerView = new RecyclerView(context);
        recyclerView.setLayoutManager(new LinearLayoutManager(context));
        recyclerView.setAdapter(new ItemAdapter());
        return recyclerView;
    }

    /** Gives focus to a view nested inside the item at {@code position}. */
    private static void focusItem(RecyclerView recyclerView, int position) {
        RecyclerView.ViewHolder holder = recyclerView.findViewHolderForAdapterPosition(position);
        assertNotNull(holder);
        View focusTarget = ((ViewGroup) holder.itemView).getChildAt(0);
        assertTrue(focusTarget.requestFocus());
    }

    /** Creates {@link #ITEM_COUNT} items, each containing a single focusable view. */
    private static class ItemAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            FrameLayout itemView = new FrameLayout(parent.getContext());
            itemView.setLayoutParams(
                    new RecyclerView.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ITEM_HEIGHT));
            View focusTarget = new View(parent.getContext());
            focusTarget.setFocusable(true);
            focusTarget.setFocusableInTouchMode(true);
            itemView.addView(focusTarget);
            return new RecyclerView.ViewHolder(itemView) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return ITEM_COUNT;
        }
    }
}
