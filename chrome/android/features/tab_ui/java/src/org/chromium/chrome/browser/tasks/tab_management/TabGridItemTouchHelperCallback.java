// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import android.content.Context;
import android.graphics.Canvas;
import android.view.HapticFeedbackConstants;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGridDialogHandler;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/**
 * A {@link ItemTouchHelper.SimpleCallback} implementation to host the logic for swipe and drag
 * related actions in grid related layouts.
 */
public class TabGridItemTouchHelperCallback extends ItemTouchHelper.SimpleCallback {
    /**
     * An interface to observe the longpress event triggered on a tab card item.
     */
    interface OnLongPressTabItemEventListener {
        /**
         * Notify the observers that the longpress event on the tab has triggered.
         * @param tabId the id of the current tab that is being selected.
         */
        void onLongPressEvent(int tabId);
    }

    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final TabListMediator.TabActionListener mTabClosedListener;
    private final String mComponentName;
    private final TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    private final @TabListMode int mMode;
    @Nullable
    private OnLongPressTabItemEventListener mOnLongPressTabItemEventListener;
    private final int mLongPressDpThreshold;
    private float mSwipeToDismissThreshold;
    private float mMergeThreshold;
    private float mUngroupThreshold;
    // A bool to track whether an action such as swiping, group/ungroup and drag past a certain
    // threshold was attempted. This can determine if a longpress on the tab is the objective.
    private boolean mActionAttempted;
    // A bool to track whether any action that is not a pure longpress hold-no-drag, was started.
    // This can determine if an unwanted following click from a pure longpress must be blocked.
    private boolean mActionStarted;
    private boolean mActionsOnAllRelatedTabs;
    private boolean mIsSwipingToDismiss;
    private boolean mShouldBlockAction;
    private int mDragFlags;
    private int mSelectedTabIndex = TabModel.INVALID_TAB_INDEX;
    private int mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
    private int mUnGroupTabIndex = TabModel.INVALID_TAB_INDEX;
    private int mCurrentActionState = ItemTouchHelper.ACTION_STATE_IDLE;
    private RecyclerView mRecyclerView;
    private Profile mProfile;
    private Context mContext;

    public TabGridItemTouchHelperCallback(Context context, TabListModel tabListModel,
            TabModelSelector tabModelSelector, TabActionListener tabClosedListener,
            TabGridDialogHandler tabGridDialogHandler, String componentName,
            boolean actionsOnAllRelatedTabs, @TabListMode int mode) {
        super(0, 0);
        mModel = tabListModel;
        mTabModelSelector = tabModelSelector;
        mTabClosedListener = tabClosedListener;
        mComponentName = componentName;
        mActionsOnAllRelatedTabs = actionsOnAllRelatedTabs;
        mTabGridDialogHandler = tabGridDialogHandler;
        mContext = context;
        mMode = mode;
        mLongPressDpThreshold = context.getResources().getDimensionPixelSize(
                R.dimen.tab_selection_editor_longpress_entry_threshold);
    }

    /**
     * @param listener the handler for longpress actions.
     */
    void setOnLongPressTabItemEventListener(OnLongPressTabItemEventListener listener) {
        mOnLongPressTabItemEventListener = listener;
    }

    /**
     * This method sets up parameters that are used by the {@link ItemTouchHelper} to make decisions
     * about user actions.
     * @param swipeToDismissThreshold          Defines the threshold that user needs to swipe in
     *         order to be considered as a remove operation.
     * @param mergeThreshold                   Defines the threshold of how much two items need to
     *         be overlapped in order to be considered as a merge operation.
     */
    void setupCallback(
            float swipeToDismissThreshold, float mergeThreshold, float ungroupThreshold) {
        mSwipeToDismissThreshold = swipeToDismissThreshold;
        mMergeThreshold = mergeThreshold;
        mUngroupThreshold = ungroupThreshold;
        boolean isMRUEnabledInTabSwitcher =
                TabSwitcherCoordinator.isShowingTabsInMRUOrder(mMode) && mActionsOnAllRelatedTabs;
        // Disable drag for MRU-order tab switcher in start surface.
        // TODO(crbug.com/1005931): Figure out how drag-to-reorder lives in StartSurface MRU
        // ordering scenario.
        boolean isDragEnabled = !isMRUEnabledInTabSwitcher;
        mDragFlags = isDragEnabled ? ItemTouchHelper.START | ItemTouchHelper.END
                        | ItemTouchHelper.UP | ItemTouchHelper.DOWN
                                   : 0;
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        final int dragFlags = viewHolder.getItemViewType() == TabProperties.UiType.MESSAGE
                        || viewHolder.getItemViewType() == TabProperties.UiType.LARGE_MESSAGE
                ? 0
                : mDragFlags;
        final int swipeFlags = ItemTouchHelper.START | ItemTouchHelper.END;
        mRecyclerView = recyclerView;
        return makeMovementFlags(dragFlags, swipeFlags);
    }

