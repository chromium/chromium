// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalExternalViewDragDropReorderStrategy.DropTargetResult;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link VerticalExternalViewDragDropReorderStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final.
        })
public class VerticalExternalViewDragDropReorderStrategyUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView mPinnedTabsRecyclerView;

    private Activity mActivity;
    private TabListModel mModelList;
    private TabListModel mPinnedTabsModelList;
    private VerticalExternalViewDragDropReorderStrategy mStrategy;

    private final List<View> mMainListChildren = new ArrayList<>();
    private final List<RecyclerView.ViewHolder> mMainListViewHolders = new ArrayList<>();
    private final List<View> mPinnedGridChildren = new ArrayList<>();
    private final List<RecyclerView.ViewHolder> mPinnedGridViewHolders = new ArrayList<>();
    private final List<Tab> mMockTabs = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mModelList = new TabListModel();
        mPinnedTabsModelList = new TabListModel();
        mMockTabs.clear();

        when(mRecyclerView.getWidth()).thenReturn(300);
        when(mRecyclerView.getHeight()).thenReturn(1000);
        when(mPinnedTabsRecyclerView.getWidth()).thenReturn(300);
        when(mPinnedTabsRecyclerView.getHeight()).thenReturn(200);
        when(mTabModel.getCount()).thenAnswer(invocation -> mMockTabs.size());

        mStrategy =
                new VerticalExternalViewDragDropReorderStrategy(
                        () -> mTabModel, mModelList, mRecyclerView, mPinnedTabsRecyclerView);
    }

    private View createChildView(int left, int top, int right, int bottom) {
        View view = new View(mActivity);
        view.layout(left, top, right, bottom);
        view.setLeft(left);
        view.setTop(top);
        view.setRight(right);
        view.setBottom(bottom);
        return view;
    }

    private void addMainListItem(
            PropertyModel model,
            @TabProperties.UiType int viewType,
            int left,
            int top,
            int right,
            int bottom) {
        int adapterPos = mModelList.size();
        mModelList.add(new ListItem(viewType, model));

        View childView = createChildView(left, top, right, bottom);
        SimpleRecyclerViewAdapter.ViewHolder vh =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(childView, (m, v, k) -> {}));
        vh.model = model;
        when(vh.getBindingAdapterPosition()).thenReturn(adapterPos);

        mMainListChildren.add(childView);
        mMainListViewHolders.add(vh);

        when(mRecyclerView.getChildCount()).thenReturn(mMainListChildren.size());
        for (int i = 0; i < mMainListChildren.size(); i++) {
            when(mRecyclerView.getChildAt(i)).thenReturn(mMainListChildren.get(i));
            when(mRecyclerView.getChildViewHolder(mMainListChildren.get(i)))
                    .thenReturn(mMainListViewHolders.get(i));
            when(mRecyclerView.getChildAdapterPosition(mMainListChildren.get(i))).thenReturn(i);
            when(mRecyclerView.getChildLayoutPosition(mMainListChildren.get(i))).thenReturn(i);
        }
    }

    private void addPinnedGridItem(PropertyModel model, int left, int top, int right, int bottom) {
        int adapterPos = mPinnedTabsModelList.size();
        mPinnedTabsModelList.add(new ListItem(TabProperties.UiType.PINNED_TAB, model));

        View childView = createChildView(left, top, right, bottom);
        SimpleRecyclerViewAdapter.ViewHolder vh =
                spy(new SimpleRecyclerViewAdapter.ViewHolder(childView, (m, v, k) -> {}));
        vh.model = model;
        when(vh.getBindingAdapterPosition()).thenReturn(adapterPos);

        mPinnedGridChildren.add(childView);
        mPinnedGridViewHolders.add(vh);

        when(mPinnedTabsRecyclerView.getChildCount()).thenReturn(mPinnedGridChildren.size());
        for (int i = 0; i < mPinnedGridChildren.size(); i++) {
            when(mPinnedTabsRecyclerView.getChildAt(i)).thenReturn(mPinnedGridChildren.get(i));
            when(mPinnedTabsRecyclerView.getChildViewHolder(mPinnedGridChildren.get(i)))
                    .thenReturn(mPinnedGridViewHolders.get(i));
            when(mPinnedTabsRecyclerView.getChildAdapterPosition(mPinnedGridChildren.get(i)))
                    .thenReturn(i);
            when(mPinnedTabsRecyclerView.getChildLayoutPosition(mPinnedGridChildren.get(i)))
                    .thenReturn(i);
        }
    }

    private void mockFindChildViewUnder(RecyclerView rv, List<View> children) {
        when(rv.findChildViewUnder(
                        org.mockito.ArgumentMatchers.anyFloat(),
                        org.mockito.ArgumentMatchers.anyFloat()))
                .thenAnswer(
                        invocation -> {
                            float x = invocation.getArgument(0);
                            float y = invocation.getArgument(1);
                            for (View child : children) {
                                if (x >= child.getLeft()
                                        && x <= child.getRight()
                                        && y >= child.getTop()
                                        && y <= child.getBottom()) {
                                    return child;
                                }
                            }
                            return null;
                        });
    }

    private Tab createMockTab(int id, boolean isPinned) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.getIsPinned()).thenReturn(isPinned);
        when(mTabModel.getTabById(id)).thenReturn(tab);
        mMockTabs.add(tab);
        return tab;
    }

    // ---------------------------------------------------------------------------------------------
    // Single Tab Over Standalone Tab
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testSingleTab_OverStandaloneTab_TopHalf_InsertsBefore() {
        Tab tab10 = createMockTab(10, /* isPinned= */ false);
        when(mTabModel.indexOf(tab10)).thenReturn(0);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 10)
                        .build();
        addMainListItem(model, TabProperties.UiType.TAB, 0, 0, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 25,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertTrue(result.insertBefore);
        assertFalse(result.isGroupTopOrBottomBoundary);
        assertEquals(0, result.adapterPosition);
        assertEquals(new Rect(0, 0, 300, 100), result.anchorBounds);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverStandaloneTab_BottomHalf_InsertsAfter() {
        Tab tab10 = createMockTab(10, /* isPinned= */ false);
        when(mTabModel.indexOf(tab10)).thenReturn(0);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 10)
                        .build();
        addMainListItem(model, TabProperties.UiType.TAB, 0, 0, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 75,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(1, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertFalse(result.insertBefore);
        assertFalse(result.isGroupTopOrBottomBoundary);
    }

    // ---------------------------------------------------------------------------------------------
    // Single Tab Over Expanded Tab Group
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testSingleTab_OverExpandedGroup_Header_TopHalf_SnapsAboveGroupAsStandaloneTab() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        // Header at pos 0 (top: 0, bottom: 50, centerY: 25)
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over top half of header (y = 15 in range 0..50)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 15,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertTrue(result.insertBefore);
        assertTrue(result.isGroupTopOrBottomBoundary);
        assertEquals(0, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverExpandedGroup_Header_BottomHalf_InsertsAsFirstChild() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        // Header at pos 0 (top: 0, bottom: 50, centerY: 25)
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over bottom half of header (y = 35 in range 0..50)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 35,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(20, result.destGroupTabId);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertFalse(result.insertBefore);
        assertFalse(result.isGroupTopOrBottomBoundary);
        assertEquals(0, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverExpandedGroup_ChildTab_TopHalf_InsertsBeforeChild() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over Child 2 at top half (y = 110 in range 100..150)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 110,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(1, result.destTabIndex);
        assertEquals(20, result.destGroupTabId);
        assertTrue(result.insertBefore);
        assertFalse(result.isGroupTopOrBottomBoundary);
        assertEquals(2, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void
            testSingleTab_OverExpandedGroup_LastChild_UpperBottomHalf_InsertsAfterChildInGroup() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Child 2 is range 100..150 (centerY: 125, 75% height: 137.5).
        // Hover at y = 130 (upper bottom half) -> inserts after Child 2 inside group.
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 130,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(20, result.destGroupTabId);
        assertFalse(result.insertBefore);
        assertFalse(result.isGroupTopOrBottomBoundary);
        assertEquals(2, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void
            testSingleTab_OverExpandedGroup_LastChild_LowerBottomHalf_SnapsBelowGroupAsStandaloneTab() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Child 2 is range 100..150 (centerY: 125, 75% height: 137.5).
        // Hover at y = 145 (lower bottom half >= 137.5) -> snaps below group as standalone tab.
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 145,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.insertBefore);
        assertTrue(result.isGroupTopOrBottomBoundary);
        assertEquals(2, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverExpandedGroup_BelowLastChild_SnapsBelowGroupAsStandaloneTab() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover below the entire list (y = 250 > 150)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 250,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.insertBefore);
        assertTrue(result.isGroupTopOrBottomBoundary);
        assertEquals(2, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverExpandedGroup_IntermediateChildTab_DropTargetPositions() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        Tab tab22 = createMockTab(22, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21, tab22));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.indexOf(tab22)).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        PropertyModel child3Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 22)
                        .build();
        addMainListItem(child3Model, TabProperties.UiType.TAB, 0, 150, 300, 200);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over Child 2 top half (y = 110) -> destTabIndex = 1 (inserts before child 2, index
        // 1)
        DropTargetResult resultTop =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 110,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);
        assertNotNull(resultTop);
        assertEquals(1, resultTop.destTabIndex);
        assertEquals(20, resultTop.destGroupTabId);
        assertTrue(resultTop.insertBefore);

        // Hover over Child 2 bottom half (y = 140) -> destTabIndex = 2 (inserts after child 2,
        // index 2)
        DropTargetResult resultBottom =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 140,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);
        assertNotNull(resultBottom);
        assertEquals(2, resultBottom.destTabIndex);
        assertEquals(20, resultBottom.destGroupTabId);
        assertFalse(resultBottom.insertBefore);
    }

    // ---------------------------------------------------------------------------------------------
    // Single Tab Over Collapsed Tab Group (Atomic rule)
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testSingleTab_OverCollapsedGroup_TopHalf_SnapsAbove_NoMerge() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, true)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over collapsed header at top half (y = 15 in range 0..50)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 15,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertTrue(result.insertBefore);
        assertTrue(result.isGroupTopOrBottomBoundary);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverCollapsedGroup_BottomHalf_SnapsBelow_NoMerge() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, true)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over collapsed header at bottom half (y = 35 in range 0..50)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 35,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex); // 1 + 1 = 2 (after tab21)
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.insertBefore);
        assertTrue(result.isGroupTopOrBottomBoundary);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
    }

    // ---------------------------------------------------------------------------------------------
    // Tab Group Over Tab Group
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testTabGroup_OverTabGroup_CloserToTop_SnapsBeforeGroupHeader() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        // Header: 0..50
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        // Child 1: 50..100
        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        // Child 2: 100..150
        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Envelope: 0..150, center = 75. Hover over Child 1 at y = 60 (< 75, closer to top)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 60,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertTrue(result.insertBefore);
        assertTrue(result.isGroupTopOrBottomBoundary);
        assertEquals(0, result.adapterPosition); // Snaps to header viewholder
    }

    @Test
    @SmallTest
    public void testTabGroup_OverTabGroup_CloserToBottom_SnapsAfterLastChild() {
        Token groupId = new Token(1L, 2L);
        Tab tab20 = createMockTab(20, /* isPinned= */ false);
        Tab tab21 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(tab20, tab21));
        when(mTabModel.indexOf(tab20)).thenReturn(0);
        when(mTabModel.indexOf(tab21)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        // Header: 0..50
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, false)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        // Child 1: 50..100
        PropertyModel child1Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(child1Model, TabProperties.UiType.TAB, 0, 50, 300, 100);

        // Child 2: 100..150
        PropertyModel child2Model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.TAB_ID, 21)
                        .build();
        addMainListItem(child2Model, TabProperties.UiType.TAB, 0, 100, 300, 150);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Envelope: 0..150, center = 75. Hover over Child 1 at y = 90 (>= 75, closer to bottom)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 90,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex); // 1 + 1 = 2 (after tab21)
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.insertBefore);
        assertTrue(result.isGroupTopOrBottomBoundary);
        assertEquals(2, result.adapterPosition); // Snaps to last child viewholder
    }

    // ---------------------------------------------------------------------------------------------
    // Tab Group Over Standalone Tab
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testTabGroup_OverStandaloneTab_TopHalf_InsertsBefore() {
        Tab tab10 = createMockTab(10, /* isPinned= */ false);
        when(mTabModel.indexOf(tab10)).thenReturn(0);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 10)
                        .build();
        addMainListItem(model, TabProperties.UiType.TAB, 0, 0, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 30,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertTrue(result.insertBefore);
        assertFalse(result.isGroupTopOrBottomBoundary);
    }

    @Test
    @SmallTest
    public void testTabGroup_OverStandaloneTab_BottomHalf_InsertsAfter() {
        Tab tab10 = createMockTab(10, /* isPinned= */ false);
        when(mTabModel.indexOf(tab10)).thenReturn(0);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 10)
                        .build();
        addMainListItem(model, TabProperties.UiType.TAB, 0, 0, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 70,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(1, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.insertBefore);
        assertFalse(result.isGroupTopOrBottomBoundary);
    }

    // ---------------------------------------------------------------------------------------------
    // Pinned Tab Drag in Grid
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testPinnedTab_InPinnedGrid_SlotCalculation_LeftHalf_InsertsBefore() {
        Tab tab1 = createMockTab(1, /* isPinned= */ true);
        Tab tab2 = createMockTab(2, /* isPinned= */ true);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);

        PropertyModel pModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel1, 0, 0, 100, 100);

        PropertyModel pModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 2)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel2, 100, 0, 200, 100);

        mockFindChildViewUnder(mPinnedTabsRecyclerView, mPinnedGridChildren);

        // Hover over pinned tab 1, left half (x = 30 in 0..100)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 30,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.PINNED_GRID, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertTrue(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertTrue(result.insertBefore);
        assertEquals(0, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void testPinnedTab_InPinnedGrid_SlotCalculation_RightHalf_InsertsAfter() {
        Tab tab1 = createMockTab(1, /* isPinned= */ true);
        Tab tab2 = createMockTab(2, /* isPinned= */ true);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);

        PropertyModel pModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel1, 0, 0, 100, 100);

        PropertyModel pModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 2)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel2, 100, 0, 200, 100);

        mockFindChildViewUnder(mPinnedTabsRecyclerView, mPinnedGridChildren);

        // Hover over pinned tab 1, right half (x = 70 in 0..100)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 70,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.PINNED_GRID, result.targetType);
        assertEquals(1, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertTrue(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertFalse(result.insertBefore);
        assertEquals(0, result.adapterPosition);
    }

    @Test
    @SmallTest
    public void testPinnedTab_InPinnedGrid_SlotCalculation_LastTab_RightHalf_InsertsAtEnd() {
        Tab tab1 = createMockTab(1, /* isPinned= */ true);
        Tab tab2 = createMockTab(2, /* isPinned= */ true);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);

        PropertyModel pModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel1, 0, 0, 100, 100);

        PropertyModel pModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 2)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel2, 100, 0, 200, 100);

        mockFindChildViewUnder(mPinnedTabsRecyclerView, mPinnedGridChildren);

        // Hover over pinned tab 2, right half (x = 170 in 100..200)
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 170,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.PINNED_GRID, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertTrue(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertFalse(result.insertBefore);
        assertEquals(1, result.adapterPosition);
    }

    // ---------------------------------------------------------------------------------------------
    // Pinned Tab into Zero-Pinned-Tab Window
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testPinnedTab_InZeroPinnedTabWindow_TargetsZeroState() {
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertTrue(result.isPinned);
        assertTrue(result.isZeroPinnedState);
        assertTrue(result.insertBefore);
    }

    // ---------------------------------------------------------------------------------------------
    // Rejection Rules (Section Isolation)
    // ---------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    public void testRegularTab_OverPinnedGrid_Rejects() {
        when(mTabModel.getCount()).thenReturn(4);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 50,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNull(result);
    }

    @Test
    @SmallTest
    public void testTabGroup_OverPinnedGrid_Rejects() {
        when(mTabModel.getCount()).thenReturn(4);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 50,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNull(result);
    }

    @Test
    @SmallTest
    public void testRegularTab_OverPinnedGrid_ZeroNormalTabs_ReturnsEmptyMainListDropTarget() {
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 50,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertTrue(result.isZeroNormalTabsState);
        assertNull(result.targetViewHolder);
        assertTrue(result.insertBefore);
    }

    @Test
    @SmallTest
    public void testTabGroup_OverPinnedGrid_ZeroNormalTabs_ReturnsEmptyMainListDropTarget() {
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 50,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, result.destGroupTabId);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertTrue(result.isZeroNormalTabsState);
        assertNull(result.targetViewHolder);
        assertTrue(result.insertBefore);
    }

    @Test
    @SmallTest
    public void testPinnedTab_OverRegularList_WhenPinnedTabsExist_Rejects() {
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 150,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNull(result);
    }

    @Test
    @SmallTest
    public void testClear_ResetsLastDropTargetResult() {
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNotNull(result);
        assertEquals(result, mStrategy.getLastDropTargetResult());

        mStrategy.clear();
        assertNull(mStrategy.getLastDropTargetResult());
    }

    @Test
    @SmallTest
    public void testCalculateDropTarget_WithChromeTabDropData() {
        Tab tab = createMockTab(5, /* isPinned= */ false);
        ChromeTabDropDataAndroid dropData =
                (ChromeTabDropDataAndroid)
                        new ChromeTabDropDataAndroid.Builder()
                                .withTab(tab)
                                .withTabInGroup(false)
                                .build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        when(mTabModel.indexOf(tab)).thenReturn(0);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 5)
                        .build();
        addMainListItem(model, TabProperties.UiType.TAB, 0, 0, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        DropTargetResult result =
                mStrategy.calculateDropTarget(mRecyclerView, /* xPx= */ 150, /* yPx= */ 25);

        assertNotNull(result);
        assertEquals(0, result.destTabIndex);
        assertTrue(result.insertBefore);

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testCalculateDropTarget_WithChromeTabGroupDropData() {
        Token groupId = new Token(1L, 2L);
        Tab tab = createMockTab(5, /* isPinned= */ false);
        TabGroupMetadata metadata =
                new TabGroupMetadata(
                        0, 0, groupId, new ArrayList<>(), 0, "Title", null, false, false, false);

        ChromeTabGroupDropDataAndroid dropData =
                (ChromeTabGroupDropDataAndroid)
                        new ChromeTabGroupDropDataAndroid.Builder()
                                .withTabGroupMetadata(metadata)
                                .withTabs(List.of(tab))
                                .build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        when(mTabModel.indexOf(tab)).thenReturn(0);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 5)
                        .build();
        addMainListItem(model, TabProperties.UiType.TAB, 0, 0, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        DropTargetResult result =
                mStrategy.calculateDropTarget(mRecyclerView, /* xPx= */ 150, /* yPx= */ 25);

        assertNotNull(result);
        assertEquals(0, result.destTabIndex);
        assertTrue(result.insertBefore);

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testPinnedTab_InPinnedGrid_Rtl_SlotCalculation() {
        LocalizationUtils.setRtlForTesting(true);

        Tab tab1 = createMockTab(1, /* isPinned= */ true);
        Tab tab2 = createMockTab(2, /* isPinned= */ true);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);

        PropertyModel pModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel1, 0, 0, 100, 100);

        PropertyModel pModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 2)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addPinnedGridItem(pModel2, 100, 0, 200, 100);

        mockFindChildViewUnder(mPinnedTabsRecyclerView, mPinnedGridChildren);

        // In RTL, right half (x = 70 in 0..100, center = 50) is the leading edge (insertBefore =
        // true -> index 0).
        DropTargetResult resultRightHalf =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 70,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNotNull(resultRightHalf);
        assertEquals(DropTargetResult.TargetType.PINNED_GRID, resultRightHalf.targetType);
        assertEquals(0, resultRightHalf.destTabIndex);
        assertTrue(resultRightHalf.insertBefore);

        // In RTL, left half (x = 30 in 0..100, center = 50) is the trailing edge (insertBefore =
        // false -> index 1).
        DropTargetResult resultLeftHalf =
                mStrategy.calculateDropTarget(
                        mPinnedTabsRecyclerView,
                        /* xPx= */ 30,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ true);

        assertNotNull(resultLeftHalf);
        assertEquals(DropTargetResult.TargetType.PINNED_GRID, resultLeftHalf.targetType);
        assertEquals(1, resultLeftHalf.destTabIndex);
        assertFalse(resultLeftHalf.insertBefore);
    }

    @Test
    @SmallTest
    public void testEmptyDestinationWindow_ReturnsFirstNonPinnedTabIndex() {
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);
        when(mRecyclerView.getChildCount()).thenReturn(0);
        assertEquals(0, mModelList.size());

        // Single tab drag into empty destination window.
        DropTargetResult resultSingleTab =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(resultSingleTab);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, resultSingleTab.targetType);
        assertEquals(2, resultSingleTab.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, resultSingleTab.destGroupTabId);
        assertFalse(resultSingleTab.isPinned);
        assertFalse(resultSingleTab.isZeroPinnedState);
        assertNull(resultSingleTab.targetViewHolder);
        assertEquals(0, resultSingleTab.adapterPosition);
        assertTrue(resultSingleTab.insertBefore);
        assertFalse(resultSingleTab.isGroupTopOrBottomBoundary);
        assertTrue(resultSingleTab.isZeroNormalTabsState);

        // Tab group drag into empty destination window.
        DropTargetResult resultGroup =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNotNull(resultGroup);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, resultGroup.targetType);
        assertEquals(2, resultGroup.destTabIndex);
        assertEquals(TabList.INVALID_TAB_INDEX, resultGroup.destGroupTabId);
        assertFalse(resultGroup.isPinned);
        assertFalse(resultGroup.isZeroPinnedState);
        assertTrue(resultGroup.isZeroNormalTabsState);
        assertNull(resultGroup.targetViewHolder);
        assertEquals(0, resultGroup.adapterPosition);
        assertTrue(resultGroup.insertBefore);
        assertFalse(resultGroup.isGroupTopOrBottomBoundary);
    }

    @Test
    @SmallTest
    public void testDestinationWindow_ZeroPinnedAndZeroNormalTabs_ReturnsFlagsFalse() {
        when(mTabModel.getCount()).thenReturn(0);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mRecyclerView.getChildCount()).thenReturn(0);
        assertEquals(0, mModelList.size());

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(0, result.destTabIndex);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertFalse(result.isZeroNormalTabsState);
        assertNull(result.targetViewHolder);
    }

    @Test
    @SmallTest
    public void testEmptyViewHierarchy_NonEmptyTabModel_ZeroNormalTabsIsFalse() {
        when(mTabModel.getCount()).thenReturn(4);
        when(mTabModel.getPinnedTabsCount()).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(1);
        when(mRecyclerView.getChildCount()).thenReturn(0);
        assertEquals(0, mModelList.size());

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(1, result.destTabIndex);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertFalse(result.isZeroNormalTabsState);
    }

    @Test
    @SmallTest
    public void testRegularTab_OverMainList_ZeroNormalTabs_WithPinnedItemsInModelList() {
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        PropertyModel pinnedModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        PropertyModel pinnedModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, 2)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addMainListItem(pinnedModel1, TabProperties.UiType.PINNED_TAB, 0, 0, 0, 0);
        addMainListItem(pinnedModel2, TabProperties.UiType.PINNED_TAB, 0, 0, 0, 0);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertTrue(result.isZeroNormalTabsState);
        assertNull(result.targetViewHolder);
    }

    @Test
    @SmallTest
    public void testTabGroup_OverMainList_ZeroNormalTabs_WithPinnedItemsInModelList() {
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        PropertyModel pinnedModel1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, 1)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        PropertyModel pinnedModel2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, 2)
                        .with(TabProperties.IS_PINNED, true)
                        .build();
        addMainListItem(pinnedModel1, TabProperties.UiType.PINNED_TAB, 0, 0, 0, 0);
        addMainListItem(pinnedModel2, TabProperties.UiType.PINNED_TAB, 0, 0, 0, 0);

        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 50,
                        /* isGroupDrag= */ true,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertFalse(result.isPinned);
        assertFalse(result.isZeroPinnedState);
        assertTrue(result.isZeroNormalTabsState);
        assertNull(result.targetViewHolder);
    }

    @Test
    @SmallTest
    public void testMainList_GapHover_FindChildViewUnderNull_ClosestChildFallbackActivates() {
        Tab tab1 = createMockTab(10, /* isPinned= */ false);
        Tab tab2 = createMockTab(11, /* isPinned= */ false);
        when(mTabModel.indexOf(tab1)).thenReturn(0);
        when(mTabModel.indexOf(tab2)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        // Item 0: 0..50 (center = 25)
        PropertyModel model1 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 10)
                        .build();
        addMainListItem(model1, TabProperties.UiType.TAB, 0, 0, 300, 50);

        // Item 1: 70..120 (center = 95) with a 20px gap (50..70)
        PropertyModel model2 =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 11)
                        .build();
        addMainListItem(model2, TabProperties.UiType.TAB, 0, 70, 300, 120);

        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover in gap at y = 55 (findChildViewUnder returns null).
        // Closest child is Item 0 (center 25, diff = 30 vs Item 1 center 95, diff = 40).
        // Since y = 55 > 25, insertBefore is false -> destTabIndex = 0 + 1 = 1.
        DropTargetResult resultCloserToItem0 =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 55,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(resultCloserToItem0);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, resultCloserToItem0.targetType);
        assertEquals(1, resultCloserToItem0.destTabIndex);
        assertEquals(0, resultCloserToItem0.adapterPosition);
        assertFalse(resultCloserToItem0.insertBefore);

        // Hover in gap at y = 65 (closest to Item 1: center 95, diff = 30 vs Item 0 diff = 40).
        // Since y = 65 < 95, insertBefore is true -> destTabIndex = 1.
        DropTargetResult resultCloserToItem1 =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 65,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(resultCloserToItem1);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, resultCloserToItem1.targetType);
        assertEquals(1, resultCloserToItem1.destTabIndex);
        assertEquals(1, resultCloserToItem1.adapterPosition);
        assertTrue(resultCloserToItem1.insertBefore);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverStandaloneTab_UnresolvableTab_FallbackScansPrecedingGroup() {
        Token groupId = new Token(1L, 2L);
        Tab groupTab0 = createMockTab(20, /* isPinned= */ false);
        Tab groupTab1 = createMockTab(21, /* isPinned= */ false);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(groupTab0, groupTab1));
        when(mTabModel.indexOf(groupTab0)).thenReturn(0);
        when(mTabModel.indexOf(groupTab1)).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        // Preceding group header at pos 0
        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, true)
                        .with(TabProperties.TAB_ID, 20)
                        .build();
        addMainListItem(headerModel, TabProperties.UiType.TAB_GROUP, 0, 0, 300, 50);

        // Standalone tab at pos 1 whose ID 999 does NOT exist in mTabModel (unresolvable)
        PropertyModel unresolvableModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 999)
                        .build();
        addMainListItem(unresolvableModel, TabProperties.UiType.TAB, 0, 50, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over top half of unresolvable item (y = 60).
        // Fallback backward scan resolves preceding group's lastIndex = 1 -> fallback modelIndex =
        // 2.
        // insertBefore = true -> destTabIndex = 2.
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 60,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(1, result.adapterPosition);
        assertTrue(result.insertBefore);
    }

    @Test
    @SmallTest
    public void testSingleTab_OverStandaloneTab_UnresolvableTab_FallbackScansFollowingTab() {
        createMockTab(10, /* isPinned= */ false);
        createMockTab(20, /* isPinned= */ false);
        Tab tabFollow = createMockTab(30, /* isPinned= */ false);
        when(mTabModel.indexOf(tabFollow)).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        // Standalone tab at pos 0 whose ID 999 does NOT exist in mTabModel (unresolvable)
        PropertyModel unresolvableModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 999)
                        .build();
        addMainListItem(unresolvableModel, TabProperties.UiType.TAB, 0, 0, 300, 50);

        // Following standalone tab at pos 1 with valid model index 2
        PropertyModel followModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, 30)
                        .build();
        addMainListItem(followModel, TabProperties.UiType.TAB, 0, 50, 300, 100);
        mockFindChildViewUnder(mRecyclerView, mMainListChildren);

        // Hover over top half of unresolvable item (y = 20).
        // Fallback forward scan resolves following tab's modelIndex = 2 -> fallback modelIndex = 2.
        // insertBefore = true -> destTabIndex = 2.
        DropTargetResult result =
                mStrategy.calculateDropTarget(
                        mRecyclerView,
                        /* xPx= */ 150,
                        /* yPx= */ 20,
                        /* isGroupDrag= */ false,
                        /* isPinnedDrag= */ false);

        assertNotNull(result);
        assertEquals(DropTargetResult.TargetType.MAIN_LIST, result.targetType);
        assertEquals(2, result.destTabIndex);
        assertEquals(0, result.adapterPosition);
        assertTrue(result.insertBefore);
    }
}
