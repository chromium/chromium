// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import android.content.Context;
import android.graphics.Canvas;
import android.view.View;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.util.function.Supplier;

/** Unit tests for {@link PinnedTabStripItemTouchHelperCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final
        })
public class PinnedTabStripItemTouchHelperCallbackTest {
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int TAB_ID1 = 10;
    private static final int TAB_ID2 = 20;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static class TestViewHolder extends RecyclerView.ViewHolder {
        TestViewHolder(View itemView) {
            super(itemView);
        }
    }

    @Mock private TabListModel mTabListModel;
    @Mock private Supplier<RecyclerView> mRecyclerViewSupplier;
    @Mock private RecyclerView mRecyclerView;
    @Mock private OnLongPressTabItemEventListener mOnLongPressListener;
    @Mock private Canvas mCanvas;
    @Mock private ObservableSupplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private View mItemView1;
    @Mock private View mItemView2;
    private ViewHolder mMockViewHolder1;
    private ViewHolder mMockViewHolder2;
    private RecyclerView.ViewHolder mViewHolder;
    private PinnedTabStripItemTouchHelperCallback mCallback;

    @Before
    public void setUp() throws Exception {
        Context context = ApplicationProvider.getApplicationContext();
        mViewHolder = spy(new TestViewHolder(new View(context)));

        when(mRecyclerViewSupplier.get()).thenReturn(mRecyclerView);
        when(mTabGroupModelFilterSupplier.get()).thenReturn(mTabGroupModelFilter);
        mMockViewHolder1 = prepareMockViewHolder(TAB_ID1, mItemView1, POSITION1);
        mMockViewHolder2 = prepareMockViewHolder(TAB_ID2, mItemView2, POSITION2);

        mCallback =
                new PinnedTabStripItemTouchHelperCallback(
                        context,
                        mTabGroupModelFilterSupplier,
                        mTabListModel,
                        mRecyclerViewSupplier,
                        mOnLongPressListener);
    }

    @Test
    public void testOnMove() {
        mCallback.onMove(null, mMockViewHolder1, mMockViewHolder2);

        verify(mTabGroupModelFilter).moveRelatedTabs(TAB_ID1, POSITION2);
        verify(mTabListModel).move(POSITION1, POSITION2);
    }

    @Test
    public void testGetMovementFlags() {
        int flags = mCallback.getMovementFlags(mRecyclerView, mViewHolder);
        assertEquals(
                ItemTouchHelper.Callback.makeMovementFlags(
                        ItemTouchHelper.START | ItemTouchHelper.END, 0),
                flags);
    }

    @Test
    public void testOnSwiped() {
        // Should not crash.
        mCallback.onSwiped(mViewHolder, 0);
    }

    @Test
    public void testLongPress_NoOpWithNoAction() {
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_IDLE);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mOnLongPressListener, never()).onLongPressEvent(anyInt(), any());
    }

    @Test
    public void testOnSelectedChanged_ActionStateDrag() {
        mCallback.onSelectedChanged(mMockViewHolder1, ItemTouchHelper.ACTION_STATE_DRAG);
        verify(mTabListModel).updateSelectedCardForSelection(POSITION1, true);
    }

    @Test
    public void testLongPress_CancelledByClearView() {
        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
        mCallback.clearView(mRecyclerView, mViewHolder);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mOnLongPressListener, never()).onLongPressEvent(anyInt(), any());
    }

    @Test
    public void testOnSelectedChanged_ActionStateIdle() {
        mCallback.onSelectedChanged(mMockViewHolder1, ItemTouchHelper.ACTION_STATE_DRAG);
        mCallback.onSelectedChanged(mMockViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);
        verify(mTabListModel).updateSelectedCardForSelection(POSITION1, false);
    }

    @Test
    public void testLongPress_CancelledByDisplacement() {
        // Using a hardcoded value to avoid resource dependency.
        float threshold = 20f;

        mCallback.onSelectedChanged(mViewHolder, ItemTouchHelper.ACTION_STATE_DRAG);
        // dX=threshold, dY=1, dX*dX+dY*dY > threshold*threshold
        mCallback.onChildDraw(mCanvas, mRecyclerView, mViewHolder, threshold, 1f, 0, true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mOnLongPressListener, never()).onLongPressEvent(anyInt(), any());
    }

    private ViewHolder prepareMockViewHolder(int tabId, View itemView, int position) {
        ViewHolder viewHolder = spy(new ViewHolder(itemView, /* binder= */ null));
        when(viewHolder.getItemViewType()).thenReturn(TabProperties.UiType.TAB);
        when(viewHolder.getBindingAdapterPosition()).thenReturn(position);
        viewHolder.model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, tabId)
                        .with(CARD_TYPE, TAB)
                        .build();
        return viewHolder;
    }
}