    @Override
    public boolean canDropOver(@NonNull RecyclerView recyclerView,
            @NonNull RecyclerView.ViewHolder current, @NonNull RecyclerView.ViewHolder target) {
        if (target.getItemViewType() == TabProperties.UiType.MESSAGE
                || target.getItemViewType() == TabProperties.UiType.LARGE_MESSAGE) {
            return false;
        }
        return super.canDropOver(recyclerView, current, target);
    }

    @Override
    public boolean onMove(RecyclerView recyclerView, RecyclerView.ViewHolder fromViewHolder,
            RecyclerView.ViewHolder toViewHolder) {
        assert !(fromViewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder)
                || hasTabPropertiesModel(fromViewHolder);

        mSelectedTabIndex = toViewHolder.getAdapterPosition();
        if (mHoveredTabIndex != TabModel.INVALID_TAB_INDEX) {
            mModel.updateHoveredTabForMergeToGroup(mHoveredTabIndex, false);
            mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
        }

        int currentTabId = ((SimpleRecyclerViewAdapter.ViewHolder) fromViewHolder)
                                   .model.get(TabProperties.TAB_ID);
        int destinationTabId = ((SimpleRecyclerViewAdapter.ViewHolder) toViewHolder)
                                       .model.get(TabProperties.TAB_ID);
        int distance = toViewHolder.getAdapterPosition() - fromViewHolder.getAdapterPosition();
        TabModelFilter filter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (filter instanceof EmptyTabModelFilter) {
            tabModel.moveTab(currentTabId,
                    mModel.getTabCardCountsBefore(mModel.indexFromId(currentTabId)
                            + (distance > 0 ? distance + 1 : distance)));
        } else if (!mActionsOnAllRelatedTabs) {
            int destinationIndex = tabModel.indexOf(mTabModelSelector.getTabById(destinationTabId));
            tabModel.moveTab(currentTabId, distance > 0 ? destinationIndex + 1 : destinationIndex);
        } else {
            List<Tab> destinationTabGroup = getRelatedTabsForId(destinationTabId);
            int newIndex = distance >= 0
                    ? TabGroupUtils.getLastTabModelIndexForList(tabModel, destinationTabGroup) + 1
                    : TabGroupUtils.getFirstTabModelIndexForList(tabModel, destinationTabGroup);
            ((TabGroupModelFilter) filter).moveRelatedTabs(currentTabId, newIndex);
        }
        RecordUserAction.record("TabGrid.Drag.Reordered." + mComponentName);
        mActionAttempted = true;
        return true;
    }

    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int i) {
        assert viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder;

        SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder =
                (SimpleRecyclerViewAdapter.ViewHolder) viewHolder;

        if (simpleViewHolder.model.get(CARD_TYPE) == TAB) {
            mTabClosedListener.run(simpleViewHolder.model.get(TabProperties.TAB_ID));

            RecordUserAction.record("MobileStackViewSwipeCloseTab." + mComponentName);
        } else if (simpleViewHolder.model.get(CARD_TYPE) == MESSAGE) {
            // TODO(crbug.com/1004570): Have a caller instead of simulating the close click. And
            // write unit test to verify the caller is called.
            viewHolder.itemView.findViewById(R.id.close_button).performClick();
            // TODO(crbug.com/1004570): UserAction swipe to dismiss.
        }
        mActionAttempted = true;
    }

    @Override
    public void onSelectedChanged(RecyclerView.ViewHolder viewHolder, int actionState) {
        super.onSelectedChanged(viewHolder, actionState);
        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            mSelectedTabIndex = viewHolder.getAdapterPosition();
            mModel.updateSelectedTabForMergeToGroup(mSelectedTabIndex, true);
            RecordUserAction.record("TabGrid.Drag.Start." + mComponentName);
        } else if (actionState == ItemTouchHelper.ACTION_STATE_IDLE) {
            mIsSwipingToDismiss = false;
            if (!TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)) {
                mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
            }

            RecyclerView.ViewHolder hoveredViewHolder =
                    mRecyclerView.findViewHolderForAdapterPosition(mHoveredTabIndex);
            boolean shouldUpdate =
                    !(hoveredViewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder)
                    || hasTabPropertiesModel(hoveredViewHolder);

            if (mHoveredTabIndex != TabModel.INVALID_TAB_INDEX && mActionsOnAllRelatedTabs) {
                RecyclerView.ViewHolder selectedViewHolder =
                        mRecyclerView.findViewHolderForAdapterPosition(mSelectedTabIndex);
                if (selectedViewHolder != null && !mRecyclerView.isComputingLayout()
                        && shouldUpdate) {
                    View selectedItemView = selectedViewHolder.itemView;
                    onTabMergeToGroup(mModel.getTabCardCountsBefore(mSelectedTabIndex),
                            mModel.getTabCardCountsBefore(mHoveredTabIndex));
                    mRecyclerView.getLayoutManager().removeView(selectedItemView);
                }
                mActionAttempted = true;
            } else {
                mModel.updateSelectedTabForMergeToGroup(mSelectedTabIndex, false);
            }

            if (mHoveredTabIndex != TabModel.INVALID_TAB_INDEX && shouldUpdate) {
                mModel.updateHoveredTabForMergeToGroup(mSelectedTabIndex > mHoveredTabIndex
                                ? mHoveredTabIndex
                                : mModel.getTabIndexBefore(mHoveredTabIndex),
                        false);
                mActionAttempted = true;
            }
            if (mUnGroupTabIndex != TabModel.INVALID_TAB_INDEX) {
                TabGroupModelFilter filter =
                        (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
                RecyclerView.ViewHolder ungroupViewHolder =
                        mRecyclerView.findViewHolderForAdapterPosition(mUnGroupTabIndex);
                if (ungroupViewHolder != null && !mRecyclerView.isComputingLayout()) {
                    View ungroupItemView = ungroupViewHolder.itemView;
                    filter.moveTabOutOfGroup(
                            mModel.get(mUnGroupTabIndex).model.get(TabProperties.TAB_ID));
                    // Handle the case where the recyclerView is cleared out after ungrouping the
                    // last tab in group.
                    if (mRecyclerView.getAdapter().getItemCount() != 0) {
                        mRecyclerView.getLayoutManager().removeView(ungroupItemView);
                    }
                    RecordUserAction.record("TabGrid.Drag.RemoveFromGroup." + mComponentName);
                }
                mActionAttempted = true;
            }

            // There is a bug with ItemTouchHelper where on longpress, if the held tab is not
            // dragged (no movement occurs), then the gesture will not actually be consumed by the
            // ItemTouchHelper. This manifests as a MOTION_UP event being propagated to child view
            // click handlers and resulting in a real "click" occurring despite the action having
            // technically been consumed as a longpress by this class. The downstream click
            // handlers running can result in a tab being selected or closed in an unexpected manner
            // and due to a race condition between animations a phantom tab can even remain in the
            // UI (see crbug.com/1425336).
            //
            // To avoid this it is necessary for TabListMediator to attach an additional
            // OnItemTouchListener that resolves after the OnItemTouchListener attached by the
            // ItemTouchHelper that TabGridItemTouchHelperCallback is bound to. This additional
            // OnItemTouchListener will block the MOTION_UP event preventing the unintended action
            // from resolving.
            //
            // This block will not trigger if:
            //      a swipe was started but unfinished as mSelectedTabIndex may not be set.
            //      a swipe, move or group/ungroup happens.
            //      a tab is moved beyond a minimum distance from its original location.
            //
            // Otherwise, the unwanted click behaviour will be blocked.
            if (mSelectedTabIndex != TabModel.INVALID_TAB_INDEX && mSelectedTabIndex < mModel.size()
                    && !mActionAttempted
                    && mModel.get(mSelectedTabIndex).model.get(CARD_TYPE) == TAB) {
                // If the child was ever dragged or swiped do not consume the next action, as the
                // longpress will resolve safely due to the listener intercepting the DRAG event
                // and negating any further action. However, if we just release the tab without
                // starting a swipe or drag then it is possible the longpress instead resolves as a
                // MOTION_UP click event leading to the problems described above.
                if (!mActionStarted) {
                    mShouldBlockAction = true;
                }

                if (mOnLongPressTabItemEventListener != null
                        && TabUiFeatureUtilities.isTabSelectionEditorLongPressEntryEnabled()) {
                    int tabId = mModel.get(mSelectedTabIndex).model.get(TabProperties.TAB_ID);
                    mOnLongPressTabItemEventListener.onLongPressEvent(tabId);
                }
            }
            mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
            mSelectedTabIndex = TabModel.INVALID_TAB_INDEX;
            mUnGroupTabIndex = TabModel.INVALID_TAB_INDEX;
            if (mTabGridDialogHandler != null) {
                mTabGridDialogHandler.updateUngroupBarStatus(
                        TabGridDialogView.UngroupBarStatus.HIDE);
            }
        }
        mActionStarted = false;
        mActionAttempted = false;
    }

    private boolean hasTabPropertiesModel(RecyclerView.ViewHolder viewHolder) {
        return viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder
                && ((SimpleRecyclerViewAdapter.ViewHolder) viewHolder).model.get(CARD_TYPE) == TAB;
    }

    @Override
    public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        super.clearView(recyclerView, viewHolder);
        int prevActionState = mCurrentActionState;
        mCurrentActionState = ItemTouchHelper.ACTION_STATE_IDLE;
        if (prevActionState != ItemTouchHelper.ACTION_STATE_DRAG) return;
        // If this item view becomes stale after the dragging animation is finished, manually clean
        // it out. TODO(yuezhanggg): Figure out why the deleting signal is not properly sent when
        // item is being dragged (crbug: 995799).
        if (recyclerView.getAdapter().getItemCount() == 0 && recyclerView.getChildCount() != 0) {
            recyclerView.getLayoutManager().removeView(viewHolder.itemView);
        }
    }

    @Override
    public void onChildDraw(Canvas c, RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder,
            float dX, float dY, int actionState, boolean isCurrentlyActive) {
        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);
        if (Math.abs(dX) > 0 || Math.abs(dY) > 0) {
            mActionStarted = true;
        }
        if (actionState == ItemTouchHelper.ACTION_STATE_SWIPE) {
            float alpha = Math.max(0.2f, 1f - 0.8f * Math.abs(dX) / mSwipeToDismissThreshold);

            assert viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder;

            int index = TabModel.INVALID_TAB_INDEX;
            SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder =
                    (SimpleRecyclerViewAdapter.ViewHolder) viewHolder;

            if (simpleViewHolder.model.get(CARD_TYPE) == TAB) {
                index = mModel.indexFromId(simpleViewHolder.model.get(TabProperties.TAB_ID));
            } else if (simpleViewHolder.model.get(CARD_TYPE) == MESSAGE) {
                index = mModel.lastIndexForMessageItemFromType(
                        simpleViewHolder.model.get(MessageCardViewProperties.MESSAGE_TYPE));
            }

            if (index == TabModel.INVALID_TAB_INDEX) return;

            mModel.get(index).model.set(TabListModel.CardProperties.CARD_ALPHA, alpha);
            boolean isOverThreshold = Math.abs(dX) >= mSwipeToDismissThreshold;
            if (isOverThreshold && !mIsSwipingToDismiss) {
                viewHolder.itemView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
            }
            mIsSwipingToDismiss = isOverThreshold;
            return;
        }
        if (dX * dX + dY * dY > mLongPressDpThreshold * mLongPressDpThreshold) {
            mActionAttempted = true;
        }
        mCurrentActionState = actionState;
        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG && mActionsOnAllRelatedTabs) {
            if (!TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)) return;
            int prev_hovered = mHoveredTabIndex;
            mHoveredTabIndex = TabListRecyclerView.getHoveredTabIndex(
                    recyclerView, viewHolder.itemView, dX, dY, mMergeThreshold);

            RecyclerView.ViewHolder hoveredViewHolder =
                    mRecyclerView.findViewHolderForAdapterPosition(mHoveredTabIndex);

            if (hoveredViewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder
                    && !hasTabPropertiesModel(hoveredViewHolder)) {
                mHoveredTabIndex = TabModel.INVALID_TAB_INDEX;
            } else {
                mModel.updateHoveredTabForMergeToGroup(mHoveredTabIndex, true);
                if (prev_hovered != mHoveredTabIndex) {
                    mModel.updateHoveredTabForMergeToGroup(prev_hovered, false);
                }
            }
        } else if (actionState == ItemTouchHelper.ACTION_STATE_DRAG
                && mTabGridDialogHandler != null) {
            boolean isHoveredOnUngroupBar = viewHolder.itemView.getBottom() + dY
                    > recyclerView.getBottom() - mUngroupThreshold;
            if (mSelectedTabIndex == TabModel.INVALID_TAB_INDEX) return;
            mUnGroupTabIndex = isHoveredOnUngroupBar ? viewHolder.getAdapterPosition()
                                                     : TabModel.INVALID_TAB_INDEX;
            mTabGridDialogHandler.updateUngroupBarStatus(isHoveredOnUngroupBar
                            ? TabGridDialogView.UngroupBarStatus.HOVERED
                            : (mSelectedTabIndex == TabModel.INVALID_TAB_INDEX
                                            ? TabGridDialogView.UngroupBarStatus.HIDE
                                            : TabGridDialogView.UngroupBarStatus.SHOW));
        }
    }

    @Override
    public float getSwipeThreshold(RecyclerView.ViewHolder viewHolder) {
        return mSwipeToDismissThreshold / mRecyclerView.getWidth();
    }

    private List<Tab> getRelatedTabsForId(int id) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(id);
    }

    private void onTabMergeToGroup(int selectedCardIndex, int hoveredCardIndex) {
        TabGroupModelFilter filter =
                (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();
        if (filter.getTabAt(selectedCardIndex) == null) return;
        if (filter.getTabAt(hoveredCardIndex) == null) return;
        filter.mergeTabsToGroup(filter.getTabAt(selectedCardIndex).getId(),
                filter.getTabAt(hoveredCardIndex).getId());

        // If user has used drop-to-merge, send a signal to disable
        // FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE.
        final Tracker tracker = TrackerFactory.getTrackerForProfile(
                mTabModelSelector.getCurrentModel().getProfile());
        tracker.notifyEvent(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP);
    }

    /*
     * Returns whether or not a touch action should be blocked on an item accessed from
     * the TabListCoordinator. The bit is always defaulted to false and reset to that
     * value after shouldBlockAction() is called. It is used primarily to prevent a
     * secondary touch event from occurring on a longpress event on a tab grid item.
     */
    boolean shouldBlockAction() {
        boolean out = mShouldBlockAction;
        mShouldBlockAction = false;
        return out;
    }

    @VisibleForTesting
    void setActionsOnAllRelatedTabsForTesting(boolean flag) {
        mActionsOnAllRelatedTabs = flag;
    }

    @VisibleForTesting
    void setHoveredTabIndexForTesting(int index) {
        mHoveredTabIndex = index;
    }

    @VisibleForTesting
    void setSelectedTabIndexForTesting(int index) {
        mSelectedTabIndex = index;
    }

    @VisibleForTesting
    void setUnGroupTabIndexForTesting(int index) {
        mUnGroupTabIndex = index;
    }

    @VisibleForTesting
    void setCurrentActionStateForTesting(int actionState) {
        mCurrentActionState = actionState;
    }

    @VisibleForTesting
    boolean hasDragFlagForTesting(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        int flags = getMovementFlags(recyclerView, viewHolder);
        return (flags >> 16) != 0;
    }

    @VisibleForTesting
    boolean hasSwipeFlag(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        int flags = getMovementFlags(recyclerView, viewHolder);
        return ((flags >> 8) & 0xFF) != 0;
    }
}
