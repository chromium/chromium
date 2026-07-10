// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
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

import androidx.recyclerview.widget.ItemTouchHelper;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;

import java.util.Arrays;
import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabListItemTouchHelperCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final
        })
public class VerticalTabListItemTouchHelperCallbackUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Supplier<TabModel> mCurrentTabModelSupplier;
    @Mock private TabModel mTabModel;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private RecyclerView mRecyclerView;
    @Mock private ViewGroupOverlay mViewGroupOverlay;

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

        View itemView = mock(View.class);
        mViewHolder = spy(new SimpleRecyclerViewAdapter.ViewHolder(itemView, /* binder= */ null));
        mViewHolder.model = mPropertyModel;

        // Set up the mocked property model for the target drop view holder.
        mTargetPropertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 2)
                        .build();
        View targetItemView = mock(View.class);
        mTargetViewHolder =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(targetItemView, /* binder= */ null));
        mTargetViewHolder.model = mTargetPropertyModel;

        mModel = new TabListModel();
        mModel.add(new ListItem(TabProperties.UiType.TAB, mPropertyModel));
        mModel.add(new ListItem(TabProperties.UiType.TAB, mTargetPropertyModel));

        mCallback =
                new VerticalTabListItemTouchHelperCallback(
                        context, mModel, mCurrentTabModelSupplier);
        mCallback.setRecyclerView(mRecyclerView);
    }

    @Test
    public void testGetMovementFlags_RegularTab() {
        // Regular tabs can only move UP or DOWN.
        mPropertyModel.set(TabProperties.IS_PINNED, false);

        int flags = mCallback.getMovementFlags(mRecyclerView, mViewHolder);
        int dragFlags = ItemTouchHelper.UP | ItemTouchHelper.DOWN;
        assertEquals(ItemTouchHelper2.Callback.makeMovementFlags(dragFlags, 0), flags);
    }

    @Test
    public void testGetMovementFlags_PinnedTab() {
        // Pinned tabs can move UP, DOWN, LEFT, and RIGHT.
        mPropertyModel.set(TabProperties.IS_PINNED, true);

        int flags = mCallback.getMovementFlags(mRecyclerView, mViewHolder);
        int dragFlags =
                ItemTouchHelper.UP
                        | ItemTouchHelper.DOWN
                        | ItemTouchHelper.LEFT
                        | ItemTouchHelper.RIGHT;
        assertEquals(ItemTouchHelper2.Callback.makeMovementFlags(dragFlags, 0), flags);
    }

    @Test
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
        // Set up mock listener.
        TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener mockListener =
                mock(TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener.class);

        mCallback.setOnLongPressTabItemEventListener(mockListener);

        TabGridItemLongPressOrchestrator orchestrator =
                mCallback.getTabGridItemLongPressOrchestratorForTesting();

        assertNotNull(
                "Orchestrator should be initialized when listener is provided.", orchestrator);
    }

    @Test
    @SmallTest
    public void testOnSelectedChanged_DragStateTriggersOrchestrator() {
        // Set up the callback with a mock orchestrator so we can verify the execution.
        TabGridItemLongPressOrchestrator mockOrchestrator =
                mock(TabGridItemLongPressOrchestrator.class);
        mCallback.setTabGridItemLongPressOrchestratorForTesting(mockOrchestrator);

        // Create a real ViewHolder instance using an empty lambda for the ViewBinder.
        View placeholderView = mock(View.class);
        SimpleRecyclerViewAdapter.ViewHolder realViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(placeholderView, (model, view, key) -> {});

        // Inject a mock PropertyModel to prevent the NPE inside hasTabPropertiesModel().
        PropertyModel mockPropertyModel = mock(PropertyModel.class);
        realViewHolder.model = mockPropertyModel;
        doReturn(TabProperties.UiType.TAB)
                .when(mockPropertyModel)
                .get(TabListModel.CardProperties.CARD_TYPE);

        mCallback.onSelectedChanged(realViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Verify that the long-press pipeline correctly intercepts the dragging state change.
        verify(mockOrchestrator)
                .onSelectedChanged(
                        realViewHolder.getBindingAdapterPosition(),
                        ItemTouchHelper.ACTION_STATE_DRAG);
    }

    @Test
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
    public void testOnMove_StandaloneTab() {
        // Verify onMove appropriately moves the tab in the TabModel based on bounds constraints.
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab1.getIsPinned()).thenReturn(false);
        when(tab2.getIsPinned()).thenReturn(false);
        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(tab2).when(mTabModel).getTabById(2);
        doReturn(Arrays.asList(tab1)).when(mTabModel).getRelatedTabList(1);
        doReturn(Arrays.asList(tab2)).when(mTabModel).getRelatedTabList(2);

        when(mTabModel.indexOf(tab2)).thenReturn(5);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel).moveTab(1, 5);
    }

    @Test
    public void testOnMove_StandaloneTabToGroupHeader_Downward_Groups() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        Token destGroupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, destGroupId); // It's a header
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(tab2).when(mTabModel).getTabById(2);

        // distance > 0 -> dragging downward
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        Arrays.asList(tab1), tab2, 0, TabGroupMergeNotificationType.NOTIFY_ALWAYS);
    }

    @Test
    public void testOnMove_StandaloneTabToLowestGroupTab_Upward_Groups() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        Token destGroupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, destGroupId);
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(tab2).when(mTabModel).getTabById(2);

        // Mock target as lowest tab
        doReturn(Arrays.asList(mock(Tab.class), tab2)).when(mTabModel).getRelatedTabList(2);

        // distance < 0 -> dragging upward
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(2);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        Arrays.asList(tab1),
                        tab2,
                        null,
                        TabGroupMergeNotificationType.NOTIFY_ALWAYS);
    }

    @Test
    public void testOnMove_ChildTab() {
        // Verify onMove appropriately moves the tab in the TabModel based on bounds constraints.
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab1.getIsPinned()).thenReturn(false);
        when(tab2.getIsPinned()).thenReturn(false);

        // Set a group ID to make it a child tab
        when(tab1.getTabGroupId()).thenReturn(groupId);

        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(tab2).when(mTabModel).getTabById(2);
        doReturn(Arrays.asList(tab1)).when(mTabModel).getRelatedTabList(1);
        doReturn(Arrays.asList(tab2)).when(mTabModel).getRelatedTabList(2);

        when(mTabModel.indexOf(tab2)).thenReturn(5);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        verify(mTabModel).moveTab(1, 5);
    }

    @Test
    public void testOnMove_ChildTab_InsideGroup() {
        // Verify onMove appropriately moves the tab in the TabModel when swapping with another
        // child tab in the same group that has more tabs.
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        Tab tab3 = mock(Tab.class); // Third tab in the group
        when(tab1.getIsPinned()).thenReturn(false);
        when(tab2.getIsPinned()).thenReturn(false);
        when(tab3.getIsPinned()).thenReturn(false);

        // All tabs are in the same group
        when(tab1.getTabGroupId()).thenReturn(groupId);
        when(tab2.getTabGroupId()).thenReturn(groupId);
        when(tab3.getTabGroupId()).thenReturn(groupId);

        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(tab2).when(mTabModel).getTabById(2);

        // Both tabs share the same related tabs (they are in the same group)
        List<Tab> relatedTabs = Arrays.asList(tab1, tab2, tab3);
        doReturn(relatedTabs).when(mTabModel).getRelatedTabList(1);
        doReturn(relatedTabs).when(mTabModel).getRelatedTabList(2);

        // Set up the indices: tab1 at 4, tab2 at 5, tab3 at 6
        when(mTabModel.indexOf(tab2)).thenReturn(5);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        // Distance > 0 (dragging downward)
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(4);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(5);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        // It should move to the index of tab2 (which is 5), NOT the end of the group (which would
        // be 6)
        verify(mTabModel).moveTab(1, 5);
    }

    @Test
    public void testIsLongPressDragEnabled() {
        // Mouse input disables long press requirement for instant dragging.
        mCallback.setIsMouseInputSource(true);
        assertFalse(mCallback.isLongPressDragEnabled());

        // Touch input requires long press.
        mCallback.setIsMouseInputSource(false);
        assertTrue(mCallback.isLongPressDragEnabled());
    }

    @Test
    public void testOnSelectedChanged_Drag() {
        // Dragging highlights the selected card and activates it.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);

        Tab tab = mock(Tab.class);
        doReturn(tab).when(mTabModel).getTabById(1);
        when(mTabModel.indexOf(tab)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        assertEquals(
                TabListModel.AnimationStatus.SELECTED_CARD_ZOOM_IN,
                mPropertyModel.get(TabListModel.CardProperties.CARD_ANIMATION_STATUS));
        assertEquals(0.8f, mPropertyModel.get(TabListModel.CardProperties.CARD_ALPHA), 0.01f);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    public void testOnSelectedChanged_Idle() {
        // Setup initial drag state.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);

        Tab tab = mock(Tab.class);
        doReturn(tab).when(mTabModel).getTabById(1);
        when(mTabModel.indexOf(tab)).thenReturn(0);
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
    public void testOnMove_updatesSelectedTabIndex() {
        // Setup initial drag state.
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);

        Tab tab1 = mock(Tab.class);
        doReturn(tab1).when(mTabModel).getTabById(1);
        when(mTabModel.indexOf(tab1)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Move to a new position.
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(1);
        Tab tab2 = mock(Tab.class);
        when(tab2.getIsPinned()).thenReturn(false);
        doReturn(tab2).when(mTabModel).getTabById(2);
        doReturn(Arrays.asList(tab1)).when(mTabModel).getRelatedTabList(1);
        doReturn(Arrays.asList(tab2)).when(mTabModel).getRelatedTabList(2);
        when(mTabModel.indexOf(tab2)).thenReturn(1);
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
    public void testCreateMouseDragDetector_ActionDownSelectsTab() {
        ItemTouchHelper2 itemTouchHelper = mock(ItemTouchHelper2.class);
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(itemTouchHelper);

        MotionEvent event = createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f);

        View childView = mock(View.class);
        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(childView);
        when(mRecyclerView.getChildViewHolder(childView)).thenReturn(mViewHolder);

        Tab tab = mock(Tab.class);
        doReturn(tab).when(mTabModel).getTabById(1);
        when(mTabModel.indexOf(tab)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, event);

        assertFalse(intercepted);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);

        event.recycle();
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
    public void testCreateMouseDragDetector_ActionMoveTriggersDrag() {
        Context context = ApplicationProvider.getApplicationContext();
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        ItemTouchHelper2 itemTouchHelper = mock(ItemTouchHelper2.class);
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(itemTouchHelper);

        // 1. ACTION_DOWN
        MotionEvent downEvent = createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f);
        View childView = mock(View.class);
        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(childView);
        when(mRecyclerView.getChildViewHolder(childView)).thenReturn(mViewHolder);

        // Stub tab model to avoid NPE during selection in ACTION_DOWN
        Tab tab = mock(Tab.class);
        doReturn(tab).when(mTabModel).getTabById(1);
        when(mTabModel.indexOf(tab)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        listener.onInterceptTouchEvent(mRecyclerView, downEvent);

        // 2. ACTION_MOVE (exceeding slop)
        float moveY = 10f + (touchSlop / 4f) + 5f;
        MotionEvent moveEvent = createMouseEvent(MotionEvent.ACTION_MOVE, 10f, moveY);

        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, moveEvent);

        assertFalse(intercepted);
        verify(itemTouchHelper).startDrag(mViewHolder);

        downEvent.recycle();
        moveEvent.recycle();
    }

    @Test
    public void testCreateMouseDragDetector_CloseButtonClickNoDragNoSelect() {
        ItemTouchHelper2 itemTouchHelper = mock(ItemTouchHelper2.class);
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(itemTouchHelper);

        // Setup views
        View childView = mock(View.class);
        View actionButton = mock(View.class);
        when(childView.findViewById(R.id.action_button)).thenReturn(actionButton);
        when(actionButton.getVisibility()).thenReturn(View.VISIBLE);

        // Stub dimensions and locations
        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 100;
                            pos[1] = 100;
                            return null;
                        })
                .when(actionButton)
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

        when(actionButton.getWidth()).thenReturn(50);
        when(actionButton.getHeight()).thenReturn(50);

        // Click at (120, 120) relative to RecyclerView (inside the close button)
        MotionEvent downEvent = createMouseEvent(MotionEvent.ACTION_DOWN, 120f, 120f);

        when(mRecyclerView.findChildViewUnder(120f, 120f)).thenReturn(childView);
        when(mRecyclerView.getChildViewHolder(childView)).thenReturn(mViewHolder);

        // ACTION_DOWN
        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, downEvent);
        assertFalse(intercepted);

        // Verify NO tab selection occurred
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());

        // ACTION_MOVE (should not drag)
        Context context = ApplicationProvider.getApplicationContext();
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        MotionEvent moveEvent = createMouseEvent(MotionEvent.ACTION_MOVE, 120f, 120f + touchSlop);
        listener.onInterceptTouchEvent(mRecyclerView, moveEvent);

        verify(itemTouchHelper, never()).startDrag(any());

        downEvent.recycle();
        moveEvent.recycle();
    }

    @Test
    public void testCreateMouseDragDetector_GroupHeaderNoSelectButDrags() {
        Context context = ApplicationProvider.getApplicationContext();
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        ItemTouchHelper2 itemTouchHelper = mock(ItemTouchHelper2.class);
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(itemTouchHelper);

        // Set up ViewHolder as TAB_GROUP header
        PropertyModel groupHeaderModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        View headerView = mock(View.class);
        SimpleRecyclerViewAdapter.ViewHolder headerViewHolder =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(headerView, /* binder= */ null));
        headerViewHolder.model = groupHeaderModel;
        when(headerViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);

        // ACTION_DOWN
        MotionEvent downEvent = createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f);
        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(headerView);
        when(mRecyclerView.getChildViewHolder(headerView)).thenReturn(headerViewHolder);

        listener.onInterceptTouchEvent(mRecyclerView, downEvent);

        // Verify NO tab selection occurred
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());

        // ACTION_MOVE (should still drag)
        float moveY = 10f + (touchSlop / 4f) + 5f;
        MotionEvent moveEvent = createMouseEvent(MotionEvent.ACTION_MOVE, 10f, moveY);

        listener.onInterceptTouchEvent(mRecyclerView, moveEvent);

        // Verify drag WAS triggered
        verify(itemTouchHelper).startDrag(headerViewHolder);

        downEvent.recycle();
        moveEvent.recycle();
    }

    @Test
    public void testCreateMouseDragDetector_RightClickIgnored() {
        ItemTouchHelper2 itemTouchHelper = mock(ItemTouchHelper2.class);
        RecyclerView.OnItemTouchListener listener =
                mCallback.createMouseDragDetector(itemTouchHelper);

        // Simulate a RIGHT click (BUTTON_SECONDARY)
        MotionEvent event =
                createMouseEvent(MotionEvent.ACTION_DOWN, 10f, 10f, MotionEvent.BUTTON_SECONDARY);

        View childView = mock(View.class);
        when(mRecyclerView.findChildViewUnder(10f, 10f)).thenReturn(childView);
        when(mRecyclerView.getChildViewHolder(childView)).thenReturn(mViewHolder);

        Tab tab = mock(Tab.class);
        doReturn(tab).when(mTabModel).getTabById(1);
        when(mTabModel.indexOf(tab)).thenReturn(0);
        when(mTabModel.index()).thenReturn(1);

        boolean intercepted = listener.onInterceptTouchEvent(mRecyclerView, event);

        assertFalse(intercepted);
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());

        event.recycle();
    }

    @Test
    public void testCanDropOver_GroupHeaderOnChild() {
        // Current is a group header
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        // Target is a tab in the same group
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        // Dragging a group over its own child is blocked.
        assertFalse(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));

        // Different group
        Token differentGroupId = new Token(3L, 4L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, differentGroupId);
        // Dragging a group over a child of another group should still be atomic (return false)
        assertFalse(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));
    }

    @Test
    public void testOnMove_GroupHeader_Downward() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);

        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(5);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        Tab tab3 = mock(Tab.class);
        when(tab1.getIsPinned()).thenReturn(false);
        when(tab2.getIsPinned()).thenReturn(false);
        when(tab3.getIsPinned()).thenReturn(false);

        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(tab2).when(mTabModel).getTabById(2);

        List<Tab> destinationGroup = Arrays.asList(tab2, tab3);
        doReturn(destinationGroup).when(mTabModel).getRelatedTabList(2);

        when(mTabModel.indexOf(tab2)).thenReturn(5);
        when(mTabModel.indexOf(tab3)).thenReturn(6);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        // For distance > 0, should use getLastTabModelIndexForList (which is 6)
        verify(mTabModel).moveRelatedTabs(1, 6);
    }

    @Test
    public void testOnMove_GroupHeader_Upward() {
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);

        when(mViewHolder.getBindingAdapterPosition()).thenReturn(5);
        when(mTargetViewHolder.getBindingAdapterPosition()).thenReturn(0);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        Tab tab3 = mock(Tab.class);
        when(tab1.getIsPinned()).thenReturn(false);
        when(tab2.getIsPinned()).thenReturn(false);
        when(tab3.getIsPinned()).thenReturn(false);

        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(tab2).when(mTabModel).getTabById(2);

        List<Tab> destinationGroup = Arrays.asList(tab2, tab3);
        doReturn(destinationGroup).when(mTabModel).getRelatedTabList(2);

        when(mTabModel.indexOf(tab2)).thenReturn(0);
        when(mTabModel.indexOf(tab3)).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        assertTrue(mCallback.onMove(mRecyclerView, mViewHolder, mTargetViewHolder));

        // For distance < 0, should use getFirstTabModelIndexForList (which is 0)
        verify(mTabModel).moveRelatedTabs(1, 0);
    }

    @Test
    public void testOnSelectedChanged_DragGroupHeader_HighlightsChildren() {
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class); // Inactive child
        when(tab1.getId()).thenReturn(1);
        when(tab2.getId()).thenReturn(2);

        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(Arrays.asList(tab1, tab2)).when(mTabModel).getRelatedTabList(1);

        // Setup mModel indices
        when(mTabModel.indexOf(tab1)).thenReturn(0);
        when(mTabModel.indexOf(tab2)).thenReturn(1);
        // current active tab index
        when(mTabModel.index()).thenReturn(1);
        when(mTabModel.getTabAt(1)).thenReturn(mock(Tab.class));

        // mModel already has mTargetPropertyModel with TAB_ID=2 from setUp()

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Verify selectTabForGroup sets the index
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_DRAG);

        // Verify child is highlighted
        assertTrue(mTargetPropertyModel.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testOnSelectedChanged_DragGroupHeader_PreservesSelection() {
        when(mViewHolder.getBindingAdapterPosition()).thenReturn(0);
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab2.getId()).thenReturn(2);

        doReturn(tab1).when(mTabModel).getTabById(1);
        doReturn(Arrays.asList(tab1, tab2)).when(mTabModel).getRelatedTabList(1);

        // Setup mModel indices
        when(mTabModel.indexOf(tab1)).thenReturn(0);
        when(mTabModel.indexOf(tab2)).thenReturn(1);
        // Current active tab index is 1, which corresponds to tab2.
        when(mTabModel.index()).thenReturn(1);
        when(mTabModel.getTabAt(1)).thenReturn(tab2);

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Since tab2 is already selected and it belongs to the group being dragged,
        // we shouldn't change the selection index!
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testOnSelectedChanged_Idle_ClearsHighlight() {
        // Setup state to DRAG first
        testOnSelectedChanged_DragGroupHeader_HighlightsChildren();

        // Transition to IDLE
        mCallback.onSelectedChanged(null, ItemTouchHelper.ACTION_STATE_IDLE);

        // Verify highlight cleared
        assertFalse(mTargetPropertyModel.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testOnChildDraw_DragsGroupChildren() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        // Child view inside group
        View childView1 = mock(View.class);
        SimpleRecyclerViewAdapter.ViewHolder childVH1 =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(childView1, null));
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        childModel1.set(TabProperties.TAB_ID, 2);
        childModel1.set(TabProperties.TAB_GROUP_ID, groupId);
        childVH1.model = childModel1;

        // Child view outside group
        View childView2 = mock(View.class);
        SimpleRecyclerViewAdapter.ViewHolder childVH2 =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(childView2, null));
        PropertyModel childModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        childModel2.set(TabProperties.TAB_ID, 3);
        childModel2.set(TabProperties.TAB_GROUP_ID, new Token(3L, 4L));
        childVH2.model = childModel2;

        when(mRecyclerView.getChildCount()).thenReturn(2);
        when(mRecyclerView.getChildAt(0)).thenReturn(childView1);
        when(mRecyclerView.getChildViewHolder(childView1)).thenReturn(childVH1);
        when(childView1.getParent()).thenReturn(mRecyclerView);
        when(mRecyclerView.getChildAt(1)).thenReturn(childView2);
        when(mRecyclerView.getChildViewHolder(childView2)).thenReturn(childVH2);
        when(childView2.getParent()).thenReturn(mRecyclerView);

        Canvas canvas = mock(Canvas.class);
        when(mViewHolder.itemView.getElevation()).thenReturn(5f);

        mCallback.onChildDraw(
                canvas,
                mRecyclerView,
                mViewHolder,
                10f,
                20f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        // Child 1 inside group should move
        verify(childView1).setTranslationX(10f);
        verify(childView1).setTranslationY(20f);
        verify(childView1).setTranslationZ(5f);

        // Child 2 outside group should NOT move
        verify(childView2, never()).setTranslationX(anyInt());
    }

    @Test
    public void testClearView_RestoresChildren() {
        Token groupId = new Token(1L, 2L);

        // Setup a child view to simulate a drag in progress
        View childView1 = mock(View.class);
        SimpleRecyclerViewAdapter.ViewHolder childVH1 =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(childView1, null));
        PropertyModel childModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        childModel1.set(TabProperties.TAB_ID, 2);
        childModel1.set(TabProperties.TAB_GROUP_ID, groupId);
        childVH1.model = childModel1;

        // Header view
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mPropertyModel.set(TabProperties.TAB_ID, 1);
        mPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        when(mRecyclerView.getChildCount()).thenReturn(1);
        when(mRecyclerView.getChildAt(0)).thenReturn(childView1);
        when(mRecyclerView.getChildViewHolder(childView1)).thenReturn(childVH1);
        when(childView1.getParent()).thenReturn(mRecyclerView);

        // Call onChildDraw to simulate an ongoing drag that populates internal view state
        Canvas canvas = mock(Canvas.class);
        when(mViewHolder.itemView.getElevation()).thenReturn(5f);
        when(childView1.getElevation()).thenReturn(2f);
        mCallback.onChildDraw(
                canvas,
                mRecyclerView,
                mViewHolder,
                10f,
                20f,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        Mockito.clearInvocations(childView1);

        // Call clearView
        mCallback.clearView(mRecyclerView, mViewHolder);

        // Restores to 0
        verify(childView1).setTranslationX(0f);
        verify(childView1).setTranslationY(0f);
        verify(childView1).setTranslationZ(0f);
    }

    @Test
    public void testCanDropOver_StandaloneTabOnGroupChild_ReturnsTrue() {
        mPropertyModel.set(TabProperties.IS_PINNED, false);
        mTargetPropertyModel.set(TabProperties.IS_PINNED, false);

        // Make current a standalone tab
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);

        // Make target a child tab
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);

        assertTrue(mCallback.canDropOver(mRecyclerView, mViewHolder, mTargetViewHolder));
    }

    @Test
    public void testGetBoundingBox_DraggingGroup_ExpandsTargetGroup() {
        // Initialize mRecyclerViewSupplier in callback
        mCallback.getMovementFlags(mRecyclerView, mViewHolder);

        // Setup currently dragged item (mViewHolder) as a group header
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Setup target item as a group header
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        Token targetGroupId = new Token(3L, 4L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, targetGroupId);

        // Target view bounds
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(10);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(100);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(1000);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(200);

        // Add a child tab to the target group in the RecyclerView
        View childView = mock(View.class);
        SimpleRecyclerViewAdapter.ViewHolder childVH =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(childView, null));
        PropertyModel childModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        childModel.set(TabProperties.TAB_GROUP_ID, targetGroupId);
        childVH.model = childModel;

        when(mRecyclerView.getChildCount()).thenReturn(1);
        when(mRecyclerView.getChildAt(0)).thenReturn(childView);
        when(mRecyclerView.getChildViewHolder(childView)).thenReturn(childVH);

        // Child view bounds (below target header)
        when(childView.getLeft()).thenReturn(20);
        when(childView.getTop()).thenReturn(200);
        when(childView.getRight()).thenReturn(990);
        when(childView.getBottom()).thenReturn(300);

        Rect bounds = new Rect();
        mCallback.getBoundingBox(mTargetViewHolder, bounds);

        // Should be expanded to include the child
        assertEquals(new Rect(10, 100, 1000, 300), bounds);
    }

    @Test
    public void testGetBoundingBox_DraggingTab_DoesNotExpandTargetGroup() {
        // Initialize mRecyclerViewSupplier in callback
        mCallback.getMovementFlags(mRecyclerView, mViewHolder);

        // Setup currently dragged item (mViewHolder) as a normal tab
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // Setup target item as a group header
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        Token targetGroupId = new Token(3L, 4L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, targetGroupId);

        // Target view bounds
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(10);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(100);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(1000);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(200);

        // Add a child tab to the target group in the RecyclerView
        View childView = mock(View.class);
        SimpleRecyclerViewAdapter.ViewHolder childVH =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(childView, null));
        PropertyModel childModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        childModel.set(TabProperties.TAB_GROUP_ID, targetGroupId);
        childVH.model = childModel;

        when(mRecyclerView.getChildCount()).thenReturn(1);
        when(mRecyclerView.getChildAt(0)).thenReturn(childView);
        when(mRecyclerView.getChildViewHolder(childView)).thenReturn(childVH);

        // Child view bounds
        when(childView.getLeft()).thenReturn(20);
        when(childView.getTop()).thenReturn(200);
        when(childView.getRight()).thenReturn(990);
        when(childView.getBottom()).thenReturn(300);

        Rect bounds = new Rect();
        mCallback.getBoundingBox(mTargetViewHolder, bounds);

        // Should NOT be expanded because we are dragging a tab, not a group
        assertEquals(new Rect(10, 100, 1000, 200), bounds);
    }

    @Test
    public void testChooseDropTarget_VerticalDrag_SwapsAtCenter() {
        // Setup currently dragged item (mViewHolder) as a normal tab
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // selected view layout
        when(mViewHolder.itemView.getLeft()).thenReturn(0);
        when(mViewHolder.itemView.getTop()).thenReturn(0);
        when(mViewHolder.itemView.getRight()).thenReturn(100);
        when(mViewHolder.itemView.getBottom()).thenReturn(100);

        // Setup target item
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(0);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(150);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(100);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(250); // Center is y=200

        List<RecyclerView.ViewHolder> targets = Arrays.asList(mTargetViewHolder);

        // Scenario 1: Drag downward, leading edge (bottom) is at y=190.
        // It has NOT crossed the center of target (y=200).
        RecyclerView.ViewHolder winner1 = mCallback.chooseDropTarget(mViewHolder, targets, 0, 90);
        assertEquals(null, winner1);

        // Scenario 2: Drag downward, leading edge (bottom) is at y=210.
        // It HAS crossed the center of target (y=200).
        RecyclerView.ViewHolder winner2 = mCallback.chooseDropTarget(mViewHolder, targets, 0, 110);
        assertEquals(mTargetViewHolder, winner2);

        // Scenario 3: Drag upward, target is above selected.
        when(mTargetViewHolder.itemView.getTop()).thenReturn(-200);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(-100); // Center is y=-150

        // Drag upward, leading edge (top) is at y=-140.
        // It has NOT crossed the center of target (y=-150).
        RecyclerView.ViewHolder winner3 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -140);
        assertEquals(null, winner3);

        // Drag upward, leading edge (top) is at y=-160.
        // It HAS crossed the center of target (y=-150).
        RecyclerView.ViewHolder winner4 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -160);
        assertEquals(mTargetViewHolder, winner4);
    }

    @Test
    public void testChooseDropTarget_VerticalDrag_StandaloneTabToGroupLowestTab_SwapsAt25Percent() {
        // Setup currently dragged item (mViewHolder) as a normal standalone tab
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);

        // selected view layout
        when(mViewHolder.itemView.getLeft()).thenReturn(0);
        when(mViewHolder.itemView.getTop()).thenReturn(0);
        when(mViewHolder.itemView.getRight()).thenReturn(100);
        when(mViewHolder.itemView.getBottom()).thenReturn(100);

        // Setup target item as a child tab of a group
        when(mTargetViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mTargetPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mTargetPropertyModel.set(TabProperties.TAB_ID, 2);
        when(mTargetViewHolder.itemView.getLeft()).thenReturn(0);
        when(mTargetViewHolder.itemView.getTop()).thenReturn(-200);
        when(mTargetViewHolder.itemView.getRight()).thenReturn(100);
        when(mTargetViewHolder.itemView.getBottom()).thenReturn(-100); // Center is y=-150

        // Mock TabModel to target the lowest tab
        Tab targetTab = mock(Tab.class);
        when(targetTab.getId()).thenReturn(2);
        when(mTabModel.getRelatedTabList(2))
                .thenReturn(Arrays.asList(mock(Tab.class), targetTab)); // last item is target

        List<RecyclerView.ViewHolder> targets = Arrays.asList(mTargetViewHolder);

        // Standard 50% overlap is at y=-150.
        // But for grouping a standalone tab UPWARDS into the lowest tab of a group,
        // the threshold is 25% overlap (bottom quarter).
        // 25% overlap with bottom (-100) and height (100) -> threshold is -125.

        // Scenario 1: Drag upward, leading edge (top) is at -115.
        // Not crossed 25% threshold (-125).
        RecyclerView.ViewHolder winner1 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -115);
        assertEquals(null, winner1);

        // Scenario 2: Drag upward, leading edge (top) is at -135.
        // Crossed 25% threshold (-125).
        RecyclerView.ViewHolder winner2 = mCallback.chooseDropTarget(mViewHolder, targets, 0, -135);
        assertEquals(mTargetViewHolder, winner2);
    }

    @Test
    public void testHasDragEscapedBounds_GroupHeader() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB_GROUP);
        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, 0));
    }

    @Test
    public void testHasDragEscapedBounds_StandaloneTab() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, null);
        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, 0));
    }

    @Test
    public void testHasDragEscapedBounds_SolitaryChild() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        Tab tab1 = mock(Tab.class);
        when(tab1.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(1)).thenReturn(tab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(Arrays.asList(tab1));

        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, 0));
    }

    @Test
    public void testHasDragEscapedBounds_FirstChild_DragUp_ThresholdMet() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab2.getId()).thenReturn(2);
        when(tab1.getTabGroupId()).thenReturn(groupId);
        when(tab2.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(1)).thenReturn(tab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(Arrays.asList(tab1, tab2));

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Threshold is top - height/2 = 200 - 50 = 150
        // y < 150 -> return true
        assertTrue(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 140, 0, -10));
        verify(mTabUngrouper).ungroupTabs(List.of(tab1), false, false);
    }

    @Test
    public void testHasDragEscapedBounds_FirstChild_DragUp_ThresholdNotMet() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 1);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab2.getId()).thenReturn(2);
        when(tab1.getTabGroupId()).thenReturn(groupId);
        when(tab2.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(1)).thenReturn(tab1);
        when(mTabModel.getRelatedTabList(1)).thenReturn(Arrays.asList(tab1, tab2));

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Threshold is 150, y = 160 is not < 150
        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 160, 0, -10));
    }

    @Test
    public void testHasDragEscapedBounds_LastChild_DragDown_ThresholdMet() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 2);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab2.getId()).thenReturn(2);
        when(tab1.getTabGroupId()).thenReturn(groupId);
        when(tab2.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(2)).thenReturn(tab2);
        when(mTabModel.getRelatedTabList(2)).thenReturn(Arrays.asList(tab1, tab2)); // tab2 is last

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Threshold is top + height/4 = 200 + 25 = 225
        // y > 225 -> return true
        assertTrue(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 230, 0, 10));
        verify(mTabUngrouper).ungroupTabs(List.of(tab2), true, false);
    }

    @Test
    public void testHasDragEscapedBounds_MiddleChild_DragUpOrDown() {
        when(mViewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        Token groupId = new Token(1L, 2L);
        mPropertyModel.set(TabProperties.TAB_GROUP_ID, groupId);
        mPropertyModel.set(TabProperties.TAB_ID, 2);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        Tab tab3 = mock(Tab.class);
        when(tab1.getId()).thenReturn(1);
        when(tab2.getId()).thenReturn(2);
        when(tab3.getId()).thenReturn(3);
        when(tab1.getTabGroupId()).thenReturn(groupId);
        when(tab2.getTabGroupId()).thenReturn(groupId);
        when(tab3.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(2)).thenReturn(tab2);
        when(mTabModel.getRelatedTabList(2))
                .thenReturn(Arrays.asList(tab1, tab2, tab3)); // tab2 is middle

        when(mViewHolder.itemView.getHeight()).thenReturn(100);
        when(mViewHolder.itemView.getTop()).thenReturn(200);

        // Middle child should never escape bounds
        assertFalse(mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 0, 0, -50)); // up
        assertFalse(
                mCallback.hasDragEscapedBounds(mRecyclerView, mViewHolder, 0, 1000, 0, 50)); // down
    }
}
