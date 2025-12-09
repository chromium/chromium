// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ANIMATION_STATUS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_PINNED;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.graphics.Canvas;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link TabGridItemTouchHelperCallback}. */
@SuppressWarnings({"ResultOfMethodCallIgnored", "DirectInvocationOnMock"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final
        })
@DisableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
public class TabGridItemTouchHelperCallbackUnitTest {

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int POSITION4 = 3;
    private static final int ARCHIVED_MSG_CARD_POSITION = 4;
    private static final float THRESHOLD = 2f;
    private static final float MERGE_AREA_THRESHOLD = 0.5f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Canvas mCanvas;
    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView.Adapter mAdapter;
    @Spy private TabModel mTabModel;
    @Mock private TabActionListener mTabClosedListener;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private GridLayoutManager mGridLayoutManager;
    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    @Mock private TabGroupColorViewProvider mTabGroupColorViewProvider;

    @Mock private TabGridItemLongPressOrchestrator mTabGridItemLongPressOrchestrator;

    private final ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private SimpleRecyclerViewAdapter mSimpleAdapter;
    private ViewHolder mMockViewHolder1;
    private ViewHolder mMockViewHolder2;
    private ViewHolder mMockViewHolder3;
    private ViewHolder mMockViewHolder4;
    private ViewHolder mMockArchivedMsgViewHolder;
    private View mItemView1;
    private View mItemView2;
    private View mItemView3;
    private View mItemView4;
    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private Tab mTab4;
    private View mArchivedMsgItemView;
    private TabGridItemTouchHelperCallback mItemTouchHelperCallback;
    private TabListModel mModel;

