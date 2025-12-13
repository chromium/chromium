// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.util.RunnableTimer;

import java.util.function.Supplier;

/**
 * Orchestrates the long press event on items within a {@link RecyclerView} to differentiate between
 * a regular long press drag and a long press that intends to trigger a different action (e.g.,
 * opening a context menu).
 */
@NullMarked
public class TabGridItemLongPressOrchestrator {
    /** An interface to observe the long press event triggered on a tab card item. */
    @FunctionalInterface
    public interface OnLongPressTabItemEventListener {
        /**
         * Notify the observers that the long press event on the tab has triggered.
         *
         * @param tabId The id of the current tab that is being selected.
         * @param cardView The view representing the tab card.
         */
        @Nullable CancelLongPressTabItemEventListener onLongPressEvent(
                @TabId int tabId, @Nullable View cardView);
    }

    /**
     * An interface to observe the long press cancel event triggered on a tab card item. This occurs
     * when the tab card is dragged after already being long pressed.
     */
    @FunctionalInterface
    public interface CancelLongPressTabItemEventListener {
        /** Notify the observers that the long press event on the tab has been cancelled. */
        void cancelLongPress();
    }

    private final RunnableTimer mTimer;
    private final TabListModel mModel;
    private final Supplier<RecyclerView> mRecyclerViewSupplier;
    private final OnLongPressTabItemEventListener mOnLongPressTabItemEventListener;
    private final float mLongPressDpCancelThresholdSquared;
    private final long mTimerDuration;
    private @Nullable CancelLongPressTabItemEventListener mCancelLongPressTabItemEventListener;

    /**
     * @param recyclerViewSupplier Supplies the {@link RecyclerView} whose items are being observed
     *     for long presses.
     * @param model The model representing the data in the RecyclerView.
     * @param onLongPress The listener to be notified when a long press is detected.
     * @param longPressDpCancelThreshold The distance (in dp) the item can be dragged before a
     *     scheduled long press is cancelled.
     * @param timerDuration The time taken for the timer to expire (in milliseconds).
     */
    public TabGridItemLongPressOrchestrator(
            Supplier<RecyclerView> recyclerViewSupplier,
            TabListModel model,
            OnLongPressTabItemEventListener onLongPress,
            float longPressDpCancelThreshold,
            long timerDuration) {
        this(
                recyclerViewSupplier,
                model,
                onLongPress,
                longPressDpCancelThreshold,
                timerDuration,
                new RunnableTimer());
    }

    @VisibleForTesting
    TabGridItemLongPressOrchestrator(
            Supplier<RecyclerView> recyclerViewSupplier,
            TabListModel model,
            OnLongPressTabItemEventListener onLongPress,
            float longPressDpCancelThreshold,
            long timerDuration,
            RunnableTimer timer) {
        mRecyclerViewSupplier = recyclerViewSupplier;
        mModel = model;
        mOnLongPressTabItemEventListener = onLongPress;
        mLongPressDpCancelThresholdSquared =
                longPressDpCancelThreshold * longPressDpCancelThreshold;
        mTimerDuration = timerDuration;
        mTimer = timer;
    }

    /** Cancels any pending long presses. */
    public void cancel() {
        mTimer.cancelTimer();
    }

    /**
     * Called when the selection state of an item in the tab grid changes. Should be called towards
     * the start of {@link ItemTouchHelper.Callback#onSelectedChanged(ViewHolder, int)}.
     *
     * @param selectedTabIndex The index of the selected tab card.
     * @param actionState One of {@link ItemTouchHelper#ACTION_STATE_IDLE}, {@link
     *     ItemTouchHelper#ACTION_STATE_DRAG} or {@link ItemTouchHelper#ACTION_STATE_SWIPE}.
     */
    public void onSelectedChanged(int selectedTabIndex, int actionState) {
        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            if (selectedTabIndex != TabModel.INVALID_TAB_INDEX
                    && selectedTabIndex < mModel.size()) {
                onLongPress(selectedTabIndex);
            }
        } else if (actionState == ItemTouchHelper.ACTION_STATE_IDLE) {
            mCancelLongPressTabItemEventListener = null;
            mTimer.cancelTimer();
        }
    }

    /**
     * Processes the displacement of a child view during a drag operation. If a long press was
     * pending and the displacement exceeds a threshold, the scheduled long press is cancelled.
     *
     * @param dpMagnitudeSquared The squared magnitude of the displacement from the initial touch
     *     point in dp squared.
     */
    public void processChildDisplacement(float dpMagnitudeSquared) {
        boolean overThreshold = dpMagnitudeSquared > mLongPressDpCancelThresholdSquared;
        if (overThreshold && mCancelLongPressTabItemEventListener != null) {
            mCancelLongPressTabItemEventListener.cancelLongPress();
            mTimer.cancelTimer();
            mCancelLongPressTabItemEventListener = null;
        }
    }

    /**
     * Starts a timer to detect a long press on a tab item.
     *
     * @param selectedTabIndex The index of the tab item in the {@link TabListModel} that is being
     *     long pressed.
     */
    private void onLongPress(int selectedTabIndex) {
        @Nullable RecyclerView recyclerView = mRecyclerViewSupplier.get();
        if (recyclerView == null) {
            return;
        }

        MVCListAdapter.ListItem listItem = mModel.get(selectedTabIndex);
        @TabId int tabId = listItem.model.get(TabProperties.TAB_ID);

        mTimer.cancelTimer();
        mTimer.startTimer(
                mTimerDuration,
                () -> {
                    int cardIdx = mModel.indexFromTabId(tabId);
                    if (cardIdx == TabModel.INVALID_TAB_INDEX) {
                        return;
                    }
                    ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(cardIdx);
                    if (viewHolder == null) return;
                    View card = viewHolder.itemView;
                    mCancelLongPressTabItemEventListener =
                            mOnLongPressTabItemEventListener.onLongPressEvent(tabId, card);
                });
    }
}
