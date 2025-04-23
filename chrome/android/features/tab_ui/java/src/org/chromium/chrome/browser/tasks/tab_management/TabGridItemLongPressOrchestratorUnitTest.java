// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewConfiguration;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.CancelLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.RunnableTimer;

/** Unit tests for {@link TabGridItemLongPressOrchestrator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGridItemLongPressOrchestratorUnitTest {
    private static class MockViewHolder extends ViewHolder {
        public MockViewHolder(@NonNull View itemView) {
            super(itemView);
        }
    }

    private static final float LONG_PRESS_DP_CANCEL_THRESHOLD = 8.f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListModel mModel;
    @Mock private OnLongPressTabItemEventListener mOnLongPressListener;
    @Mock private CancelLongPressTabItemEventListener mCancelListener;
    @Mock private RecyclerView mRecyclerView;
    @Mock private View mCardView;
    @Mock private RunnableTimer mTimer;

    private static final long TIMER_DURATION = ViewConfiguration.getLongPressTimeout();
    private static final int TAB_ID = 1;
    private static final int TAB_INDEX = 0;

    private final Supplier<RecyclerView> mRecyclerViewSupplier = () -> mRecyclerView;
    private ViewHolder mViewHolder;
    private TabGridItemLongPressOrchestrator mOrchestrator;

    @Before
    public void setUp() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, TAB_ID)
                        .build();
        MVCListAdapter.ListItem listItem = new MVCListAdapter.ListItem(0, propertyModel);
        when(mModel.get(TAB_INDEX)).thenReturn(listItem);
        when(mModel.size()).thenReturn(1);
        when(mModel.indexFromTabId(TAB_ID)).thenReturn(TAB_INDEX);
        when(mRecyclerView.getChildAt(TAB_INDEX)).thenReturn(mCardView);
        mViewHolder = new MockViewHolder(mCardView);
        when(mRecyclerView.findViewHolderForAdapterPosition(TAB_INDEX)).thenReturn(mViewHolder);
        when(mOnLongPressListener.onLongPressEvent(anyInt(), any())).thenReturn(mCancelListener);

        mOrchestrator =
                new TabGridItemLongPressOrchestrator(
                        mRecyclerViewSupplier,
                        mModel,
                        mOnLongPressListener,
                        LONG_PRESS_DP_CANCEL_THRESHOLD,
                        TIMER_DURATION,
                        mTimer);
    }

    @Test
    public void testOnSelectedChangedDrag() {
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        verify(mTimer).startTimer(eq(TIMER_DURATION), any(Runnable.class));
    }

    @Test
    public void testOnSelectedChangedDrag_InvalidIndex() {
        mOrchestrator.onSelectedChanged(
                TabModel.INVALID_TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);

        verify(mTimer, never()).startTimer(anyLong(), any(Runnable.class));
    }

    @Test
    public void testIdleAction() {
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_IDLE);

        // We cancel before setting the timer and once on idle. So this should be invoked exactly
        // twice.
        verify(mTimer, times(2)).cancelTimer();
    }

    @Test
    public void testIdleAction_ResetsCancelListener() {
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        verify(mTimer).startTimer(anyLong(), any(Runnable.class));
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_IDLE);

        // We cancel before setting the timer and once on idle. So this should be invoked exactly
        // twice.
        verify(mTimer, times(2)).cancelTimer();

        // Verify that the cancel listener has been reset
        mOrchestrator.processChildDisplacement(
                LONG_PRESS_DP_CANCEL_THRESHOLD * LONG_PRESS_DP_CANCEL_THRESHOLD + 1.f);
        verify(mCancelListener, never()).cancelLongPress();
    }

    @Test
    public void testLongPressOnTimerExpiry() {
        enableForceLongPresses();

        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        verify(mTimer).startTimer(eq(TIMER_DURATION), any(Runnable.class));
        verify(mOnLongPressListener).onLongPressEvent(TAB_ID, mCardView);
    }

    @Test
    public void testLongPressOnTimerExpiry_invalidCardIndex() {
        enableForceLongPresses();

        when(mModel.indexFromTabId(TAB_ID)).thenReturn(TabModel.INVALID_TAB_INDEX);
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        verify(mTimer).startTimer(eq(TIMER_DURATION), any(Runnable.class));

        verify(mOnLongPressListener, never()).onLongPressEvent(anyInt(), any());
    }

    @Test
    public void testProcessChildDisplacement_belowThreshold() {
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        verify(mTimer).startTimer(eq(TIMER_DURATION), any(Runnable.class));
        verify(mOnLongPressListener, never()).onLongPressEvent(TAB_ID, mCardView);

        mOrchestrator.processChildDisplacement(
                LONG_PRESS_DP_CANCEL_THRESHOLD * LONG_PRESS_DP_CANCEL_THRESHOLD - 1.f);
        verify(mCancelListener, never()).cancelLongPress();

        // We cancel before setting the timer. So this should be invoked exactly once.
        verify(mTimer).cancelTimer();
    }

    @Test
    public void testProcessChildDisplacement_aboveThreshold() {
        enableForceLongPresses();

        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        verify(mTimer).startTimer(eq(TIMER_DURATION), any(Runnable.class));
        verify(mOnLongPressListener).onLongPressEvent(TAB_ID, mCardView);

        mOrchestrator.processChildDisplacement(
                LONG_PRESS_DP_CANCEL_THRESHOLD * LONG_PRESS_DP_CANCEL_THRESHOLD + 1.f);
        verify(mRecyclerView).findViewHolderForAdapterPosition(TAB_INDEX);
        verify(mCancelListener).cancelLongPress();

        // We cancel before setting the timer and once on idle. So this should be invoked exactly
        // twice.
        verify(mTimer, times(2)).cancelTimer();
    }

    @Test
    public void testProcessChildDisplacement_noListener() {
        mOrchestrator.onSelectedChanged(TAB_INDEX, ItemTouchHelper.ACTION_STATE_DRAG);
        mOrchestrator.processChildDisplacement(100.f);

        // We cancel before setting the timer. So this should be invoked exactly once.
        verify(mTimer).cancelTimer();
    }

    @Test
    public void testCancel() {
        mOrchestrator.cancel();
        verify(mTimer).cancelTimer();
    }

    private void enableForceLongPresses() {
        doAnswer(
                        answer -> {
                            Runnable runnable = answer.getArgument(1);
                            runnable.run();
                            return null;
                        })
                .when(mTimer)
                .startTimer(eq(TIMER_DURATION), any(Runnable.class));
    }
}