    @Before
    public void setUp() {
        Handler handler = new Handler(Looper.getMainLooper());

        doCallback(
                        (Runnable r) -> {
                            handler.post(r);
                        })
                .when(mRecyclerView)
                .post(any());

        mModel = new TabListModel();
        mSimpleAdapter = new SimpleRecyclerViewAdapter(mModel);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = prepareTab(TAB3_ID, TAB3_TITLE);
        mTab4 = prepareTab(TAB4_ID, TAB4_TITLE);
        // Mock four cards in a grid layout. Each card is of width 4 and height 4. Both the side
        // gaps and top gaps between adjacent cards are 1.
        mItemView1 = prepareItemView(0, 0, 4, 4);
        mItemView2 = prepareItemView(5, 0, 9, 4);
        mItemView3 = prepareItemView(0, 5, 4, 9);
        mItemView4 = prepareItemView(5, 5, 9, 9);

        mTabGroupModelFilterSupplier.set(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        doReturn(mProfile).when(mTabModel).getProfile();
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab3).when(mTabModel).getTabAt(POSITION3);
        doReturn(mTab4).when(mTabModel).getTabAt(POSITION4);
        doReturn(mTab1).when(mTabModel).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModel).getTabById(TAB2_ID);
        doReturn(mTab3).when(mTabModel).getTabById(TAB3_ID);
        doReturn(mTab4).when(mTabModel).getTabById(TAB4_ID);
        doReturn(4).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(mTab3).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION3);
        doReturn(mTab4).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION4);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB2_ID).when(mTab2).getRootId();
        doReturn(TAB3_ID).when(mTab3).getRootId();
        doReturn(TAB4_ID).when(mTab4).getRootId();
        initAndAssertAllProperties();

        setupRecyclerView();

        setupItemTouchHelperCallback(false);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    private void setupRecyclerView() {
        doReturn(mAdapter).when(mRecyclerView).getAdapter();
        doReturn(mGridLayoutManager).when(mRecyclerView).getLayoutManager();
        doReturn(14).when(mRecyclerView).getBottom();
        doReturn(4).when(mRecyclerView).getChildCount();
        doReturn(4).when(mAdapter).getItemCount();
        when(mRecyclerView.getChildAt(POSITION1)).thenReturn(mItemView1);
        when(mRecyclerView.getChildAt(POSITION2)).thenReturn(mItemView2);
        when(mRecyclerView.getChildAt(POSITION3)).thenReturn(mItemView3);
        when(mRecyclerView.getChildAt(POSITION4)).thenReturn(mItemView4);
        doReturn(mRecyclerView).when(mItemView1).getParent();
        doReturn(mRecyclerView).when(mItemView2).getParent();
        doReturn(mRecyclerView).when(mItemView3).getParent();
        doReturn(mRecyclerView).when(mItemView4).getParent();
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION1))
                .thenReturn(mMockViewHolder1);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION2))
                .thenReturn(mMockViewHolder2);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION3))
                .thenReturn(mMockViewHolder3);
        when(mRecyclerView.findViewHolderForAdapterPosition(POSITION4))
                .thenReturn(mMockViewHolder4);
    }

    private void setupItemTouchHelperCallback(boolean isDialog) {
        mItemTouchHelperCallback =
                new TabGridItemTouchHelperCallback(
                        ContextUtils.getApplicationContext(),
                        mTabGroupCreationDialogManager,
                        mModel,
                        mTabGroupModelFilterSupplier,
                        mTabClosedListener,
                        isDialog ? mTabGridDialogHandler : null,
                        "",
                        !isDialog,
                        TabListMode.GRID);
        mItemTouchHelperCallback.setupCallback(THRESHOLD, MERGE_AREA_THRESHOLD, THRESHOLD);
        mItemTouchHelperCallback.getMovementFlags(mRecyclerView, mMockViewHolder1);
    }

    @Test
    public void onStartDraggingTab() {
        assertThat(
                mModel.get(0).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(1f));

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_DRAG);

        assertThat(
                mModel.get(0).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.SELECTED_CARD_ZOOM_IN));
        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(0.8f));
    }

    @Test
    public void onSwipeTab_Delete() {
        mItemTouchHelperCallback.onSwiped(mMockViewHolder1, POSITION1);

        verify(mTabClosedListener).run(mItemView1, TAB1_ID, /* triggeringMotion= */ null);
    }

    @Test
    public void onReleaseTab_NoMerge() {
        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertThat(
                mModel.get(0).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.SELECTED_CARD_ZOOM_OUT));
        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(
                mModel.get(1).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.CARD_RESTORE));
        assertThat(mModel.get(1).model.get(CARD_ALPHA), equalTo(1f));
    }

    @Test
    public void onReleaseTab_NoMergeCollaboration() {
        // Dragged object is a collaboration.
        when(mTabGroupColorViewProvider.hasCollaborationId()).thenReturn(true);
        mMockViewHolder1.model.set(
                TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, mTabGroupColorViewProvider);

        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Simulate hovering on card#2.
        mModel.get(1)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt());
        verify(mGridLayoutManager, never()).removeView(any());
    }

    @Test
    public void onReleaseTab_MergeBackward() {
        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Simulate hovering on card#2.
        mModel.get(1)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB1_ID, TAB2_ID);
        verify(mGridLayoutManager).removeView(mItemView1);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
        assertThat(
                mModel.get(0).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.HOVERED_CARD_ZOOM_OUT));
    }

    @Test
    public void onReleaseTab_Merge_NotAttachedToWindow() {
        // Simulate the selection of card#1 in TabListModel.
        mModel.get(0)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(0).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Simulate hovering on card#2.
        mModel.get(1)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION2);

        when(mItemView1.isAttachedToWindow()).thenReturn(false);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB1_ID, TAB2_ID);
        verify(mGridLayoutManager, never()).removeView(mItemView1);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
        assertThat(
                mModel.get(0).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.HOVERED_CARD_ZOOM_OUT));
    }

    @Test
    public void onReleaseTab_MergeForward() {
        // Simulate the selection of card#2 in TabListModel.
        mModel.get(1)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.SELECTED_CARD_ZOOM_IN);
        mModel.get(1).model.set(CARD_ALPHA, 0.8f);
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);

        // Simulate hovering on card#1.
        mModel.get(0)
                .model
                .set(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB2_ID, TAB1_ID);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
        assertThat(
                mModel.get(0).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.HOVERED_CARD_ZOOM_OUT));
    }

    @Test
    public void onReleaseTab_Merge_CleanOut() {
        // Simulate the selection of card#2 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);

        // Simulate hovering on card#1.
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is cleaned out when the drop-to-merge happens.
        doReturn(null).when(mRecyclerView).findViewHolderForAdapterPosition(anyInt());

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager, never()).removeView(mItemView2);
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(TAB2_ID, TAB1_ID);
        verify(mTracker, never()).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    @Test
    public void onReleaseTab_Merge_Scrolling() {
        // Simulate the selection of card#2 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);

        // Simulate hovering on card#1.
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is scrolling when the drop-to-merge happens.
        when(mRecyclerView.isComputingLayout()).thenReturn(true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager, never()).removeView(mItemView2);
        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(TAB2_ID, TAB1_ID);
        verify(mTracker, never()).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    @Test
    public void onReleaseTab_UngroupBar_Hide() {
        setupItemTouchHelperCallback(true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
    }

    @Test
    public void onReleaseTab_Ungroup() {
        setupItemTouchHelperCallback(true);
        mItemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabUngrouper)
                .ungroupTabs(
                        List.of(mTabModel.getTabById(TAB1_ID)),
                        /* trailing= */ true,
                        /* allowDialog= */ true);
        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void onReleaseTab_Ungroup_Scrolling() {
        setupItemTouchHelperCallback(true);
        mItemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is scrolling when the drop-to-ungroup happens.
        when(mRecyclerView.isComputingLayout()).thenReturn(true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabUngrouper, never())
                .ungroupTabs(
                        List.of(mTabModel.getTabById(TAB1_ID)),
                        /* trailing= */ true,
                        /* allowDialog= */ true);
        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onReleaseTab_Ungroup_CleanOut() {
        setupItemTouchHelperCallback(true);
        mItemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);

        // Simulate that the recyclerView is cleaned out when the drop-to-ungroup happens.
        doReturn(null).when(mRecyclerView).findViewHolderForAdapterPosition(anyInt());

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabUngrouper, never())
                .ungroupTabs(
                        List.of(mTabModel.getTabById(TAB1_ID)),
                        /* trailing= */ true,
                        /* allowDialog= */ true);
        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HIDE);
        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onDragTab_Hovered_Gts_OneCollaborationCannotDrop() {
        when(mTabGroupColorViewProvider.hasCollaborationId()).thenReturn(true);
        mMockViewHolder1.model.set(
                TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, mTabGroupColorViewProvider);
        // Drag card#1 rightwards to hover on card#2. We cannot drop a collaboration over a normal
        // tab or it will be destroyed.
        verifyDrag(mMockViewHolder1, 5, 0, POSITION2, AnimationStatus.CARD_RESTORE);
        // Drag card#2 leftwards to hover on card#1. This is still allowed as we can add tabs to a
        // collaboration.
        verifyDrag(mMockViewHolder2, -5, 0, POSITION1, AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_Hovered_Gts_TwoCollaborationCannotDrop() {
        when(mTabGroupColorViewProvider.hasCollaborationId()).thenReturn(true);
        mMockViewHolder1.model.set(
                TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, mTabGroupColorViewProvider);
        mMockViewHolder2.model.set(
                TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, mTabGroupColorViewProvider);

        // Merging collaborations is not allowed. Neither of these should work.
        verifyDrag(mMockViewHolder1, 5, 0, POSITION2, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder2, -5, 0, POSITION1, AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_Hovered_Gts_Horizontal() {
        // Drag card#1 rightwards to hover on card#2.
        verifyDrag(mMockViewHolder1, 5, 0, POSITION2, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#2 leftwards to hover on card#1.
        verifyDrag(mMockViewHolder2, -5, 0, POSITION1, AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_Hovered_Gts_Vertical() {
        // Drag card#1 downwards to hover on card#3.
        verifyDrag(mMockViewHolder1, 0, 5, POSITION3, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#3 upwards to hover on card#1.
        verifyDrag(mMockViewHolder3, 0, -5, POSITION1, AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_Hovered_Gts_Diagonal() {
        // Drag card#1 diagonally to hover on card#4.
        verifyDrag(mMockViewHolder1, 5, 5, POSITION4, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#4 diagonally to hover on card#1.
        verifyDrag(mMockViewHolder4, -5, -5, POSITION1, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#2 diagonally to hover on card#3.
        verifyDrag(mMockViewHolder2, -5, 5, POSITION3, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Drag card#3 diagonally to hover on card#2.
        verifyDrag(mMockViewHolder3, 5, -5, POSITION2, AnimationStatus.HOVERED_CARD_ZOOM_IN);
    }

    @Test
    public void onDragTab_NotHovered_Gts_Horizontal() {
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mMockViewHolder1, 3, 0, POSITION2, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder2, -3, 0, POSITION1, AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mMockViewHolder1, 7, 0, POSITION2, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder2, -7, 0, POSITION1, AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_NotHovered_Gts_Vertical() {
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mMockViewHolder1, 0, 3, POSITION3, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder3, 0, -3, POSITION1, AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mMockViewHolder1, 0, 7, POSITION3, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder3, 0, -7, POSITION1, AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_NotHovered_Gts_Diagonal() {
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mMockViewHolder1, 3, 4, POSITION4, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder1, 4, 3, POSITION4, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder4, -4, -3, POSITION1, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder4, -3, -4, POSITION1, AnimationStatus.CARD_RESTORE);
        // With merge threshold equal to 50% of the overlapped area, the following dX should never
        // trigger hovering.
        verifyDrag(mMockViewHolder1, 7, 6, POSITION4, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder1, 6, 7, POSITION4, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder4, -6, -7, POSITION1, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder4, -7, -6, POSITION1, AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_Restore_Hovered_Gts() {
        // Simulate the process of hovering card#1 on card#2.
        verifyDrag(mMockViewHolder1, 5, 0, POSITION2, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 horizontally so that it is no longer hovering on card#2.
        verifyDrag(mMockViewHolder1, 10, 0, POSITION2, AnimationStatus.HOVERED_CARD_ZOOM_OUT);

        // Simulate the process of hovering card#1 on card#3.
        verifyDrag(mMockViewHolder1, 0, 5, POSITION3, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 vertically so that it is no longer hovering on card#3.
        verifyDrag(mMockViewHolder1, 0, 10, POSITION3, AnimationStatus.HOVERED_CARD_ZOOM_OUT);

        // Simulate the process of hovering card#1 on card#4.
        verifyDrag(mMockViewHolder1, 5, 5, POSITION4, AnimationStatus.HOVERED_CARD_ZOOM_IN);
        // Continue to drag card#1 diagonally so that it is no longer hovering on card#4.
        verifyDrag(mMockViewHolder1, 10, 10, POSITION4, AnimationStatus.HOVERED_CARD_ZOOM_OUT);
    }

    @Test
    public void onDragTab_Hovered_NonGts() {
        // Suppose drag happens in components other than GTS.
        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(false);

        // Hovering shouldn't make any difference.
        verifyDrag(mMockViewHolder1, 5, 0, POSITION2, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder2, -5, 0, POSITION1, AnimationStatus.CARD_RESTORE);

        verifyDrag(mMockViewHolder1, 0, 5, POSITION3, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder3, 0, -5, POSITION1, AnimationStatus.CARD_RESTORE);

        verifyDrag(mMockViewHolder1, 5, 5, POSITION4, AnimationStatus.CARD_RESTORE);
        verifyDrag(mMockViewHolder4, -5, -5, POSITION1, AnimationStatus.CARD_RESTORE);
    }

    @Test
    public void onDragTab_Ungroup() {
        setupItemTouchHelperCallback(true);

        // Simulate dragging card#1 down to the ungroup bar.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                12,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HOVERED);

        // Simulate dragging card#3 down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                4,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler)
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HOVERED);
    }

    @Test
    public void onDragTab_NotUngroup() {
        setupItemTouchHelperCallback(true);

        // With recyclerview bottom equal to 14 and ungroup threshold equal to 2, any drag with
        // itemview.bottom + dY <= 12 should never trigger ungroup.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                6,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        // Simulate dragging card#3 down to the ungroup bar.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION3);
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder3,
                0,
                1,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler, times(2))
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.SHOW);

        verify(mTabGridDialogHandler, never())
                .updateUngroupBarStatus(TabGridDialogView.UngroupBarStatus.HOVERED);
    }

    @Test
    public void onDragTab_AfterRelease() {
        setupItemTouchHelperCallback(true);

        // Simulate that drop is finished, but there are some extra onChildDraw calls.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(TabModel.INVALID_TAB_INDEX);

        // Simulate dragging the tab down to the ungroup bar.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                8,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        verify(mTabGridDialogHandler, never()).updateUngroupBarStatus(anyInt());
    }

    private void clearViewBeforePost() {
        setupItemTouchHelperCallback(false);
        // Mock that when the dragging animation ends, the recyclerView is in an inconsistent state:
        // recyclerView should be cleaned out, yet the animated view is stale.
        mItemTouchHelperCallback.setCurrentActionStateForTesting(ItemTouchHelper.ACTION_STATE_DRAG);
        doReturn(1).when(mRecyclerView).getChildCount();
        doReturn(0).when(mAdapter).getItemCount();
        when(mItemView1.getParent()).thenReturn(mRecyclerView);
        when(mMockViewHolder1.getLayoutPosition()).thenReturn(POSITION1);
        when(mRecyclerView.indexOfChild(mItemView1)).thenReturn(POSITION1);

        mItemTouchHelperCallback.clearView(mRecyclerView, mMockViewHolder1);
    }

    @Test
    public void onDraggingAnimationEnd_Stale() {
        clearViewBeforePost();
        ShadowLooper.runUiThreadTasks();

        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void onDraggingAnimationEnd_NoParent() {
        clearViewBeforePost();

        when(mItemView1.getParent()).thenReturn(null);

        ShadowLooper.runUiThreadTasks();

        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onDraggingAnimationEnd_Stale_NoLayoutManager() {
        clearViewBeforePost();

        when(mRecyclerView.getLayoutManager()).thenReturn(null);

        ShadowLooper.runUiThreadTasks();

        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onDraggingAnimationEnd_Stale_HasNoRvItems() {
        clearViewBeforePost();

        when(mRecyclerView.getChildCount()).thenReturn(0);

        ShadowLooper.runUiThreadTasks();

        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onDraggingAnimationEnd_Stale_HasAdapterItems() {
        clearViewBeforePost();

        when(mAdapter.getItemCount()).thenReturn(1);

        ShadowLooper.runUiThreadTasks();

        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void onDraggingAnimationEnd_Stale_NoAdapter() {
        clearViewBeforePost();

        when(mRecyclerView.getAdapter()).thenReturn(null);

        ShadowLooper.runUiThreadTasks();

        verify(mGridLayoutManager, never()).removeView(mItemView1);
    }

    @Test
    public void messageItemNotDraggable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.IPH_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));

        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));

        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.TAB_GROUP_SUGGESTION_MESSAGE);
        mMockViewHolder1.model = mock(PropertyModel.class);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void messageItemSwipeable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.IPH_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertTrue(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void messageItemSwipeable_archivedTabsMessageNotSwipeable() {
        PropertyModel model = mock(PropertyModel.class);
        when(model.get(MESSAGE_TYPE)).thenReturn(MessageType.ARCHIVED_TABS_MESSAGE);
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.ARCHIVED_TABS_MESSAGE);
        mMockViewHolder1.model = model;

        setupItemTouchHelperCallback(false);
        assertFalse(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void messageItemSwipeable_pinnedTabNotSwipeable() {
        mMockViewHolder1.model.set(IS_PINNED, true);

        setupItemTouchHelperCallback(false);
        assertFalse(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void messageItemNotDropable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.IPH_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder2, mMockViewHolder1));
    }

    @Test
    public void dropItemOnTabArchivalMessageCardItem() {
        PropertyModel model =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(MESSAGE_TYPE, MessageType.ARCHIVED_TABS_MESSAGE)
                        .with(CARD_TYPE, UiType.ARCHIVED_TABS_IPH_MESSAGE)
                        .build();

        ViewHolder mockViewHolder = prepareMockViewHolder(model, mItemView2, POSITION2);
        setupItemTouchHelperCallback(false);
        assertTrue(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mockViewHolder, mMockViewHolder1));
    }

    @Test
    public void tabItemsAreDropable() {
        setupItemTouchHelperCallback(false);
        assertTrue(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder2, mMockViewHolder1));
    }

    @Test(expected = AssertionError.class)
    public void messageItemOnMoveFail() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.IPH_MESSAGE);
        setupItemTouchHelperCallback(false);
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder2);
    }

    @Test
    public void largeMessageItemNotDraggable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void largeMessageItemSwipeable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertTrue(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void largeMessageItemNotDropable() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        setupItemTouchHelperCallback(false);
        assertFalse(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder2, mMockViewHolder1));
    }

    @Test(expected = AssertionError.class)
    public void largeMessageItemOnMoveFail() {
        when(mMockViewHolder1.getItemViewType()).thenReturn(UiType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        setupItemTouchHelperCallback(false);
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder2);
    }

    @Test
    public void onLongPress_blockNextAction() {
        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertTrue(mItemTouchHelperCallback.shouldBlockAction());
    }

    @Test
    public void onLongPressWithDrag_dontBlockNextAction() {
        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Pretend a drag started.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                10,
                5,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        assertFalse(mItemTouchHelperCallback.shouldBlockAction());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void onDropOverArchivalCard() {
        setupItemTouchHelperCallback(false);
        addArchivedMessageCard();

        AtomicInteger recordedTabId = new AtomicInteger(TabModel.INVALID_TAB_INDEX);

        mItemTouchHelperCallback.setOnDropOnArchivalMessageCardEventListener(recordedTabId::set);
        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Pretend a drag over the archived message card has started.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                4,
                8,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        assertEquals(
                AnimationStatus.HOVERED_CARD_ZOOM_IN,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));

        mItemTouchHelperCallback.onSelectedChanged(null, ItemTouchHelper.ACTION_STATE_IDLE);
        assertEquals(
                AnimationStatus.HOVERED_CARD_ZOOM_OUT,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));
        assertEquals(TAB1_ID, recordedTabId.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void onDropOverArchivalCard_withoutHovering() {
        setupItemTouchHelperCallback(false);
        addArchivedMessageCard();

        AtomicInteger recordedTabId = new AtomicInteger(TabModel.INVALID_TAB_INDEX);

        mItemTouchHelperCallback.setOnDropOnArchivalMessageCardEventListener(recordedTabId::set);
        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));

        mItemTouchHelperCallback.onSelectedChanged(null, ItemTouchHelper.ACTION_STATE_IDLE);
        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));
        assertEquals(TabModel.INVALID_TAB_INDEX, recordedTabId.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void onHoverOverArchivalCard() {
        setupItemTouchHelperCallback(false);
        addArchivedMessageCard();

        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Pretend a drag over the archived message card has started.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                4,
                8,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        assertEquals(
                AnimationStatus.HOVERED_CARD_ZOOM_IN,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));

        // Return to original position.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                0,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(
                AnimationStatus.HOVERED_CARD_ZOOM_OUT,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void onHoverOverArchivalCard_archivalDropDisabled() {
        setupItemTouchHelperCallback(false);
        addArchivedMessageCard();

        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Pretend a drag over the archived message card has started.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                4,
                8,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));

        // Return to original position.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                0,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void onHoverAndDropPinnedTabOverArchivalCard() {
        setupItemTouchHelperCallback(false);
        addArchivedMessageCard();
        mModel.get(POSITION1).model.set(IS_PINNED, true);

        AtomicInteger recordedTabId = new AtomicInteger(TabModel.INVALID_TAB_INDEX);

        mItemTouchHelperCallback.setOnDropOnArchivalMessageCardEventListener(recordedTabId::set);
        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Pretend a drag over the archived message card has started.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                4,
                8,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));

        // Return to original position.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                0,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ARCHIVAL_DRAG_DROP_ANDROID)
    public void onHoverOverArchivalCard_sharedTabGroup() {
        when(mTabGroupColorViewProvider.hasCollaborationId()).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(Token.createRandom());
        mMockViewHolder1.model.set(
                TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, mTabGroupColorViewProvider);

        setupItemTouchHelperCallback(false);
        addArchivedMessageCard();

        mItemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Pretend a drag over the archived message card has started.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                4,
                8,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));

        // Return to original position.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                0,
                0,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);
        assertEquals(
                AnimationStatus.CARD_RESTORE,
                mModel.get(ARCHIVED_MSG_CARD_POSITION).model.get(CARD_ANIMATION_STATUS));
    }

    @Test
    public void onTabMergeToGroup_willMergingCreateNewGroup() {
        doReturn(true).when(mTabGroupModelFilter).willMergingCreateNewGroup(any());

        // Simulate the selection of card#1 in TabListModel.
        mItemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION1);

        // Simulate hovering on card#2.
        mItemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION2);

        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(TAB1_ID, TAB2_ID);
        verify(mTabGroupCreationDialogManager)
                .showDialog(mTabModel.getTabById(TAB2_ID).getTabGroupId(), mTabGroupModelFilter);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void orchestratorCreatedOnParityEnabled() {
        mItemTouchHelperCallback = spy(mItemTouchHelperCallback);
        mItemTouchHelperCallback.setOnLongPressTabItemEventListener((a, b) -> () -> {});
        verify(mItemTouchHelperCallback)
                .setTabGridItemLongPressOrchestrator(any(TabGridItemLongPressOrchestrator.class));
    }

    @Test(expected = AssertionError.class)
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void orchestratorNotCreatedTwice() {
        mItemTouchHelperCallback = spy(mItemTouchHelperCallback);
        mItemTouchHelperCallback.setOnLongPressTabItemEventListener((a, b) -> () -> {});
        mItemTouchHelperCallback.setOnLongPressTabItemEventListener((a, b) -> () -> {});
        verify(mItemTouchHelperCallback)
                .setTabGridItemLongPressOrchestrator(any(TabGridItemLongPressOrchestrator.class));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void orchestratorNotCreatedOnParityDisabled() {
        mItemTouchHelperCallback = spy(mItemTouchHelperCallback);
        mItemTouchHelperCallback.setOnLongPressTabItemEventListener((a, b) -> () -> {});
        verify(mItemTouchHelperCallback, never())
                .setTabGridItemLongPressOrchestrator(any(TabGridItemLongPressOrchestrator.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void orchestratorTriggeredOnSelectedChanged() {
        mItemTouchHelperCallback.setTabGridItemLongPressOrchestrator(
                mTabGridItemLongPressOrchestrator);
        mItemTouchHelperCallback.onSelectedChanged(
                mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);
        verify(mTabGridItemLongPressOrchestrator)
                .onSelectedChanged(
                        mMockViewHolder1.getBindingAdapterPosition(),
                        ItemTouchHelper.ACTION_STATE_IDLE);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void orchestratorTriggeredOnChildDraw() {
        mItemTouchHelperCallback.setTabGridItemLongPressOrchestrator(
                mTabGridItemLongPressOrchestrator);
        float displacement = 2.f;
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                mMockViewHolder1,
                displacement,
                displacement,
                ItemTouchHelper.ACTION_STATE_IDLE,
                true);
        float displacementSquared = displacement * displacement;
        verify(mTabGridItemLongPressOrchestrator)
                .processChildDisplacement(displacementSquared + displacementSquared);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void orchestratorCancelledOnClearView() {
        mItemTouchHelperCallback.setTabGridItemLongPressOrchestrator(
                mTabGridItemLongPressOrchestrator);
        mItemTouchHelperCallback.setCurrentActionStateForTesting(ItemTouchHelper.ACTION_STATE_DRAG);
        mItemTouchHelperCallback.clearView(mRecyclerView, mMockViewHolder1);
        verify(mTabGridItemLongPressOrchestrator).cancel();
    }

    @Test
    public void getMovementFlags_mouseInput_disablesSwipe() {
        mItemTouchHelperCallback.setIsMouseInputSource(true);
        assertFalse(mItemTouchHelperCallback.hasSwipeFlag(mRecyclerView, mMockViewHolder1));
        assertTrue(mItemTouchHelperCallback.hasDragFlagForTesting(mRecyclerView, mMockViewHolder1));
    }

    @Test
    public void interpolateOutOfBoundsScroll_mouseInput_returnsZero() {
        mItemTouchHelperCallback.setIsMouseInputSource(true);
        assertEquals(
                0,
                mItemTouchHelperCallback.interpolateOutOfBoundsScroll(
                        mRecyclerView, 100, 10, 1000, 100));
    }

    @Test
    public void testOnMove_PinnedTab_WithinPinnedTabs() {
        // Setup: 2 pinned tabs and 2 unpinned tabs.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTab3.getIsPinned()).thenReturn(false);
        when(mTab4.getIsPinned()).thenReturn(false);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        // Drag pinned tab1 to pinned tab2's position.
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder2);
        // Verify that tab1 is moved to index 1.
        verify(mTabGroupModelFilter).moveRelatedTabs(TAB1_ID, 1);
    }

    @Test
    public void testOnMove_UnpinnedTab_WithinUnpinnedTabs() {
        // Setup: 2 pinned tabs and 2 unpinned tabs.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTab3.getIsPinned()).thenReturn(false);
        when(mTab4.getIsPinned()).thenReturn(false);
        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID)).thenReturn(List.of(mTab3));
        when(mTabGroupModelFilter.getRelatedTabList(TAB4_ID)).thenReturn(List.of(mTab4));
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);
        when(mTabModel.indexOf(mTab4)).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        // Drag unpinned tab3 to unpinned tab4's position.
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder3, mMockViewHolder4);

        // Verify that tab3 is moved to index 3.
        verify(mTabGroupModelFilter).moveRelatedTabs(TAB3_ID, 3);
    }

    @Test
    public void testOnMove_GroupedTab_pinnedTabTriedToMoveBeyondLimit() {
        // Setup: 2 pinned tabs, 2 grouped tabs.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTab3.getIsPinned()).thenReturn(false);
        when(mTab4.getIsPinned()).thenReturn(false);
        Token groupId = Token.createRandom();
        when(mTab3.getTabGroupId()).thenReturn(groupId);
        when(mTab4.getTabGroupId()).thenReturn(groupId);

        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID)).thenReturn(List.of(mTab3, mTab4));
        when(mTabGroupModelFilter.getRelatedTabList(TAB4_ID)).thenReturn(List.of(mTab3, mTab4));

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);
        when(mTabModel.indexOf(mTab4)).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        // Try drag a pinned tab to an unpinned tab's position.
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder4);

        // Verify that the tab is moved to index 1, the last possible position for a pinned tab.
        verify(mTabGroupModelFilter).moveRelatedTabs(TAB1_ID, 1);
    }

    @Test
    public void testOnMove_GroupedTab_unpinnedTabTriedToMoveIntoPinnedArea() {
        // Setup: 2 pinned tabs, 2 grouped tabs.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTab3.getIsPinned()).thenReturn(false);
        when(mTab4.getIsPinned()).thenReturn(false);
        Token groupId = Token.createRandom();
        when(mTab3.getTabGroupId()).thenReturn(groupId);
        when(mTab4.getTabGroupId()).thenReturn(groupId);

        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID)).thenReturn(List.of(mTab3, mTab4));
        when(mTabGroupModelFilter.getRelatedTabList(TAB4_ID)).thenReturn(List.of(mTab3, mTab4));

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);
        when(mTabModel.indexOf(mTab4)).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(2);

        // Try drag an unpinned tab to a pinned tab's position.
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder3, mMockViewHolder1);

        // Verify that the tab is moved to index 2, the first possible position for an unpinned
        // tab.
        verify(mTabGroupModelFilter).moveRelatedTabs(TAB3_ID, 2);
    }

    @Test
    public void testOnMove_AllUnpinnedTabs_MovedAround() {
        // Setup: all tabs are unpinned.
        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTab3.getIsPinned()).thenReturn(false);
        when(mTab4.getIsPinned()).thenReturn(false);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB4_ID)).thenReturn(List.of(mTab4));
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab4)).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);

        // Drag tab1 to tab4's position.
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder4);

        // Verify that tab1 is moved to index 3.
        verify(mTabGroupModelFilter).moveRelatedTabs(TAB1_ID, 3);
    }

    @Test
    public void testOnMove_AllPinnedTabs_MovedAround() {
        // Setup: all tabs are pinned.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTab3.getIsPinned()).thenReturn(true);
        when(mTab4.getIsPinned()).thenReturn(true);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB4_ID)).thenReturn(List.of(mTab4));
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab4)).thenReturn(3);
        // All tabs are pinned, so the first non-pinned tab is at the end of the list.
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(4);

        // Drag tab1 to tab4's position.
        mItemTouchHelperCallback.onMove(mRecyclerView, mMockViewHolder1, mMockViewHolder4);

        // Verify that tab1 is moved to index 3.
        verify(mTabGroupModelFilter).moveRelatedTabs(TAB1_ID, 3);
    }

    @Test
    public void canDropOver_pinnedAndUnpinned() {
        setupItemTouchHelperCallback(false);

        // A pinned tab cannot be dropped on an unpinned tab.
        mMockViewHolder1.model.set(IS_PINNED, true);
        mMockViewHolder2.model.set(IS_PINNED, false);
        assertFalse(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder1, mMockViewHolder2));

        // An unpinned tab cannot be dropped on a pinned tab.
        assertFalse(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder2, mMockViewHolder1));

        // A pinned tab can be dropped on another pinned tab.
        mMockViewHolder2.model.set(IS_PINNED, true);
        assertTrue(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder1, mMockViewHolder2));

        // An unpinned tab can be dropped on another unpinned tab.
        mMockViewHolder1.model.set(IS_PINNED, false);
        mMockViewHolder2.model.set(IS_PINNED, false);
        assertTrue(
                mItemTouchHelperCallback.canDropOver(
                        mRecyclerView, mMockViewHolder1, mMockViewHolder2));
    }

    @Test
    public void onDragTab_Hovered_pinnedTab() {
        // Setup: tab1 is pinned, tab2 is not.
        mMockViewHolder1.model.set(IS_PINNED, true);
        mMockViewHolder2.model.set(IS_PINNED, false);

        // Drag pinned card#1 rightwards to hover on unpinned card#2.
        verifyDrag(mMockViewHolder1, 5, 0, POSITION2, AnimationStatus.CARD_RESTORE);

        // Drag unpinned card#2 leftwards to hover on pinned card#1.
        verifyDrag(mMockViewHolder2, -5, 0, POSITION1, AnimationStatus.CARD_RESTORE);

        // Setup: tab1 and tab2 are pinned.
        mMockViewHolder1.model.set(IS_PINNED, true);
        mMockViewHolder2.model.set(IS_PINNED, true);

        // Drag pinned card#1 rightwards to hover on pinned card#2.
        verifyDrag(mMockViewHolder1, 5, 0, POSITION2, AnimationStatus.CARD_RESTORE);
    }

    private void verifyDrag(
            RecyclerView.ViewHolder viewHolder, float dX, float dY, int targetIndex, int status) {
        // Simulate the process of dragging one card to a position.
        mItemTouchHelperCallback.onChildDraw(
                mCanvas,
                mRecyclerView,
                viewHolder,
                dX,
                dY,
                ItemTouchHelper.ACTION_STATE_DRAG,
                true);

        // Verify the card in target index is in correct status.
        assertThat(
                mModel.get(targetIndex).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(status));
    }

    private void initAndAssertAllProperties() {
        for (int i = 0; i < mTabModel.getCount(); i++) {
            Tab tab = mTabModel.getTabAt(i);
            addTabInfoModel(tab);
        }

        assertThat(mModel.size(), equalTo(4));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(3).model.get(TabProperties.TAB_ID), equalTo(TAB4_ID));

        assertThat(
                mModel.get(0).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.CARD_RESTORE));
        assertThat(
                mModel.get(1).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.CARD_RESTORE));
        assertThat(
                mModel.get(2).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.CARD_RESTORE));
        assertThat(
                mModel.get(3).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.CARD_RESTORE));

        assertThat(mModel.get(0).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(mModel.get(1).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(mModel.get(2).model.get(CARD_ALPHA), equalTo(1f));
        assertThat(mModel.get(3).model.get(CARD_ALPHA), equalTo(1f));

        mMockViewHolder1 = prepareMockViewHolder(mModel.get(0).model, mItemView1, POSITION1);
        mMockViewHolder2 = prepareMockViewHolder(mModel.get(1).model, mItemView2, POSITION2);
        mMockViewHolder3 = prepareMockViewHolder(mModel.get(2).model, mItemView3, POSITION3);
        mMockViewHolder4 = prepareMockViewHolder(mModel.get(3).model, mItemView4, POSITION4);
    }

    private void addTabInfoModel(Tab tab) {
        PropertyModel tabInfo =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, null)
                        .with(CardProperties.CARD_ANIMATION_STATUS, AnimationStatus.CARD_RESTORE)
                        .with(CARD_ALPHA, 1f)
                        .with(CARD_TYPE, TAB)
                        .build();
        mModel.add(new MVCListAdapter.ListItem(ModelType.TAB, tabInfo));
    }

    private void addArchivedMessageCard() {
        PropertyModel model =
                new PropertyModel.Builder(ArchivedTabsCardViewProperties.ALL_KEYS)
                        .with(MESSAGE_TYPE, MessageType.ARCHIVED_TABS_MESSAGE)
                        .with(CARD_TYPE, ModelType.MESSAGE)
                        .build();
        mModel.add(new MVCListAdapter.ListItem(ModelType.MESSAGE, model));
        mArchivedMsgItemView = prepareItemView(0, 10, 9, 12);

        doReturn(5).when(mRecyclerView).getChildCount();
        doReturn(5).when(mAdapter).getItemCount();

        mMockArchivedMsgViewHolder =
                spy(
                        new SimpleRecyclerViewAdapter.ViewHolder(
                                mArchivedMsgItemView, /* binder= */ null));
        when(mMockArchivedMsgViewHolder.getItemViewType()).thenReturn(UiType.ARCHIVED_TABS_MESSAGE);
        when(mMockArchivedMsgViewHolder.getAdapterPosition())
                .thenReturn(ARCHIVED_MSG_CARD_POSITION);
        when(mMockArchivedMsgViewHolder.getBindingAdapterPosition())
                .thenReturn(ARCHIVED_MSG_CARD_POSITION);
        mMockArchivedMsgViewHolder.model = model;

        when(mRecyclerView.getChildAt(ARCHIVED_MSG_CARD_POSITION)).thenReturn(mArchivedMsgItemView);
        doReturn(mRecyclerView).when(mArchivedMsgItemView).getParent();
        when(mRecyclerView.findViewHolderForAdapterPosition(ARCHIVED_MSG_CARD_POSITION))
                .thenReturn(mMockArchivedMsgViewHolder);

        assertFalse(mModel.get(ARCHIVED_MSG_CARD_POSITION).model.containsKey(TabProperties.TAB_ID));
        assertThat(
                mModel.get(ARCHIVED_MSG_CARD_POSITION)
                        .model
                        .get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.CARD_RESTORE));
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        doReturn(title).when(tab).getTitle();
        return tab;
    }

    private ViewHolder prepareMockViewHolder(PropertyModel model, View itemView, int position) {
        ViewHolder viewHolder = spy(new ViewHolder(itemView, /* binder= */ null));
        when(viewHolder.getItemViewType()).thenReturn(UiType.TAB);
        when(viewHolder.getAdapterPosition()).thenReturn(position);
        when(viewHolder.getBindingAdapterPosition()).thenReturn(position);
        viewHolder.model = model;
        return viewHolder;
    }

    private View prepareItemView(int left, int top, int right, int bottom) {
        View view = mock(View.class);
        doReturn(left).when(view).getLeft();
        doReturn(top).when(view).getTop();
        doReturn(right).when(view).getRight();
        doReturn(bottom).when(view).getBottom();
        doReturn(right - left).when(view).getWidth();
        doReturn(bottom - top).when(view).getHeight();
        when(view.isAttachedToWindow()).thenReturn(true);
        return view;
    }
}
