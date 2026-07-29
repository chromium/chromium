// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroupOverlay;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabListItemTouchHelperCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final.
        })
public class VerticalTabListItemTouchHelperCallbackUnitTest {
    private static final int THROTTLE_TOKEN = 123;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Supplier<TabModel> mCurrentTabModelSupplier;
    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private RecyclerView mRecyclerView;
    @Mock private LinearLayoutManager mLinearLayoutManager;
    @Mock private GridLayoutManager mGridLayoutManager;
    @Mock private ViewGroupOverlay mViewGroupOverlay;
    @Mock private ItemTouchHelper2 mItemTouchHelper;

    @Mock
    private TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener mOnLongPressListener;

    @Mock private TabGridItemLongPressOrchestrator mOrchestrator;
    @Mock private VerticalTabListItemTouchHelperCallback.OnDragOutListener mOnDragOutListener;
    @Mock private UndoBarThrottle mUndoBarThrottle;
    @Mock private Canvas mCanvas;
    @Mock private View mItemView;
    @Mock private View mTargetItemView;
    @Mock private View mChildView;
    @Mock private View mChildView2;
    @Mock private View mHeaderView;
    @Mock private View mActionButton;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;

    private TabListModel mModel;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mTargetViewHolder;

    private VerticalTabListItemTouchHelperCallback mCallback;
    private PropertyModel mPropertyModel;
    private PropertyModel mTargetPropertyModel;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();

        when(mCurrentTabModelSupplier.get()).thenReturn(mTabModel);
        when(mTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mRecyclerView.getContext()).thenReturn(context);
        when(mRecyclerView.getOverlay()).thenReturn(mViewGroupOverlay);

        // Set up the mocked property model for the dragged view holder.
        mPropertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 1)
                        .build();

        mViewHolder = spy(new SimpleRecyclerViewAdapter.ViewHolder(mItemView, /* binder= */ null));
        mViewHolder.model = mPropertyModel;
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);

        // Set up the mocked property model for the target drop view holder.
        mTargetPropertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 2)
                        .build();
        mTargetViewHolder =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(mTargetItemView, /* binder= */ null));
        mTargetViewHolder.model = mTargetPropertyModel;
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);

        mModel = new TabListModel();
        mModel.add(new ListItem(TabProperties.UiType.TAB, mPropertyModel));
        mModel.add(new ListItem(TabProperties.UiType.TAB, mTargetPropertyModel));

        mCallback =
                new VerticalTabListItemTouchHelperCallback(
                        context, mModel, mCurrentTabModelSupplier, mUndoBarThrottle);
        mCallback.setRecyclerView(mRecyclerView);
    }

    @Test
    @SmallTest
    public void testGetMovementFlags_RegularTab() {
        // Regular tabs can only move UP or DOWN.
        mPropertyModel.set(TabProperties.IS_PINNED, false);

        int flags = mCallback.getMovementFlags(mRecyclerView, mViewHolder);
        int dragFlags =
                ItemTouchHelper.UP
                        | ItemTouchHelper.DOWN
                        | ItemTouchHelper.LEFT
                        | ItemTouchHelper.RIGHT;
        assertEquals(ItemTouchHelper2.Callback.makeMovementFlags(dragFlags, 0), flags);
    }

    @Test
    @SmallTest
    public void testGetMovementFlags_PinnedTab() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.PINNED_TAB);

        // Pinned tab placeholders in the main vertical list (LinearLayoutManager) return 0.
        when(mRecyclerView.getLayoutManager()).thenReturn(mLinearLayoutManager);
        assertEquals(0, mCallback.getMovementFlags(mRecyclerView, mViewHolder));

        // Pinned tab cards in the top strip (GridLayoutManager) return movement flags.
        when(mRecyclerView.getLayoutManager()).thenReturn(mGridLayoutManager);
        int flags = mCallback.getMovementFlags(mRecyclerView, mViewHolder);
        int dragFlags =
                ItemTouchHelper.UP
                        | ItemTouchHelper.DOWN
                        | ItemTouchHelper.LEFT
                        | ItemTouchHelper.RIGHT;
        assertEquals(ItemTouchHelper2.Callback.makeMovementFlags(dragFlags, 0), flags);
    }

    @Test
    @SmallTest
    public void testCanDropOver_SameType() {
        // Both tabs are regular: drop allowed.
        mPropertyModel.set(TabProperties.IS_PINNED, false);
        mTargetPropertyModel.set(TabProperties.IS_PINNED, false);

        assertTrue(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));

        // Both tabs are pinned: drop allowed.
        mPropertyModel.set(TabProperties.IS_PINNED, true);
        mTargetPropertyModel.set(TabProperties.IS_PINNED, true);

        assertTrue(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));
    }

    @Test
    @SmallTest
    public void testSetOnLongPressTabItemEventListener_WiresCallbackCorrectly() {
        mCallback.setOnLongPressTabItemEventListener(mOnLongPressListener);

        TabGridItemLongPressOrchestrator orchestrator =
                mCallback.getTabGridItemLongPressOrchestratorForTesting();

        assertNotNull(
                "Orchestrator should be initialized when listener is provided.", orchestrator);
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_DragStateTriggersOrchestrator() {
        // Set up the callback with a mock orchestrator so we can verify the execution.
        mCallback.setTabGridItemLongPressOrchestratorForTesting(mOrchestrator);

        // Create a real ViewHolder instance using an empty lambda for the ViewBinder.
        SimpleRecyclerViewAdapter.ViewHolder realViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildView, (model, view, key) -> {});

        // Inject a real PropertyModel to satisfy hasTabPropertiesModel().
        PropertyModel realPropertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabListModel.CardProperties.CARD_TYPE, TabProperties.UiType.TAB)
                        .build();
        realViewHolder.model = realPropertyModel;

        mCallback.onSelectedChanged(realViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Verify that the long-press pipeline correctly intercepts the dragging state change.
        verify(mOrchestrator)
                .onSelectedChanged(
                        realViewHolder.getBindingAdapterPosition(),
                        ItemTouchHelper.ACTION_STATE_DRAG);
    }

    @Test
    @SmallTest
    public void testCanDropOver_MixedType() {
        // Pinned dragging over regular: drop denied.
        mPropertyModel.set(TabProperties.IS_PINNED, true);
        mTargetPropertyModel.set(TabProperties.IS_PINNED, false);

        assertFalse(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));

        // Regular dragging over pinned: drop denied.
        mPropertyModel.set(TabProperties.IS_PINNED, false);
        mTargetPropertyModel.set(TabProperties.IS_PINNED, true);

        assertFalse(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));
    }

    @Test
    @SmallTest
    public void testOnMove_StandaloneTab() {
        // Verify onMove appropriately moves the tab in the TabModel based on bounds constraints.
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);

        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(2)).thenReturn(List.of(mTab2));

        when(mTabModel.indexOf(mTab2)).thenReturn(5);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel).moveTab(1, 5);
    }

    @Test
    @SmallTest
    public void testOnMove_StandaloneTabToGroupHeader_Downward_Groups() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        Token destGroupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, destGroupId); // It's a header.
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);

        // distance > 0 -> dragging downward.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        List.of(mTab1), mTab2, 0, TabGroupMergeNotificationType.NOTIFY_ALWAYS);
    }

    @Test
    @SmallTest
    public void testOnMove_StandaloneTabToLowestGroupTab_Upward_Groups() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        Token destGroupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, destGroupId);
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        when(mTab2.getId()).thenReturn(2);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);

        // Mock target as lowest tab.
        when(mTabModel.getRelatedTabList(2)).thenReturn(List.of(mTab3, mTab2));

        // distance < 0 -> dragging upward.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(2);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        List.of(mTab1), mTab2, null, TabGroupMergeNotificationType.NOTIFY_ALWAYS);
    }

    @Test
    @SmallTest
    public void testOnMove_ChildTab() {
        // Verify onMove appropriately moves the tab in the TabModel based on bounds constraints.
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);

        // Set a group ID to make it a child tab.
        when(mTab1.getTabGroupId()).thenReturn(groupId);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(2)).thenReturn(List.of(mTab2));

        when(mTabModel.indexOf(mTab2)).thenReturn(5);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel).moveTab(1, 5);
    }

    @Test
    @SmallTest
    public void testOnMove_ChildTab_InsideGroup() {
        // Verify onMove appropriately moves the tab in the TabModel when swapping with another
        // child tab in the same group that has more tabs.
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTab3.getIsPinned()).thenReturn(false);

        // All tabs are in the same group.
        when(mTab1.getTabGroupId()).thenReturn(groupId);
        when(mTab2.getTabGroupId()).thenReturn(groupId);
        when(mTab3.getTabGroupId()).thenReturn(groupId);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);

        // Both tabs share the same related tabs (they are in the same group).
        List<Tab> relatedTabs = List.of(mTab1, mTab2, mTab3);
        when(mTabModel.getRelatedTabList(1)).thenReturn(relatedTabs);
        when(mTabModel.getRelatedTabList(2)).thenReturn(relatedTabs);

        // Set up the indices: tab1 at 4, tab2 at 5, tab3 at 6.
        when(mTabModel.indexOf(mTab2)).thenReturn(5);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        // Distance > 0 (dragging downward).
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(4);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(5);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        // It should move to the index of tab2 (which is 5), NOT the end of the group (which would
        // be 6).
        verify(mTabModel).moveTab(1, 5);
    }

    @Test
    @SmallTest
    public void testOnMove_ChildTabToDifferentGroup_Ungroups() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId1 = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId1);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        Token groupId2 = new Token(3L, 4L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId2);
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(groupId1);

        // distance > 0 -> dragging downward.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabUngrouper).ungroupTabs(List.of(mTab1), true, false);
    }

    @Test
    @SmallTest
    public void testIsLongPressDragEnabled() {
        // Mouse input disables long press requirement for instant dragging.
        mCallback.setIsMouseInputSource(true);
        assertFalse(mCallback.isLongPressDragEnabled());

        // Touch input requires long press.
        mCallback.setIsMouseInputSource(false);
        assertTrue(mCallback.isLongPressDragEnabled());
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_Drag() {
        // Dragging highlights the selected card and activates it.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        assertEquals(
                TabListModel.AnimationStatus.SELECTED_CARD_ZOOM_IN,
                mPropertyModel.get(TabListModel.CardProperties.CARD_ANIMATION_STATUS));
        assertEquals(0.8f, mPropertyModel.get(TabListModel.CardProperties.CARD_ALPHA), 0.01f);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_StartsUndoBarThrottling() {
        when(mUndoBarThrottle.startThrottling()).thenReturn(THROTTLE_TOKEN);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        verify(mUndoBarThrottle).startThrottling();
    }

    @Test
    @SmallTest
    public void testOnInterceptTouchEvent_ActionUp_StopsUndoBarThrottling() {
        when(mUndoBarThrottle.startThrottling()).thenReturn(THROTTLE_TOKEN);

        // Start drag to acquire token.
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Simulate touch up to release token.
        RecyclerView.OnItemTouchListener listener =
                VerticalTabListItemTouchHelperCallback.createBeforeOnItemTouchListener(mCallback);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0f, 0f, 0);
        listener.onInterceptTouchEvent(mRecyclerView, event);

        verify(mUndoBarThrottle).stopThrottling(THROTTLE_TOKEN);
    }

    @Test
    @SmallTest
    public void testOnInterceptTouchEvent_ActionUpWhenNotThrottled_NeverCallsStopThrottling() {
        RecyclerView.OnItemTouchListener listener =
                VerticalTabListItemTouchHelperCallback.createBeforeOnItemTouchListener(mCallback);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 0f, 0f, 0);
        listener.onInterceptTouchEvent(mRecyclerView, event);

        verify(mUndoBarThrottle, never()).stopThrottling(anyInt());
    }

    @Test
    @SmallTest
    public void testClearView_StopsUndoBarThrottling() {
        when(mUndoBarThrottle.startThrottling()).thenReturn(THROTTLE_TOKEN);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        mCallback.clearView(mRecyclerView, mViewHolder);

        verify(mUndoBarThrottle).stopThrottling(THROTTLE_TOKEN);
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_Idle_StopsUndoBarThrottling() {
        when(mUndoBarThrottle.startThrottling()).thenReturn(THROTTLE_TOKEN);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        mCallback.onSelectedChanged(null, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mUndoBarThrottle).stopThrottling(THROTTLE_TOKEN);
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_Idle() {
        // Setup initial drag state.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Transition to idle clears the highlight.
        mCallback.onSelectedChanged(null, ItemTouchHelper.ACTION_STATE_IDLE);

        assertEquals(
                TabListModel.AnimationStatus.SELECTED_CARD_ZOOM_OUT,
                mPropertyModel.get(TabListModel.CardProperties.CARD_ANIMATION_STATUS));
        assertEquals(1.0f, mPropertyModel.get(TabListModel.CardProperties.CARD_ALPHA), 0.01f);
    }

    @Test
    @SmallTest
    public void testOnMove_updatesSelectedTabIndex() {
        // Setup initial drag state.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Move to a new position.
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(2)).thenReturn(List.of(mTab2));
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder);

        // Transition to idle should clear the highlight at the NEW position.
        mCallback.onSelectedChanged(null, ItemTouchHelper.ACTION_STATE_IDLE);

        assertEquals(
                TabListModel.AnimationStatus.SELECTED_CARD_ZOOM_OUT,
                mTargetPropertyModel.get(TabListModel.CardProperties.CARD_ANIMATION_STATUS));
        assertEquals(1.0f, mTargetPropertyModel.get(TabListModel.CardProperties.CARD_ALPHA), 0.01f);
    }

    @Test
    @SmallTest
    public void testCreateMouseDragDetector_ActionDownSelectsTab() {
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(mItemTouchHelper);

        MotionEvent event = createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f);

        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(mChildView);
        when(mRecyclerView.getChildViewHolder(mChildView)).thenReturn(mViewHolder);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, event);

        assertFalse(intercepted);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);

        event.recycle();
    }

    @Test
    @SmallTest
    public void testCreateMouseDragDetector_ActionMoveTriggersDrag() {
        Context context = ApplicationProvider.getApplicationContext();
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(mItemTouchHelper);

        // 1. ACTION_DOWN.
        MotionEvent downEvent = createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f);
        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(mChildView);
        when(mRecyclerView.getChildViewHolder(mChildView)).thenReturn(mViewHolder);

        // Stub tab model to avoid NPE during selection in ACTION_DOWN.
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        listener.onInterceptTouchEvent(mRecyclerView, downEvent);

        // 2. ACTION_MOVE (exceeding slop).
        float moveY = 10f + (touchSlop / 4f) + 5f;
        MotionEvent moveEvent = createMouseEvent(MotionEvent.ACTION_MOVE, 10f, moveY);

        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, moveEvent);

        assertFalse(intercepted);
        verify(mItemTouchHelper).startDrag(mViewHolder);

        downEvent.recycle();
        moveEvent.recycle();
    }

    @Test
    @SmallTest
    public void testCreateMouseDragDetector_CloseButtonClickNoDragNoSelect() {
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(mItemTouchHelper);

        // Setup views.
        when(mChildView.findViewById(R.id.action_button)).thenReturn(mActionButton);
        when(mActionButton.getVisibility()).thenReturn(View.VISIBLE);

        // Stub dimensions and locations.
        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 100;
                            pos[1] = 100;
                            return null;
                        })
                .when(mActionButton)
                .getLocationInWindow(any(int[].class));

        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 0;
                            pos[1] = 0;
                            return null;
                        })
                .when(mRecyclerView)
                .getLocationInWindow(any(int[].class));

        when(mActionButton.getWidth()).thenReturn(50);
        when(mActionButton.getHeight()).thenReturn(50);

        // Click at (120, 120) relative to RecyclerView (inside the close button).
        MotionEvent downEvent = createMouseEvent(MotionEvent.ACTION_DOWN, 120f, 120f);

        when(mRecyclerView.findChildViewUnder(120f, 120f)).thenReturn(mChildView);
        when(mRecyclerView.getChildViewHolder(mChildView)).thenReturn(mViewHolder);

        // ACTION_DOWN.
        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, downEvent);
        assertFalse(intercepted);

        // Verify NO tab selection occurred.
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());

        // ACTION_MOVE (should not drag).
        Context context = ApplicationProvider.getApplicationContext();
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        MotionEvent moveEvent = createMouseEvent(MotionEvent.ACTION_MOVE, 120f, 120f + touchSlop);
        listener.onInterceptTouchEvent(mRecyclerView, moveEvent);

        verify(mItemTouchHelper, never()).startDrag(any());

        downEvent.recycle();
        moveEvent.recycle();
    }

    @Test
    @SmallTest
    public void testCreateMouseDragDetector_GroupHeaderNoSelectButDrags() {
        Context context = ApplicationProvider.getApplicationContext();
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(mItemTouchHelper);

        // Set up ViewHolder as TAB_GROUP header.
        PropertyModel groupHeaderModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        SimpleRecyclerViewAdapter.ViewHolder headerViewHolder =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(mHeaderView, /* binder= */ null));
        headerViewHolder.model = groupHeaderModel;
        when(headerViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);

        // ACTION_DOWN.
        MotionEvent downEvent = createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f);
        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(mHeaderView);
        when(mRecyclerView.getChildViewHolder(mHeaderView)).thenReturn(headerViewHolder);

        listener.onInterceptTouchEvent(mRecyclerView, downEvent);

        // Verify NO tab selection occurred.
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());

        // ACTION_MOVE (should still drag).
        float moveY = 10f + (touchSlop / 4f) + 5f;
        MotionEvent moveEvent = createMouseEvent(MotionEvent.ACTION_MOVE, 10f, moveY);

        listener.onInterceptTouchEvent(mRecyclerView, moveEvent);

        // Verify drag WAS triggered.
        verify(mItemTouchHelper).startDrag(headerViewHolder);

        downEvent.recycle();
        moveEvent.recycle();
    }

    @Test
    @SmallTest
    public void testCreateMouseDragDetector_RightClickIgnored() {
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(mItemTouchHelper);

        // Simulate a RIGHT click (BUTTON_SECONDARY).
        MotionEvent event =
                createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f, MotionEvent.BUTTON_SECONDARY);

        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(mChildView);
        when(mRecyclerView.getChildViewHolder(mChildView)).thenReturn(mViewHolder);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, event);

        assertFalse(intercepted);
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());

        event.recycle();
    }

    @Test
    @SmallTest
    public void testCanDropOver_GroupHeaderOnChild() {
        // Current is a group header.
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        // Target is a tab in the same group.
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        // Dragging a group over its own child is blocked.
        assertFalse(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));

        // Different group.
        Token differentGroupId = new Token(3L, 4L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, differentGroupId);
        // Dragging a group over a child of another group should still be atomic (return false).
        assertFalse(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));
    }

    @Test
    @SmallTest
    public void testOnMove_GroupHeader_Downward() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);

        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(5);

        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTab3.getIsPinned()).thenReturn(false);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);

        List<Tab> destinationGroup = List.of(mTab2, mTab3);
        when(mTabModel.getRelatedTabList(2)).thenReturn(destinationGroup);

        when(mTabModel.indexOf(mTab2)).thenReturn(5);
        when(mTabModel.indexOf(mTab3)).thenReturn(6);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        // For distance > 0, should use getLastTabModelIndexForList (which is 6).
        verify(mTabModel).moveRelatedTabs(1, 6);
    }

    @Test
    @SmallTest
    public void testOnMove_GroupHeader_Upward() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);

        when(mViewHolder.getBindingAdapterPosition()).thenReturn(5);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(0);

        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTab3.getIsPinned()).thenReturn(false);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);

        List<Tab> destinationGroup = List.of(mTab2, mTab3);
        when(mTabModel.getRelatedTabList(2)).thenReturn(destinationGroup);

        when(mTabModel.indexOf(mTab2)).thenReturn(0);
        when(mTabModel.indexOf(mTab3)).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        // For distance < 0, should use getFirstTabModelIndexForList (which is 0).
        verify(mTabModel).moveRelatedTabs(1, 0);
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_DragGroupHeader_HighlightsChildren() {
        setupDragGroupHeaderState();

        // Verify selectTabForGroup sets the index.
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_DRAG);

        // Verify child is highlighted.
        assertTrue(mTargetPropertyModel.get(TabProperties.IS_SELECTED));
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_DragGroupHeader_PreservesSelection() {
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1, mTab2));

        // Setup mModel indices.
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        // Current active tab index is 1, which corresponds to tab2.
        when(mTabModel.index()).thenReturn(1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Since tab2 is already selected and it belongs to the group being dragged,
        // we shouldn't change the selection index.
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_Idle_ClearsHighlight() {
        setupDragGroupHeaderState();

        // Transition to IDLE.
        mCallback.onSelectedChanged(null, ItemTouchHelper.ACTION_STATE_IDLE);

        // Verify highlight cleared.
        assertFalse(mTargetPropertyModel.get(TabProperties.IS_SELECTED));
    }

    @Test
    @SmallTest
    public void testOnChildDraw_DragsGroupChildren() {
        when(mRecyclerView.getPaddingTop()).thenReturn(0);
        when(mRecyclerView.getPaddingBottom()).thenReturn(0);
        when(mRecyclerView.getPaddingLeft()).thenReturn(0);
        when(mRecyclerView.getHeight()).thenReturn(1000);
        when(mItemView.getTop()).thenReturn(100);
        when(mItemView.getBottom()).thenReturn(200);
        when(mItemView.getLeft()).thenReturn(0);

        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        // Child view inside group.
        SimpleRecyclerViewAdapter.ViewHolder childVH1 =
                createChildViewHolder(mChildView, 2, groupId);
        View childView1 = childVH1.itemView;

        // Child view outside group.
        SimpleRecyclerViewAdapter.ViewHolder childVH2 =
                createChildViewHolder(mChildView2, 3, new Token(3L, 4L));
        View childView2 = childVH2.itemView;

        attachRecyclerViewChildren(childVH1, childVH2);

        when(mViewHolder.itemView.getElevation()).thenReturn(5f);

        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                10f,
                20f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        // Child 1 inside group should move.
        verify(childView1).setTranslationY(20f);
        verify(childView1).setTranslationZ(5f);

        // Child 2 outside group should NOT move.
        verify(childView2, never()).setTranslationY(anyFloat());
    }

    @Test
    @SmallTest
    public void testClearView_RestoresChildren() {
        when(mRecyclerView.getPaddingTop()).thenReturn(0);
        when(mRecyclerView.getPaddingBottom()).thenReturn(0);
        when(mRecyclerView.getPaddingLeft()).thenReturn(0);
        when(mRecyclerView.getHeight()).thenReturn(1000);
        when(mItemView.getTop()).thenReturn(100);
        when(mItemView.getBottom()).thenReturn(200);
        when(mItemView.getLeft()).thenReturn(0);

        Token groupId = new Token(1L, 2L);
        // Setup a child view to simulate a drag in progress.
        SimpleRecyclerViewAdapter.ViewHolder childVH1 =
                createChildViewHolder(mChildView, 2, groupId);
        View childView1 = childVH1.itemView;

        // Header view.
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        attachRecyclerViewChildren(childVH1);

        // Call onChildDraw to simulate an ongoing drag that populates internal view state.
        when(mViewHolder.itemView.getElevation()).thenReturn(5f);
        when(childView1.getElevation()).thenReturn(2f);
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                10f,
                20f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        Mockito.clearInvocations(childView1);

        // Call clearView.
        mCallback.clearView(mRecyclerView, mViewHolder);

        // Restores to 0.
        verify(childView1).setTranslationY(0f);
        verify(childView1).setTranslationZ(0f);
    }

    @Test
    @SmallTest
    public void testCanDropOver_StandaloneTabOnGroupChild_ReturnsTrue() {
        mPropertyModel.set(TabProperties.IS_PINNED, false);
        mTargetPropertyModel.set(TabProperties.IS_PINNED, false);

        // Make current a standalone tab.
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        // Make target a child tab.
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        assertTrue(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));
    }

    @Test
    @SmallTest
    public void testGetBoundingBox_DraggingGroup_ExpandsTargetGroup() {
        // Initialize mRecyclerViewSupplier in callback.
        mCallback.getMovementFlags(mRecyclerView, mViewHolder);

        // Setup currently dragged item (mViewHolder) as a group header.
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Setup target item as a group header.
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        Token targetGroupId = new Token(3L, 4L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, targetGroupId);

        // Target view bounds.
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(10);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(100);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(1000);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(200);

        // Add a child tab to the target group in the RecyclerView.
        SimpleRecyclerViewAdapter.ViewHolder childVH =
                createChildViewHolder(mChildView, TabModel.INVALID_TAB_INDEX, targetGroupId);
        View childView = childVH.itemView;
        attachRecyclerViewChildren(childVH);

        // Child view bounds (below target header).
        when(childView.getLeft()).thenReturn(20);
        when(childView.getTop()).thenReturn(200);
        when(childView.getRight()).thenReturn(990);
        when(childView.getBottom()).thenReturn(300);

        Rect bounds = new Rect();
        mCallback.getBoundingBox(mTargetViewHolder, bounds);

        // Should be expanded to include the child.
        assertEquals(new Rect(10, 100, 1000, 300), bounds);
    }

    @Test
    @SmallTest
    public void testGetBoundingBox_DraggingTab_DoesNotExpandTargetGroup() {
        // Initialize mRecyclerViewSupplier in callback.
        mCallback.getMovementFlags(mRecyclerView, mViewHolder);

        // Setup currently dragged item (mViewHolder) as a normal tab.
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Setup target item as a group header.
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        Token targetGroupId = new Token(3L, 4L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, targetGroupId);

        // Target view bounds.
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(10);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(100);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(1000);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(200);

        // Add a child tab to the target group in the RecyclerView.
        SimpleRecyclerViewAdapter.ViewHolder childVH =
                createChildViewHolder(mChildView, TabModel.INVALID_TAB_INDEX, targetGroupId);
        View childView = childVH.itemView;
        attachRecyclerViewChildren(childVH);

        // Child view bounds.
        when(childView.getLeft()).thenReturn(20);
        when(childView.getTop()).thenReturn(200);
        when(childView.getRight()).thenReturn(990);
        when(childView.getBottom()).thenReturn(300);

        Rect bounds = new Rect();
        mCallback.getBoundingBox(mTargetViewHolder, bounds);

        // Should NOT be expanded because we are dragging a tab, not a group.
        assertEquals(new Rect(10, 100, 1000, 200), bounds);
    }

    @Test
    @SmallTest
    public void testChooseDropTarget_VerticalDrag_SwapsAtCenter() {
        // Setup currently dragged item (mViewHolder) as a normal tab.
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // selected view layout.
        when(mViewHolder.itemView.getLeft()).thenReturn(0);
        when(mViewHolder.itemView.getTop()).thenReturn(0);
        when(mViewHolder.itemView.getRight()).thenReturn(100);
        when(mViewHolder.itemView.getBottom()).thenReturn(100);

        // Setup target item.
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(0);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(150);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(100);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(250); // Center is y=200.

        List<RecyclerView.ViewHolder> targets = List.of(mTargetViewHolder);

        // Scenario 1: Drag downward, leading edge (bottom) is at y=190.
        // It has NOT crossed the center of target (y=200).
        RecyclerView.ViewHolder winner1 = mCallback.chooseDropTarget(mViewHolder, targets, 0, 90);
        assertNull(winner1);

        // Scenario 2: Drag downward, leading edge (bottom) is at y=210.
        // It HAS crossed the center of target (y=200).
        RecyclerView.ViewHolder winner2 = mCallback.chooseDropTarget(mViewHolder, targets, 0, 110);
        assertEquals(mTargetViewHolder, winner2);

        // Scenario 3: Drag upward, target is above selected.
        when(mTargetViewHolder.itemView.getTop()).thenReturn(-200);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(-100); // Center is y=-150.

        // Drag upward, leading edge (top) is at y=-140.
        // It has NOT crossed the center of target (y=-150).
        RecyclerView.ViewHolder winner3 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -140);
        assertNull(winner3);

        // Drag upward, leading edge (top) is at y=-160.
        // It HAS crossed the center of target (y=-150).
        RecyclerView.ViewHolder winner4 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -160);
        assertEquals(mTargetViewHolder, winner4);
    }

    @Test
    @SmallTest
    public void testChooseDropTarget_VerticalDrag_StandaloneTabToGroupLowestTab_SwapsAt25Percent() {
        // Setup currently dragged item (mViewHolder) as a normal standalone tab.
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // selected view layout.
        when(mViewHolder.itemView.getLeft()).thenReturn(0);
        when(mViewHolder.itemView.getTop()).thenReturn(0);
        when(mViewHolder.itemView.getRight()).thenReturn(100);
        when(mViewHolder.itemView.getBottom()).thenReturn(100);

        // Setup target item as a child tab of a group.
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(0);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(-200);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(100);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(-100); // Center is y=-150.

        // Mock TabModel to target the lowest tab.
        when(mTab2.getId()).thenReturn(2);
        when(mTabModel.getRelatedTabList(2))
                .thenReturn(List.of(mTab1, mTab2)); // last item is target.

        List<RecyclerView.ViewHolder> targets = List.of(mTargetViewHolder);

        // Standard 50% overlap is at y=-150.
        // But for grouping a standalone tab UPWARDS into the lowest tab of a group,
        // the threshold is 25% overlap (bottom quarter).
        // 25% overlap with bottom (-100) and height (100) -> threshold is -125.

        // Scenario 1: Drag upward, leading edge (top) is at -115.
        // Not crossed 25% threshold (-125).
        RecyclerView.ViewHolder winner1 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -115);
        assertNull(winner1);

        // Scenario 2: Drag upward, leading edge (top) is at -135.
        // Crossed 25% threshold (-125).
        RecyclerView.ViewHolder winner2 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -135);
        assertEquals(mTargetViewHolder, winner2);
    }

    @Test
    @SmallTest
    public void testHasDragEscapedBounds_GroupHeader() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, 0));
    }

    @Test
    @SmallTest
    public void testHasDragEscapedBounds_StandaloneTab() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, 0));
    }

    @Test
    @SmallTest
    public void testHasDragEscapedBounds_SolitaryChild() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        when(mTab1.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1));

        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, 0));
    }

    @Test
    @SmallTest
    public void testHasDragEscapedBounds_FirstChild_DragUp_ThresholdMet() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);
        when(mTab1.getTabGroupId()).thenReturn(groupId);
        when(mTab2.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1, mTab2));

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Threshold is top - height/2 = 200 - 50 = 150.
        // y < 150 -> return true.
        assertTrue(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 140, 0, -10));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab1), false, false);
    }

    @Test
    @SmallTest
    public void testHasDragEscapedBounds_FirstChild_DragUp_ThresholdNotMet() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);
        when(mTab1.getTabGroupId()).thenReturn(groupId);
        when(mTab2.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1, mTab2));

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Threshold is 150, y = 160 is not < 150.
        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 160, 0, -10));
    }

    @Test
    @SmallTest
    public void testHasDragEscapedBounds_LastChild_DragDown_ThresholdMet() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 2);

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);
        when(mTab1.getTabGroupId()).thenReturn(groupId);
        when(mTab2.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mTabModel.getRelatedTabList(2)).thenReturn(List.of(mTab1, mTab2)); // tab2 is last.

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Threshold is top + height/4 = 200 + 25 = 225.
        // y > 225 -> return true.
        assertTrue(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 230, 0, 10));
        verify(mTabUngrouper).ungroupTabs(List.of(mTab2), true, false);
    }

    @Test
    @SmallTest
    public void testHasDragEscapedBounds_MiddleChild_DragUpOrDown() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 2);

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);
        when(mTab3.getId()).thenReturn(3);
        when(mTab1.getTabGroupId()).thenReturn(groupId);
        when(mTab2.getTabGroupId()).thenReturn(groupId);
        when(mTab3.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mTabModel.getRelatedTabList(2))
                .thenReturn(List.of(mTab1, mTab2, mTab3)); // tab2 is middle.

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Middle child should never escape bounds.
        assertFalse(
                mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, -50)); // up.
        assertFalse(
                mCallback.hasDragEscapedBounds(
                        mRecyclerView, mViewHolder, 0, 1000, 0, 50)); // down.
    }

    @Test
    @SmallTest
    public void testOnChildDraw_TriggersOnDragOutListener() {
        mCallback.setOnDragOutListener(mOnDragOutListener);

        when(mRecyclerView.getWidth()).thenReturn(200);

        mCallback.setDragStartX(100f);

        // Dragged outside bounds (left).
        // cursorX = 100 + (-110) = -10 < 0.
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                -110f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mOnDragOutListener).onDragOut(mViewHolder, -110f, 0f);

        // Dragged outside bounds (right).
        // cursorX = 100 + 110 = 210 > 200.
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                110f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mOnDragOutListener).onDragOut(mViewHolder, 110f, 0f);

        // Dragged within bounds (no additional trigger).
        // cursorX = 100 + 50 = 150 (between 0 and 200).
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                50f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mOnDragOutListener, Mockito.times(2))
                .onDragOut(Mockito.any(), Mockito.anyFloat(), Mockito.anyFloat());
    }

    @Test
    @SmallTest
    public void testOnChildDraw_ClampsVerticalDisplacement() {
        when(mRecyclerView.getPaddingTop()).thenReturn(10);
        when(mRecyclerView.getPaddingBottom()).thenReturn(20);
        when(mRecyclerView.getPaddingLeft()).thenReturn(8);
        when(mRecyclerView.getHeight()).thenReturn(500);

        when(mItemView.getTop()).thenReturn(100);
        when(mItemView.getBottom()).thenReturn(180);
        when(mItemView.getLeft()).thenReturn(16);

        // topLimitDy = 10 - 100 = -90.
        // bottomLimitDy = 500 - 20 - 180 = 300.

        // 1. Drag too far up (dY = -150). Should clamp to topLimitDy (-90).
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                0f,
                -150f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(-90f, mPropertyModel.get(TabProperties.DRAGGING_Y), 0.01f);
        verify(mItemView).setTranslationY(-90f);

        Mockito.clearInvocations(mItemView);

        // 2. Drag too far down (dY = 400). Should clamp to bottomLimitDy (300).
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                0f,
                400f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(300f, mPropertyModel.get(TabProperties.DRAGGING_Y), 0.01f);
        verify(mItemView).setTranslationY(300f);

        Mockito.clearInvocations(mItemView);

        // 3. Drag within bounds (dY = 50). Should not clamp.
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                0f,
                50f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(50f, mPropertyModel.get(TabProperties.DRAGGING_Y), 0.01f);
        verify(mItemView).setTranslationY(50f);
    }

    @Test
    @SmallTest
    public void testOnChildDraw_ClampsHorizontalDisplacement_PinnedTab() {
        // Set the tab as pinned.
        mPropertyModel.set(TabProperties.IS_PINNED, true);

        when(mRecyclerView.getPaddingLeft()).thenReturn(8);
        when(mRecyclerView.getPaddingRight()).thenReturn(12);
        when(mRecyclerView.getWidth()).thenReturn(200);

        when(mItemView.getLeft()).thenReturn(16);
        when(mItemView.getRight()).thenReturn(150);
        // leftLimitDx = 8 - 16 = -8.
        // rightLimitDx = 200 - 12 - 150 = 38.

        // 1. Drag too far left (dX = -15). Should clamp to leftLimitDx (-8).
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                -15f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mItemView).setTranslationX(-8f);

        Mockito.clearInvocations(mItemView);

        // 2. Drag too far right (dX = 60). Should clamp to rightLimitDx (38).
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                60f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mItemView).setTranslationX(38f);

        Mockito.clearInvocations(mItemView);

        // 3. Drag within bounds (dX = 20). Should not clamp.
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                20f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mItemView).setTranslationX(20f);
    }

    @Test
    @SmallTest
    public void testOnChildDraw_LocksHorizontalDisplacement_RegularTab() {
        // Regular tab should always lock translationX to 0f, regardless of dX.
        mPropertyModel.set(TabProperties.IS_PINNED, false);

        when(mRecyclerView.getPaddingLeft()).thenReturn(8);
        when(mItemView.getLeft()).thenReturn(16);
        // leftLimitDx = 8 - 16 = -8.

        // 1. Drag left (dX = -15). Should be locked to 0f.
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                -15f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mItemView).setTranslationX(0f);

        Mockito.clearInvocations(mItemView);

        // 2. Drag right (dX = 30). Should also be locked to 0f.
        mCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mViewHolder,
                30f,
                0f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        verify(mItemView).setTranslationX(0f);
    }

    // ============================================================================================
    // Private Helpers.
    // ============================================================================================

    private SimpleRecyclerViewAdapter.ViewHolder createChildViewHolder(
            View view, int tabId, Token groupId) {
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(view, /* binder= */ null));
        PropertyModel model = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        if (tabId != TabModel.INVALID_TAB_INDEX) {
            model.set(TabProperties.TAB_ID, tabId);
        }
        if (groupId != null) {
            model.set(TabProperties.TAB_GROUP_ID, groupId);
        }
        viewHolder.model = model;
        return viewHolder;
    }

    private void attachRecyclerViewChildren(SimpleRecyclerViewAdapter.ViewHolder... viewHolders) {
        when(mRecyclerView.getChildCount()).thenReturn(viewHolders.length);
        for (int i = 0; i < viewHolders.length; i++) {
            SimpleRecyclerViewAdapter.ViewHolder vh = viewHolders[i];
            View view = vh.itemView;
            when(mRecyclerView.getChildAt(i)).thenReturn(view);
            when(mRecyclerView.getChildViewHolder(view)).thenReturn(vh);
            when(view.getParent()).thenReturn(mRecyclerView);
        }
    }

    private void setupDragGroupHeaderState() {
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1, mTab2));

        // Setup mModel indices.
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        // Current active tab index (mTab3 is active, outside this group).
        when(mTabModel.index()).thenReturn(1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab3);

        // mModel already has mTargetPropertyModel with TAB_ID=2 from setUp().
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
    }

    private MotionEvent createMouseEvent(int action, float x, float y) {
        return createMouseEvent(action, x, y, MotionEvent.BUTTON_PRIMARY);
    }

    private MotionEvent createMouseEvent(int action, float x, float y, int buttonState) {
        long time = 1000L;
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = x;
        coords[0].y = y;
        coords[0].pressure = 1.0f;
        coords[0].size = 1.0f;

        MotionEvent event =
                MotionEvent.obtain(
                        /* downTime= */ time,
                        /* eventTime= */ time,
                        /* action= */ action,
                        /* pointerCount= */ 1,
                        /* pointerProperties= */ properties,
                        /* pointerCoords= */ coords,
                        /* metaState= */ 0,
                        /* buttonState= */ buttonState,
                        /* xPrecision= */ 1.0f,
                        /* yPrecision= */ 1.0f,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        /* source= */ InputDevice.SOURCE_MOUSE,
                        /* flags= */ 0);
        return event;
    }

    @Test
    @SmallTest
    public void testDragDropResult_Aborted() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.DragDropResult",
                        VerticalTabListItemTouchHelperCallback.DragDropResult.ABORTED_NO_CHANGE);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
        mCallback.clearView(mRecyclerView, mViewHolder);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testDragDropResult_Reordered() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.DragDropResult",
                        VerticalTabListItemTouchHelperCallback.DragDropResult.REORDERED);

        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(2)).thenReturn(List.of(mTab2));
        when(mTabModel.indexOf(mTab2)).thenReturn(5);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
        mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder);
        mCallback.clearView(mRecyclerView, mViewHolder);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testDragDropResult_Grouped() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.DragDropResult",
                        VerticalTabListItemTouchHelperCallback.DragDropResult.GROUPED);

        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        Token destGroupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, destGroupId);
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
        mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder);
        mCallback.clearView(mRecyclerView, mViewHolder);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testDragDropResult_Ungrouped() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.DragDropResult",
                        VerticalTabListItemTouchHelperCallback.DragDropResult.UNGROUPED);

        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        when(mTab1.getId()).thenReturn(1);
        when(mTab2.getId()).thenReturn(2);
        when(mTab1.getTabGroupId()).thenReturn(groupId);
        when(mTab2.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(List.of(mTab1, mTab2));
        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
        assertTrue(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 140, 0, -10));
        mCallback.clearView(mRecyclerView, mViewHolder);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testDragDropResult_DraggedOut() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.DragDropResult",
                        VerticalTabListItemTouchHelperCallback.DragDropResult.DRAGGED_OUT);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(RecyclerView.NO_POSITION);
        mCallback.clearView(mRecyclerView, mViewHolder);

        histogramWatcher.assertExpected();
    }
}
