// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.SystemClock;
import android.text.format.DateUtils;
import android.util.SparseLongArray;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.NestedTabReorderUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator;
import org.chromium.chrome.browser.tasks.tab_management.TabListItemTouchHelperCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabMultiSelectHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

/**
 * A {@link TabListItemTouchHelperCallback} implementation to host the logic for swipe and drag
 * related actions in vertical tab list layout.
 */
@NullMarked
public class VerticalTabListItemTouchHelperCallback extends TabListItemTouchHelperCallback {
    // LINT.IfChange(AndroidVerticalTabsDragDropResult)
    @IntDef({
        DragDropResult.REORDERED,
        DragDropResult.GROUPED,
        DragDropResult.UNGROUPED,
        DragDropResult.ABORTED_NO_CHANGE,
        DragDropResult.DRAGGED_OUT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DragDropResult {
        int REORDERED = 0;
        int GROUPED = 1;
        int UNGROUPED = 2;
        int ABORTED_NO_CHANGE = 3;
        int DRAGGED_OUT = 4;
        int COUNT = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidVerticalTabsDragDropResult)

    private static final long CONTEXT_MENU_ORCHESTRATOR_DELAY_MS = 10L;
    private static final long MAX_UNGROUP_TRACKING_DURATION_MS = 3 * DateUtils.MINUTE_IN_MILLIS;
    private final int mMouseDragThresholdSquared;
    private final Set<Integer> mDraggedChildTabIds = new HashSet<>();
    private final List<Integer> mSelectedGroupTabIds = new ArrayList<>();
    private final List<RecyclerView.ViewHolder> mDraggedChildViewHolders = new ArrayList<>();
    private final SparseLongArray mGroupedTabTimestamps = new SparseLongArray();
    private final @Nullable UndoBarThrottle mUndoBarThrottle;
    // State snapshot captured at drag start to diff against the final state on drop.
    private int mDragStartTabId = Tab.INVALID_TAB_ID;
    private int mDragStartTabModelIndex = TabModel.INVALID_TAB_INDEX;
    private @Nullable Token mDragStartGroupId;
    private boolean mIsOSNewWindowDrop;
    private RecyclerView.@Nullable ViewHolder mSelectedViewHolder;
    private @Nullable OnDragOutListener mOnDragOutListener;
    private @Nullable Runnable mOnDragStartCallback;
    private boolean mHasFiredDragMovementCallback;
    private int mUndoBarThrottleToken = TokenHolder.INVALID_TOKEN;
    private float mDragStartX;

    public void setDragStartX(float x) {
        mDragStartX = x;
    }

    public static RecyclerView.OnItemTouchListener createBeforeOnItemTouchListener(
            VerticalTabListItemTouchHelperCallback callback) {
        return new RecyclerView.OnItemTouchListener() {
            @Override
            public boolean onInterceptTouchEvent(RecyclerView recyclerView, MotionEvent event) {
                callback.setIsMouseInputSource(
                        event.getSource() == android.view.InputDevice.SOURCE_MOUSE);
                int action = event.getActionMasked();
                if (action == MotionEvent.ACTION_DOWN) {
                    callback.setDragStartX(event.getX());
                } else if (action == MotionEvent.ACTION_UP) {
                    callback.stopThrottling();
                }
                return false;
            }

            @Override
            public void onTouchEvent(RecyclerView recyclerView, MotionEvent event) {}

            @Override
            public void onRequestDisallowInterceptTouchEvent(boolean disallowIntercept) {}
        };
    }

    /** Listener for when a dragged tab exits the horizontal boundaries of the tab strip. */
    public interface OnDragOutListener {
        void onDragOut(RecyclerView.ViewHolder viewHolder, float dX, float dY);
    }

    /** Sets the listener for outward drag events. */
    public void setOnDragOutListener(OnDragOutListener listener) {
        mOnDragOutListener = listener;
    }

    /** Sets the listener for when drag reordering starts. */
    public void setOnDragStartCallback(@Nullable Runnable callback) {
        mOnDragStartCallback = callback;
    }

    /**
     * @param context The Android context.
     * @param model The {@link TabListModel} for the tab list.
     * @param currentTabModelSupplier Supplier for the current {@link TabModel}.
     * @param undoBarThrottle Throttle to pause undo snackbars during active drag operations.
     */
    public VerticalTabListItemTouchHelperCallback(
            Context context,
            TabListModel model,
            Supplier<TabModel> currentTabModelSupplier,
            @Nullable UndoBarThrottle undoBarThrottle) {
        super(context, model, currentTabModelSupplier);
        mUndoBarThrottle = undoBarThrottle;
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop() / 4;
        mMouseDragThresholdSquared = touchSlop * touchSlop;
    }

    @Override
    public void setRecyclerView(RecyclerView recyclerView) {
        RecyclerView oldRecyclerView = mRecyclerViewSupplier.get();
        if (oldRecyclerView == recyclerView) {
            return;
        }
        if (oldRecyclerView != null) {
            oldRecyclerView.removeOnChildAttachStateChangeListener(mChildAttachListener);
        }
        super.setRecyclerView(recyclerView);
        if (recyclerView != null) {
            recyclerView.addOnChildAttachStateChangeListener(mChildAttachListener);
        }
    }

    /**
     * Returns the movement flags for the given view holder. Regular and child tabs can move
     * vertically, while pinned tabs can move horizontally as well.
     */
    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        if (!hasTabPropertiesModel(viewHolder)) return 0;
        // Pinned tab hidden placeholders in the main vertical list (LinearLayoutManager) are
        // non-draggable.
        // Pinned tab cards in the top strip (GridLayoutManager) remain draggable in 2D.
        if (viewHolder.getItemViewType() == TabProperties.UiType.PINNED_TAB
                && !(recyclerView.getLayoutManager() instanceof GridLayoutManager)) {
            return 0;
        }

        // All tabs visually move vertically unless pinned, but we universally enable
        // horizontal flags so that ItemTouchHelper provides us with horizontal cursor tracking.
        // The visual horizontal movement for non-pinned tabs is suppressed below in onChildDraw().
        int dragFlags =
                ItemTouchHelper.UP
                        | ItemTouchHelper.DOWN
                        | ItemTouchHelper.LEFT
                        | ItemTouchHelper.RIGHT;

        return makeMovementFlags(dragFlags, 0);
    }

    @Override
    public void getBoundingBox(RecyclerView.ViewHolder viewHolder, Rect outRect) {
        super.getBoundingBox(viewHolder, outRect);

        // When dragging a group, we want the layout swap to happen when the dragged group crosses
        // the midway point of the entire target group, rather than just crossing the midway point
        // of the target group's header. To achieve this, we expand the bounds of all target group
        // headers to encompass their respective children when a group is being dragged.
        boolean isDraggingGroup =
                mSelectedViewHolder != null
                        && (mSelectedViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP
                                || isSolitaryChild(mSelectedViewHolder));

        if (isDraggingGroup
                && (viewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP
                        || isSolitaryChild(viewHolder))) {
            Token groupId = getTabGroupId(viewHolder);
            if (groupId == null) return;

            RecyclerView recyclerView = mRecyclerViewSupplier.get();
            if (recyclerView == null) return;

            int minTop = outRect.top;
            int maxBottom = outRect.bottom;
            int minLeft = outRect.left;
            int maxRight = outRect.right;

            for (int i = 0; i < recyclerView.getChildCount(); i++) {
                View childView = recyclerView.getChildAt(i);
                RecyclerView.ViewHolder childViewHolder =
                        recyclerView.getChildViewHolder(childView);

                if (childViewHolder == viewHolder) continue;

                if (hasTabPropertiesModel(childViewHolder)) {
                    Token childGroupId = getTabGroupId(childViewHolder);
                    if (groupId.equals(childGroupId)) {
                        minTop = Math.min(minTop, childView.getTop());
                        maxBottom = Math.max(maxBottom, childView.getBottom());
                        minLeft = Math.min(minLeft, childView.getLeft());
                        maxRight = Math.max(maxRight, childView.getRight());
                    }
                }
            }

            outRect.set(minLeft, minTop, maxRight, maxBottom);
        }
    }

    /**
     * Checks whether a dragged tab can be dropped over a target tab. Prevents drops across pinned
     * and unpinned boundaries.
     */
    @Override
    public boolean canDropOver(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder current,
            RecyclerView.ViewHolder target) {
        if (!hasTabPropertiesModel(target)) {
            return false;
        }

        // A pinned tab cannot be dropped in the unpinned section and vice versa.
        if (isPinnedRegularTab(current) != isPinnedRegularTab(target)) {
            return false;
        }

        boolean isCurrentGroupHeader = current.getItemViewType() == TabProperties.UiType.TAB_GROUP;
        if (isCurrentGroupHeader || isSolitaryChild(current)) {
            // Prevent dropping a group over a child tab within any group to ensure the entire
            // dragged group moves as a single atomic unit and avoids janky intermediate states.
            Token targetGroupId = getTabGroupId(target);
            boolean isTargetGroupChild =
                    target.getItemViewType() == TabProperties.UiType.TAB && targetGroupId != null;
            if (isTargetGroupChild) {
                return false;
            }

            // Allow a group header to float past its own children until it hits a valid
            // target, at which point the entire group (header + children) moves together.
            Token currentGroupId = getTabGroupId(current);
            if (currentGroupId != null && currentGroupId.equals(targetGroupId)) {
                return false;
            }
        }

        return super.canDropOver(recyclerView, current, target);
    }

    @Override
    public RecyclerView.@Nullable ViewHolder chooseDropTarget(
            RecyclerView.ViewHolder selected,
            List<RecyclerView.ViewHolder> dropTargets,
            int curX,
            int curY) {
        if (isDraggedItemCollapsed()) {
            return null;
        }
        Rect selectedBounds = new Rect();
        getBoundingBox(selected, selectedBounds);
        if (selectedBounds.height() <= 0 || selectedBounds.width() <= 0) {
            return null;
        }
        // The base implementation requires the dragged item's bounding box center to pass the
        // target item's bounding box edge before a swap is triggered. However, when dragging large
        // items (like an expanded tab group), this logic makes it difficult to reorder because the
        // user has to drag the group very far to move its center past the next item.
        //
        // This custom implementation instead requires only that the dragged item's leading edge
        // passes the target item's bounding box center.
        RecyclerView.ViewHolder winner = null;
        int winnerScore = -1;
        final int dx = curX - selected.itemView.getLeft();
        final int dy = curY - selected.itemView.getTop();
        int right = selectedBounds.right + dx;
        int bottom = selectedBounds.bottom + dy;
        int left = selectedBounds.left + dx;
        int top = selectedBounds.top + dy;
        final int targetsSize = dropTargets.size();
        Rect targetBounds = new Rect();

        boolean isSelectedStandalone =
                selected.getItemViewType() == TabProperties.UiType.TAB
                        && getTabGroupId(selected) == null;

        for (int i = 0; i < targetsSize; i++) {
            final RecyclerView.ViewHolder target = dropTargets.get(i);
            getBoundingBox(target, targetBounds);
            if (dx > 0) {
                int diff = targetBounds.right - right;
                if (diff < 0 && targetBounds.right > selectedBounds.right) {
                    final int score = Math.abs(diff);
                    if (score > winnerScore) {
                        winnerScore = score;
                        winner = target;
                    }
                }
            }
            if (dx < 0) {
                int diff = targetBounds.left - left;
                if (diff > 0 && targetBounds.left < selectedBounds.left) {
                    final int score = Math.abs(diff);
                    if (score > winnerScore) {
                        winnerScore = score;
                        winner = target;
                    }
                }
            }

            // For vertical drag, trigger swap when the dragged item's leading edge passes the
            // target's center.
            int targetCenterY = targetBounds.top + targetBounds.height() / 2;

            if (dy < 0) {
                // If a standalone tab is dragged upward into the lowest tab of a group,
                // trigger grouping.
                if (isSelectedStandalone) {
                    boolean isTargetChildTab =
                            target.getItemViewType() == TabProperties.UiType.TAB
                                    && getTabGroupId(target) != null;

                    if (isTargetChildTab) {
                        int targetTabId = getTabId(target);
                        List<Tab> relatedTabs = getRelatedTabsForId(targetTabId);
                        boolean isTargetLowestTab =
                                relatedTabs != null
                                        && !relatedTabs.isEmpty()
                                        && relatedTabs.get(relatedTabs.size() - 1).getId()
                                                == targetTabId;

                        if (isTargetLowestTab) {
                            targetCenterY = targetBounds.bottom - targetBounds.height() / 4;
                        }
                    }
                }

                if (top < targetCenterY && targetBounds.top < selectedBounds.top) {
                    final int score = Math.abs(targetBounds.top - top);
                    if (score > winnerScore) {
                        winnerScore = score;
                        winner = target;
                    }
                }
            }

            if (dy > 0) {
                if (bottom > targetCenterY && targetBounds.bottom > selectedBounds.bottom) {
                    final int score = Math.abs(targetBounds.bottom - bottom);
                    if (score > winnerScore) {
                        winnerScore = score;
                        winner = target;
                    }
                }
            }
        }
        return winner;
    }

    @Override
    protected boolean shouldBlockOutOfBoundsScroll() {
        return false;
    }

    @Override
    protected boolean shouldBlockOnMoved() {
        return false;
    }

    /**
     * Called when a tab is moved. Updates the underlying {@link TabModel} to reflect the
     * reordering.
     */
    @Override
    public boolean onMove(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder fromViewHolder,
            RecyclerView.ViewHolder toViewHolder) {
        if (!hasTabPropertiesModel(fromViewHolder) || !hasTabPropertiesModel(toViewHolder)) {
            return false;
        }

        int currentTabId = getTabId(fromViewHolder);
        int destinationTabId = getTabId(toViewHolder);

        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return false;

        boolean isGroupHeader = fromViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP;
        Token currentGroupId = getTabGroupId(fromViewHolder);
        boolean isStandaloneTab = !isGroupHeader && currentGroupId == null;
        boolean isSolitaryChild = !isGroupHeader && isSolitaryChild(fromViewHolder);
        boolean isGroup = isGroupHeader || isSolitaryChild;

        Token destGroupId = getTabGroupId(toViewHolder);
        boolean isDestGroupHeader =
                toViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP;

        int distance =
                toViewHolder.getBindingAdapterPosition()
                        - fromViewHolder.getBindingAdapterPosition();

        if (!isStandaloneTab && !isGroup) {
            // This is a non-solitary child tab.
            if (NestedTabReorderUtils.tryUngroupChildTab(
                    tabModel,
                    currentTabId,
                    currentGroupId,
                    destGroupId,
                    isDestGroupHeader,
                    distance)) {
                return true;
            }
        }

        if (isStandaloneTab) {
            // Intercept swaps between a standalone tab and a tab group.
            PropertyModel destModel = ((ViewHolder) toViewHolder).model;
            if (NestedTabReorderUtils.tryMergeStandaloneTab(
                    tabModel,
                    currentTabId,
                    destinationTabId,
                    destGroupId,
                    isDestGroupHeader,
                    destModel,
                    distance)) {
                return true;
            }
        }

        int destinationIndex =
                NestedTabReorderUtils.calculateDestinationIndex(
                        tabModel,
                        currentTabId,
                        destinationTabId,
                        isGroup,
                        isStandaloneTab,
                        destGroupId,
                        distance);

        if (destinationIndex == TabModel.INVALID_TAB_INDEX) return false;

        // Track the current UI position to correctly clean up visual selection on drop
        mSelectedTabIndex = toViewHolder.getBindingAdapterPosition();

        // Perform basic list reordering by updating the TabModel immediately.
        NestedTabReorderUtils.moveTabOrGroup(tabModel, currentTabId, destinationIndex, isGroup);
        return true;
    }

    /** Called when a tab is swiped. Swiping is not supported for vertical tabs. */
    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int direction) {
        // Empty/default
    }

    /**
     * Returns whether long press to drag is enabled. Disabled for mouse input to allow instant
     * dragging.
     */
    @Override
    public boolean isLongPressDragEnabled() {
        return !mIsMouseInputSource;
    }

    @Override
    public boolean isDragSweepingEnabled() {
        // Enable drag sweeping for Vertical Tabs to ensure fast mouse drags
        // don't skip over swap targets.
        return true;
    }

    /**
     * Called when the selected state of a tab changes, such as when dragging starts or stops.
     * Updates the visual state and tab model selection.
     */
    @Override
    public void onSelectedChanged(RecyclerView.@Nullable ViewHolder viewHolder, int actionState) {
        super.onSelectedChanged(viewHolder, actionState);

        if (mTabGridItemLongPressOrchestrator != null
                && viewHolder != null
                && !mIsMouseInputSource) {
            int position = viewHolder.getBindingAdapterPosition();
            mTabGridItemLongPressOrchestrator.onSelectedChanged(position, actionState);
        }

        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            mHasFiredDragMovementCallback = false;
            TabModel tabModel = mCurrentTabModelSupplier.get();
            if (tabModel == null || !hasTabPropertiesModel(viewHolder)) return;

            // TODO(crbug.com/544185227): Support batch drag and drop of multi-selected tabs.
            // Currently, we fallback to a standard single-tab drag by clearing
            // the multi-selection state if the user drags a highlighted item.
            if (TabMultiSelectHelper.hasMultipleTabsSelected(tabModel)) {
                tabModel.clearMultiSelection(/* notifyObservers= */ true);
            }

            // Pause undo snackbars while dragging.
            startThrottling();
            mIsOSNewWindowDrop = false;
            mSelectedViewHolder = viewHolder;
            mSelectedGroupTabIds.clear();

            // Capture initial snapshot to evaluate the final drag result upon release.
            assumeNonNull(viewHolder);
            mDragStartTabId = getTabId(viewHolder);
            mDragStartGroupId = getTabGroupId(viewHolder);
            Tab startTab = tabModel.getTabById(mDragStartTabId);
            mDragStartTabModelIndex =
                    startTab != null ? tabModel.indexOf(startTab) : TabModel.INVALID_TAB_INDEX;
            mSelectedTabIndex = viewHolder.getBindingAdapterPosition();
            mModel.updateSelectedCardForSelection(mSelectedTabIndex, true);

            if (viewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP) {
                // Select the group header, which ensures a tab within the group is active.
                selectTabForGroup(viewHolder);

                int currentSelectedTabId = getCurrentSelectedTabId();

                // Give inactive child tabs a selected background so the entire group looks
                // highlighted while dragging. Skip the currently active tab.
                // TODO(crbug.com/518307037): These should receive a slightly different background
                // than the selected tab.
                List<Tab> relatedTabs = getRelatedTabsForId(getTabId(viewHolder));
                if (relatedTabs != null) {
                    for (Tab tab : relatedTabs) {
                        int tabId = tab.getId();
                        int childIndex = mModel.indexFromTabId(tabId);
                        if (childIndex != TabModel.INVALID_TAB_INDEX
                                && childIndex != mSelectedTabIndex) {
                            if (tabId != currentSelectedTabId) {
                                PropertyModel childModel = mModel.get(childIndex).model;
                                mSelectedGroupTabIds.add(tabId);
                                childModel.set(TabProperties.IS_SELECTED, true);
                            }
                        }
                    }
                }
            } else {
                selectTab(viewHolder, TabSelectionType.FROM_DRAG);
            }
        } else if (actionState == ItemTouchHelper.ACTION_STATE_IDLE) {
            stopThrottling();
            mSelectedViewHolder = null;
            if (mSelectedTabIndex != TabModel.INVALID_TAB_INDEX) {
                mModel.updateSelectedCardForSelection(mSelectedTabIndex, false);
                mSelectedTabIndex = TabModel.INVALID_TAB_INDEX;
            }
            int currentSelectedTabId = getCurrentSelectedTabId();

            // Clean up the temporary IS_SELECTED state from the inactive children.
            for (int childTabId : mSelectedGroupTabIds) {
                if (childTabId == currentSelectedTabId) continue;
                int childIndex = mModel.indexFromTabId(childTabId);
                if (childIndex != TabModel.INVALID_TAB_INDEX) {
                    PropertyModel childModel = mModel.get(childIndex).model;
                    childModel.set(TabProperties.IS_SELECTED, false);
                }
            }
            mSelectedGroupTabIds.clear();
        }
    }

    @Override
    public void setOnLongPressTabItemEventListener(
            TabGridItemLongPressOrchestrator.@Nullable OnLongPressTabItemEventListener listener) {
        if (listener != null) {
            // Use 10ms so that the context menu appears almost immediately after the ~500ms
            // long-press.
            mTabGridItemLongPressOrchestrator =
                    new TabGridItemLongPressOrchestrator(
                            mRecyclerViewSupplier,
                            mModel,
                            listener,
                            mLongPressDpCancelThreshold,
                            CONTEXT_MENU_ORCHESTRATOR_DELAY_MS);
        } else {
            mTabGridItemLongPressOrchestrator = null;
        }
    }

    @Override
    public void onChildDraw(
            Canvas c,
            RecyclerView recyclerView,
            RecyclerView.ViewHolder viewHolder,
            float dX,
            float dY,
            int actionState,
            boolean isCurrentlyActive) {

        float renderDx = dX;
        float renderDy = dY;

        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            float topLimitDy = recyclerView.getPaddingTop() - viewHolder.itemView.getTop();
            float bottomLimitDy =
                    recyclerView.getHeight()
                            - recyclerView.getPaddingBottom()
                            - viewHolder.itemView.getBottom();
            renderDy = MathUtils.clamp(dY, topLimitDy, bottomLimitDy);

            if (isPinnedRegularTab(viewHolder)) {
                // Clamp horizontal movement to the left and right edges for pinned tabs.
                float leftLimitDx = recyclerView.getPaddingLeft() - viewHolder.itemView.getLeft();
                float rightLimitDx =
                        recyclerView.getWidth()
                                - recyclerView.getPaddingRight()
                                - viewHolder.itemView.getRight();
                renderDx = MathUtils.clamp(dX, leftLimitDx, rightLimitDx);
            } else {
                // Suppress visual horizontal movement for regular tabs.
                renderDx = 0f;
            }
        }

        super.onChildDraw(
                c, recyclerView, viewHolder, renderDx, renderDy, actionState, isCurrentlyActive);
        if (!mIsMouseInputSource) {
            float displacementSquared = calcMagnitudeSquared(dX, dY);
            if (mTabGridItemLongPressOrchestrator != null) {
                mTabGridItemLongPressOrchestrator.processChildDisplacement(displacementSquared);
            }
            if (!mHasFiredDragMovementCallback
                    && displacementSquared
                            > mLongPressDpCancelThreshold * mLongPressDpCancelThreshold) {
                mHasFiredDragMovementCallback = true;
                if (mOnDragStartCallback != null) {
                    mOnDragStartCallback.run();
                }
            }
        }

        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            if (isCurrentlyActive && mOnDragOutListener != null) {
                float cursorX = mDragStartX + dX;

                float panelLeft = 0;
                float panelRight = recyclerView.getWidth();

                if (recyclerView.getParent() instanceof android.view.View parentView) {
                    panelLeft = -recyclerView.getLeft();
                    panelRight = parentView.getWidth() - recyclerView.getLeft();
                }

                if (cursorX < panelLeft || cursorX > panelRight) {
                    mOnDragOutListener.onDragOut(viewHolder, dX, dY);
                }
            }

            if (isDraggedItemCollapsed()) return;
            if (!hasTabPropertiesModel(viewHolder)) return;
            setDraggingY(viewHolder, isCurrentlyActive ? renderDy : null);

            if (viewHolder.getItemViewType() != TabProperties.UiType.TAB_GROUP
                    && !isSolitaryChild(viewHolder)) return;

            Token groupId = getTabGroupId(viewHolder);
            if (groupId == null) return;

            Set<Integer> currentChildIds = new HashSet<>();
            for (int i = 0; i < recyclerView.getChildCount(); i++) {
                View childView = recyclerView.getChildAt(i);
                RecyclerView.ViewHolder childViewHolder =
                        recyclerView.getChildViewHolder(childView);

                if (childViewHolder == viewHolder) continue;

                if (hasTabPropertiesModel(childViewHolder)) {
                    Token childGroupId = getTabGroupId(childViewHolder);
                    if (groupId.equals(childGroupId)) {
                        int childTabId = getTabId(childViewHolder);
                        currentChildIds.add(childTabId);

                        if (recyclerView.getItemAnimator() != null) {
                            recyclerView.getItemAnimator().endAnimation(childViewHolder);
                        }

                        if (isCurrentlyActive
                                && !mDraggedChildViewHolders.contains(childViewHolder)) {
                            childViewHolder.setIsRecyclable(false);
                            mDraggedChildViewHolders.add(childViewHolder);
                        }
                        childView.setTranslationY(renderDy);

                        mDraggedChildTabIds.add(childTabId);
                        if (isCurrentlyActive) {
                            childView.setTranslationZ(viewHolder.itemView.getElevation());
                        } else {
                            // Reset translation of non-active children after release.
                            childView.setTranslationZ(0f);
                        }
                    }
                }
            }

            for (RecyclerView.ViewHolder childViewHolder : mDraggedChildViewHolders) {
                View childView = childViewHolder.itemView;
                if (childView.getParent() != recyclerView) {
                    // Ensure it is in the overlay when explicitly detached
                    recyclerView.getOverlay().add(childView);
                    childView.setTranslationY(renderDy);
                    if (isCurrentlyActive) {
                        childView.setTranslationZ(viewHolder.itemView.getElevation());
                    } else {
                        childView.setTranslationZ(0f);
                    }
                }
            }

            // Restore any views that scrolled off screen or were recycled away from the group.
            Iterator<Integer> it = mDraggedChildTabIds.iterator();
            while (it.hasNext()) {
                int savedTabId = it.next();
                if (!currentChildIds.contains(savedTabId)) {
                    // If the view is still attached but no longer in the group, reset it.
                    for (int i = 0; i < recyclerView.getChildCount(); i++) {
                        View childView = recyclerView.getChildAt(i);
                        RecyclerView.ViewHolder childViewHolder =
                                recyclerView.getChildViewHolder(childView);
                        if (hasTabPropertiesModel(childViewHolder)
                                && getTabId(childViewHolder) == savedTabId) {
                            childView.setTranslationZ(0f);
                            childView.setTranslationY(0f);
                            break;
                        }
                    }

                    Iterator<RecyclerView.ViewHolder> holderIt =
                            mDraggedChildViewHolders.iterator();
                    while (holderIt.hasNext()) {
                        RecyclerView.ViewHolder trackedHolder = holderIt.next();
                        if (hasTabPropertiesModel(trackedHolder)
                                && getTabId(trackedHolder) == savedTabId) {
                            trackedHolder.setIsRecyclable(true);
                            recyclerView.getOverlay().remove(trackedHolder.itemView);

                            // Because it was in the overlay, it might have structural translations
                            // persisting.
                            trackedHolder.itemView.setTranslationZ(0f);
                            trackedHolder.itemView.setTranslationY(0f);
                            holderIt.remove();
                            break;
                        }
                    }

                    it.remove();
                }
            }
        }
    }

    @Override
    public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        super.clearView(recyclerView, viewHolder);
        if (viewHolder == mSelectedViewHolder || mSelectedViewHolder == null) {
            stopThrottling();
        }
        for (RecyclerView.ViewHolder childViewHolder : mDraggedChildViewHolders) {
            childViewHolder.setIsRecyclable(true);
            recyclerView.getOverlay().remove(childViewHolder.itemView);
        }
        mDraggedChildViewHolders.clear();

        // Safeguard finger lift: explicitly wipe out any running timer threads.
        if (mTabGridItemLongPressOrchestrator != null) {
            mTabGridItemLongPressOrchestrator.cancel();
        }
        mHasFiredDragMovementCallback = false;
        setDraggingY(viewHolder, null);
        // When the drag completely finishes, clean up all manual visual overrides on children.
        if (viewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP
                || isSolitaryChild(viewHolder)) {
            Token groupId = getTabGroupId(viewHolder);
            if (groupId != null) {
                for (int i = 0; i < recyclerView.getChildCount(); i++) {
                    View childView = recyclerView.getChildAt(i);
                    RecyclerView.ViewHolder childViewHolder =
                            recyclerView.getChildViewHolder(childView);

                    if (childViewHolder == viewHolder) continue;

                    if (hasTabPropertiesModel(childViewHolder)) {
                        Token childGroupId = getTabGroupId(childViewHolder);
                        if (groupId.equals(childGroupId)) {
                            childView.setTranslationZ(0f);
                            childView.setTranslationY(0f);
                        }
                    }
                }
            }
        }
        if (mDragStartTabId != Tab.INVALID_TAB_ID) {
            @DragDropResult int dragResult = computeDragDropResult(viewHolder);
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.VerticalTabs.DragDropResult", dragResult, DragDropResult.COUNT);
            long now = SystemClock.elapsedRealtime();
            pruneExpiredGroupTimestamps(now);
            if (dragResult == DragDropResult.GROUPED) {
                mGroupedTabTimestamps.put(mDragStartTabId, now);
                for (int childTabId : mDraggedChildTabIds) {
                    mGroupedTabTimestamps.put(childTabId, now);
                }
            } else if (dragResult == DragDropResult.UNGROUPED) {
                int index = mGroupedTabTimestamps.indexOfKey(mDragStartTabId);
                if (index >= 0) {
                    long durationMs = now - mGroupedTabTimestamps.valueAt(index);
                    if (durationMs <= MAX_UNGROUP_TRACKING_DURATION_MS) {
                        RecordHistogram.recordMediumTimesHistogram(
                                "Android.VerticalTabs.DragDropTimeToUngroup", durationMs);
                    }
                    mGroupedTabTimestamps.removeAt(index);
                }
            }
            mDragStartTabId = Tab.INVALID_TAB_ID;
            mDragStartGroupId = null;
            mDragStartTabModelIndex = TabModel.INVALID_TAB_INDEX;
            mIsOSNewWindowDrop = false;
        }
        mSavedItemStates.clear();
        mCollapsedItems.clear();
        mCollapsedViewHolder = null;
        mDraggedTabId = Tab.INVALID_TAB_ID;
        mDraggedGroupId = null;
        mIsDraggedGroupHeader = false;
        mDraggedItemViewType = -1;
        mDraggedChildTabIds.clear();
    }

    @Override
    public boolean shouldAllowDragPastLayout() {
        return true;
    }

    @Override
    public RecyclerView.@Nullable ViewHolder findLiveViewHolder(
            RecyclerView recyclerView, RecyclerView.ViewHolder current) {
        if (current == null || !hasTabPropertiesModel(current)) return null;
        int currentTabId = getTabId(current);
        Token currentGroupId = getTabGroupId(current);
        boolean isGroupHeader = current.getItemViewType() == TabProperties.UiType.TAB_GROUP;

        for (int i = 0; i < recyclerView.getChildCount(); i++) {
            View childView = recyclerView.getChildAt(i);
            RecyclerView.ViewHolder childViewHolder = recyclerView.getChildViewHolder(childView);
            if (!hasTabPropertiesModel(childViewHolder)) continue;

            if (isGroupHeader) {
                if (childViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP
                        && Objects.equals(getTabGroupId(childViewHolder), currentGroupId)) {
                    return childViewHolder;
                }
            } else if (getTabId(childViewHolder) == currentTabId) {
                return childViewHolder;
            }
        }
        return null;
    }

    private static final long EXTERNAL_DROP_RESTORE_DELAY_MS = 1000L;

    private static class CollapsedItemState {
        public final int width;
        public final int height;
        public final int topMargin;
        public final int bottomMargin;
        public final int marginStart;
        public final int marginEnd;

        CollapsedItemState(
                ViewGroup.MarginLayoutParams params,
                @Nullable CollapsedItemState fallback,
                @Nullable Context context,
                boolean isPinned) {
            int w = params.width;
            int h = params.height;
            int tm = params.topMargin;
            int bm = params.bottomMargin;
            int ms = params.getMarginStart();
            int me = params.getMarginEnd();

            if (w == 0 && h == 0) {
                if (fallback != null) {
                    w = fallback.width;
                    h = fallback.height;
                    tm = fallback.topMargin;
                    bm = fallback.bottomMargin;
                    ms = fallback.marginStart;
                    me = fallback.marginEnd;
                } else if (context != null) {
                    Resources res = context.getResources();
                    h =
                            isPinned
                                    ? TabVerticalViewBinder.getPinnedItemHeight(context)
                                    : TabVerticalViewBinder.getTabItemHeight(context);
                    w =
                            isPinned
                                    ? TabVerticalViewBinder.getPinnedItemMinWidth(context)
                                    : ViewGroup.LayoutParams.MATCH_PARENT;
                    bm =
                            res.getDimensionPixelSize(
                                    isPinned
                                            ? R.dimen.vertical_tab_pinned_item_margin_bottom
                                            : R.dimen.vertical_tab_item_margin_bottom);
                }
            }

            this.width = w;
            this.height = h;
            this.topMargin = tm;
            this.bottomMargin = bm;
            this.marginStart = ms;
            this.marginEnd = me;
        }

        void restore(ViewGroup.MarginLayoutParams params) {
            params.width = width;
            params.height = height;
            params.topMargin = topMargin;
            params.bottomMargin = bottomMargin;
            params.setMarginStart(marginStart);
            params.setMarginEnd(marginEnd);
        }

        void collapse(ViewGroup.MarginLayoutParams params) {
            params.width = 0;
            params.height = 0;
            params.topMargin = 0;
            params.bottomMargin = 0;
            params.setMarginStart(0);
            params.setMarginEnd(0);
        }
    }

    private static class CollapsedViewHolderInfo {
        public final View view;
        public final RecyclerView.ViewHolder viewHolder;
        public final CollapsedItemState state;
        public final float initialAlpha;

        CollapsedViewHolderInfo(
                RecyclerView.ViewHolder viewHolder, CollapsedItemState state, float initialAlpha) {
            this.view = viewHolder.itemView;
            this.viewHolder = viewHolder;
            this.state = state;
            this.initialAlpha = initialAlpha;
        }
    }

    private final List<CollapsedViewHolderInfo> mCollapsedItems = new ArrayList<>();
    private final List<CollapsedViewHolderInfo> mDelayedRestorationItems = new ArrayList<>();
    private final Map<Object, CollapsedItemState> mSavedItemStates = new HashMap<>();
    private RecyclerView.@Nullable ViewHolder mCollapsedViewHolder;
    private int mDraggedTabId = Tab.INVALID_TAB_ID;
    private @Nullable Token mDraggedGroupId;
    private boolean mIsDraggedGroupHeader;
    private int mDraggedItemViewType = -1;

    @VisibleForTesting @Nullable Runnable mDelayedExternalItemRestorationRunnable;

    private final View.OnAttachStateChangeListener mDelayedExternalItemRestorationDetachListener =
            new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View v) {}

                @Override
                public void onViewDetachedFromWindow(View v) {
                    cancelDelayedExternalItemRestoration();
                }
            };

    private final RecyclerView.OnChildAttachStateChangeListener mChildAttachListener =
            new RecyclerView.OnChildAttachStateChangeListener() {
                @Override
                public void onChildViewAttachedToWindow(View view) {
                    onChildAttached(view);
                }

                @Override
                public void onChildViewDetachedFromWindow(View view) {}
            };

    private boolean matchesDraggedItem(RecyclerView.@Nullable ViewHolder holder) {
        if (holder == null || !hasTabPropertiesModel(holder)) {
            return false;
        }
        if (mIsDraggedGroupHeader && mDraggedGroupId != null) {
            return Objects.equals(getTabGroupId(holder), mDraggedGroupId);
        } else if (mDraggedTabId != Tab.INVALID_TAB_ID) {
            return getTabId(holder) == mDraggedTabId;
        }
        return false;
    }

    private @Nullable CollapsedViewHolderInfo removeCollapsedViewHolderInfo(
            @Nullable View view, RecyclerView.@Nullable ViewHolder holder) {
        if (view == null && holder == null) return null;
        for (int i = 0; i < mCollapsedItems.size(); i++) {
            CollapsedViewHolderInfo info = mCollapsedItems.get(i);
            if ((view != null && info.view == view)
                    || (holder != null && info.viewHolder == holder)) {
                return mCollapsedItems.remove(i);
            }
        }
        return null;
    }

    private void collapseViewHolderWithFallback(
            RecyclerView.ViewHolder holder, @Nullable CollapsedViewHolderInfo previousInfo) {
        CollapsedItemState fallbackState = previousInfo != null ? previousInfo.state : null;
        float initialAlpha = previousInfo != null ? previousInfo.initialAlpha : 1.0f;
        collapseViewHolder(holder, fallbackState, initialAlpha);
    }

    private void onChildAttached(View view) {
        if (!isDraggedItemCollapsed()) {
            return;
        }
        RecyclerView recyclerView = mRecyclerViewSupplier.get();
        if (recyclerView == null) {
            return;
        }
        RecyclerView.ViewHolder holder = recyclerView.getChildViewHolder(view);
        if (matchesDraggedItem(holder)) {
            CollapsedViewHolderInfo existingInfo = removeCollapsedViewHolderInfo(view, holder);
            collapseViewHolderWithFallback(holder, existingInfo);
        }
    }

    RecyclerView.OnChildAttachStateChangeListener getOnChildAttachStateChangeListenerForTesting() {
        return mChildAttachListener;
    }

    private Object getItemKey(RecyclerView.ViewHolder holder) {
        if (holder.getItemViewType() == TabProperties.UiType.TAB_GROUP) {
            Token groupId = getTabGroupId(holder);
            if (groupId != null) return groupId;
        }
        if (hasTabPropertiesModel(holder)) {
            return getTabId(holder);
        }
        return holder.itemView;
    }

    private void restoreCollapsedItem(CollapsedViewHolderInfo info) {
        info.view.removeOnAttachStateChangeListener(mDelayedExternalItemRestorationDetachListener);
        boolean isHiddenPinnedPlaceholder = info.view.getId() == R.id.hidden_pinned_tab;
        if (!isHiddenPinnedPlaceholder) {
            info.view.setVisibility(View.VISIBLE);
        }
        info.view.setAlpha(info.initialAlpha);
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) info.view.getLayoutParams();
        if (params != null) {
            info.state.restore(params);
            info.view.setLayoutParams(params);
        }
        info.view.setTranslationY(0f);
        info.view.setTranslationZ(0f);
    }

    /** Cancels any pending delayed restoration of an externally dropped item. */
    public void cancelDelayedExternalItemRestoration() {
        RecyclerView recyclerView = mRecyclerViewSupplier.get();
        if (recyclerView != null && mDelayedExternalItemRestorationRunnable != null) {
            recyclerView.removeCallbacks(mDelayedExternalItemRestorationRunnable);
        }
        if (!mDelayedRestorationItems.isEmpty()) {
            List<CollapsedViewHolderInfo> itemsToCancel = new ArrayList<>(mDelayedRestorationItems);
            mDelayedRestorationItems.clear();
            for (CollapsedViewHolderInfo info : itemsToCancel) {
                restoreCollapsedItem(info);
            }
        }
        mDelayedExternalItemRestorationRunnable = null;
    }

    View.OnAttachStateChangeListener getDelayedExternalItemRestorationDetachListenerForTesting() {
        return mDelayedExternalItemRestorationDetachListener;
    }

    private RecyclerView.@Nullable ViewHolder getLiveViewHolder() {
        if (mCollapsedViewHolder != null) {
            boolean isStillValid =
                    mCollapsedViewHolder.getItemViewType() == mDraggedItemViewType
                            && matchesDraggedItem(mCollapsedViewHolder);
            if (!isStillValid) {
                mCollapsedViewHolder = null;
            }
        }

        RecyclerView recyclerView = mRecyclerViewSupplier.get();
        if (recyclerView == null) return mCollapsedViewHolder;

        if (mCollapsedViewHolder != null
                && mCollapsedViewHolder.itemView.getParent() == recyclerView) {
            return mCollapsedViewHolder;
        }

        for (int i = 0; i < recyclerView.getChildCount(); i++) {
            View childView = recyclerView.getChildAt(i);
            RecyclerView.ViewHolder childViewHolder = recyclerView.getChildViewHolder(childView);
            if (childViewHolder.getItemViewType() == mDraggedItemViewType
                    && matchesDraggedItem(childViewHolder)) {
                mCollapsedViewHolder = childViewHolder;
                return childViewHolder;
            }
        }
        return mCollapsedViewHolder;
    }

    private void collapseViewHolder(
            RecyclerView.ViewHolder holder,
            @Nullable CollapsedItemState fallbackState,
            float initialAlpha) {
        RecyclerView recyclerView = mRecyclerViewSupplier.get();
        if (recyclerView != null && recyclerView.getItemAnimator() != null) {
            recyclerView.getItemAnimator().endAnimation(holder);
        }

        View itemView = holder.itemView;
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) itemView.getLayoutParams();
        if (params != null) {
            Object key = getItemKey(holder);
            if (fallbackState == null) {
                fallbackState = mSavedItemStates.get(key);
            }
            boolean isPinned = isPinnedRegularTab(holder);
            CollapsedItemState state =
                    new CollapsedItemState(params, fallbackState, itemView.getContext(), isPinned);
            if (state.width > 0 || state.height > 0) {
                mSavedItemStates.put(key, state);
            }
            mCollapsedItems.add(new CollapsedViewHolderInfo(holder, state, initialAlpha));
            state.collapse(params);
            itemView.setLayoutParams(params);
        }
        itemView.setVisibility(View.GONE);
        itemView.setAlpha(0f);
        itemView.setTranslationY(0f);
        itemView.setTranslationZ(0f);
    }

    /**
     * Completely hides the dragged item from the layout by shrinking it to 0px. This is required to
     * close the gap left by the dragged item in vertical lists.
     *
     * @param viewHolder The initial ViewHolder being dragged out, or null to lookup the live
     *     ViewHolder.
     */
    public void collapseDraggedItem(RecyclerView.@Nullable ViewHolder viewHolder) {
        cancelDelayedExternalItemRestoration();
        if (viewHolder != null) {
            mCollapsedViewHolder = viewHolder;
            mDraggedItemViewType = viewHolder.getItemViewType();
            if (hasTabPropertiesModel(viewHolder)) {
                mDraggedTabId = getTabId(viewHolder);
                mDraggedGroupId = getTabGroupId(viewHolder);
                mIsDraggedGroupHeader = mDraggedItemViewType == TabProperties.UiType.TAB_GROUP;
            }
        }

        RecyclerView recyclerView = mRecyclerViewSupplier.get();
        if (recyclerView == null) return;

        List<RecyclerView.ViewHolder> holdersToCollapse = new ArrayList<>();
        Set<RecyclerView.ViewHolder> seenHolders = new HashSet<>();

        if (mIsDraggedGroupHeader && mDraggedGroupId != null) {
            for (RecyclerView.ViewHolder childViewHolder : mDraggedChildViewHolders) {
                if (hasTabPropertiesModel(childViewHolder)
                        && Objects.equals(getTabGroupId(childViewHolder), mDraggedGroupId)) {
                    if (seenHolders.add(childViewHolder)) {
                        holdersToCollapse.add(childViewHolder);
                    }
                }
            }
            for (CollapsedViewHolderInfo info : mCollapsedItems) {
                if (info.viewHolder != null && seenHolders.add(info.viewHolder)) {
                    holdersToCollapse.add(info.viewHolder);
                }
            }
            RecyclerView.ViewHolder liveViewHolder = getLiveViewHolder();
            if (liveViewHolder != null && seenHolders.add(liveViewHolder)) {
                holdersToCollapse.add(liveViewHolder);
            }
            for (int i = 0; i < recyclerView.getChildCount(); i++) {
                View child = recyclerView.getChildAt(i);
                RecyclerView.ViewHolder childHolder = recyclerView.getChildViewHolder(child);
                if (hasTabPropertiesModel(childHolder)) {
                    Token childGroupId = getTabGroupId(childHolder);
                    if (Objects.equals(childGroupId, mDraggedGroupId)) {
                        if (seenHolders.add(childHolder)) {
                            holdersToCollapse.add(childHolder);
                        }
                    }
                }
            }
        } else {
            RecyclerView.ViewHolder liveViewHolder = getLiveViewHolder();
            if (liveViewHolder != null) {
                holdersToCollapse.add(liveViewHolder);
            }
        }

        // Clean up any overlay views and reset translations from in-strip dragging
        for (RecyclerView.ViewHolder childViewHolder : mDraggedChildViewHolders) {
            childViewHolder.setIsRecyclable(true);
            recyclerView.getOverlay().remove(childViewHolder.itemView);
            childViewHolder.itemView.setTranslationZ(0f);
            childViewHolder.itemView.setTranslationY(0f);
        }
        mDraggedChildViewHolders.clear();

        // Map previous states by view to use as fallback if re-collapsing
        Map<View, CollapsedItemState> previousStates = new HashMap<>();
        for (CollapsedViewHolderInfo info : mCollapsedItems) {
            previousStates.put(info.view, info.state);
        }
        mCollapsedItems.clear();

        for (RecyclerView.ViewHolder holder : holdersToCollapse) {
            View itemView = holder.itemView;
            float currentAlpha = itemView.getAlpha();
            float initialAlpha = currentAlpha > 0f ? currentAlpha : 1.0f;
            CollapsedItemState fallback = previousStates.get(itemView);
            collapseViewHolder(holder, fallback, initialAlpha);
        }

        recyclerView.invalidate();
    }

    /**
     * Restores the dragged item's layout dimensions and sets visibility back to VISIBLE.
     *
     * @param isOSNewWindowDrop If true, delays the restoration by {@link
     *     #EXTERNAL_DROP_RESTORE_DELAY_MS}. Required when the drag ended externally and the item
     *     might be removed asynchronously. If the item is detached before the delay completes, the
     *     pending restoration is cancelled to prevent ghost tabs.
     */
    public void restoreDraggedItem(boolean isOSNewWindowDrop) {
        cancelDelayedExternalItemRestoration();
        mIsOSNewWindowDrop = isOSNewWindowDrop;
        if (mCollapsedItems.isEmpty()) {
            return;
        }

        final List<CollapsedViewHolderInfo> itemsToRestore = new ArrayList<>(mCollapsedItems);
        mCollapsedItems.clear();
        mCollapsedViewHolder = null;

        mDelayedRestorationItems.clear();
        mDelayedRestorationItems.addAll(itemsToRestore);

        mDelayedExternalItemRestorationRunnable =
                () -> {
                    RecyclerView recyclerView = mRecyclerViewSupplier.get();
                    for (CollapsedViewHolderInfo info : mDelayedRestorationItems) {
                        restoreCollapsedItem(info);
                    }
                    mDelayedRestorationItems.clear();
                    mDelayedExternalItemRestorationRunnable = null;

                    if (recyclerView != null) {
                        for (int i = 0; i < recyclerView.getChildCount(); i++) {
                            View child = recyclerView.getChildAt(i);
                            RecyclerView.ViewHolder childHolder =
                                    recyclerView.getChildViewHolder(child);
                            if (childHolder != null && hasTabPropertiesModel(childHolder)) {
                                boolean matches = false;
                                if (mIsDraggedGroupHeader && mDraggedGroupId != null) {
                                    matches =
                                            Objects.equals(
                                                    getTabGroupId(childHolder), mDraggedGroupId);
                                } else if (mDraggedTabId != Tab.INVALID_TAB_ID) {
                                    matches = getTabId(childHolder) == mDraggedTabId;
                                }
                                if (matches) {
                                    boolean isHiddenPinnedPlaceholder =
                                            child.getId() == R.id.hidden_pinned_tab;
                                    if (!isHiddenPinnedPlaceholder) {
                                        child.setVisibility(View.VISIBLE);
                                    }
                                    child.setAlpha(1.0f);
                                    Object key = getItemKey(childHolder);
                                    CollapsedItemState savedState = mSavedItemStates.get(key);
                                    ViewGroup.MarginLayoutParams params =
                                            (ViewGroup.MarginLayoutParams) child.getLayoutParams();
                                    if (savedState != null && params != null) {
                                        savedState.restore(params);
                                        child.setLayoutParams(params);
                                    }
                                    child.setTranslationY(0f);
                                    child.setTranslationZ(0f);
                                }
                            }
                        }
                        ViewUtils.requestLayout(
                                recyclerView,
                                "VerticalTabListItemTouchHelperCallback.restoreDraggedItem");
                        recyclerView.invalidate();
                    }
                };

        if (isOSNewWindowDrop) {
            for (CollapsedViewHolderInfo info : mDelayedRestorationItems) {
                info.view.addOnAttachStateChangeListener(
                        mDelayedExternalItemRestorationDetachListener);
            }
            RecyclerView recyclerView = mRecyclerViewSupplier.get();
            if (recyclerView != null) {
                recyclerView.postDelayed(
                        mDelayedExternalItemRestorationRunnable, EXTERNAL_DROP_RESTORE_DELAY_MS);
            }
        } else {
            mDelayedExternalItemRestorationRunnable.run();
        }
    }

    public boolean isDraggedItemCollapsed() {
        return !mCollapsedItems.isEmpty();
    }

    @Override
    public void onExternalDragItemRebound(
            RecyclerView.ViewHolder oldHolder, RecyclerView.ViewHolder newHolder) {
        if (isDraggedItemCollapsed()) {
            CollapsedViewHolderInfo oldInfo =
                    removeCollapsedViewHolderInfo(
                            oldHolder != null ? oldHolder.itemView : null, oldHolder);

            boolean newHolderMatches = matchesDraggedItem(newHolder);

            if (oldHolder != null
                    && oldInfo != null
                    && (!newHolderMatches || oldHolder != newHolder)) {
                restoreCollapsedItem(oldInfo);
            }

            if (newHolder != null && newHolderMatches) {
                if (newHolder.getItemViewType() == mDraggedItemViewType) {
                    mCollapsedViewHolder = newHolder;
                }
                collapseViewHolderWithFallback(newHolder, oldInfo);
            }
        }
    }

    /**
     * Determines whether a dragged child tab has escaped the visual boundaries of its tab group.
     *
     * <p>This establishes a "drop zone" just outside the top and bottom of a group. When a child
     * tab is dragged past this threshold, it is immediately ungrouped. By returning true and
     * executing the ungroup early, it short-circuits ItemTouchHelper's swap logic, preventing the
     * dragged tab from erroneously swapping with adjacent groups and leaping past them.
     */
    @Override
    public boolean hasDragEscapedBounds(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder viewHolder,
            int x,
            int y,
            float dx,
            float dy) {
        if (isDraggedItemCollapsed()) return false;
        if (!hasTabPropertiesModel(viewHolder)) return false;
        if (viewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP) return false;

        Token groupId = getTabGroupId(viewHolder);
        if (groupId == null) return false;

        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return false;

        int currentTabId = getTabId(viewHolder);
        Tab currentTab = tabModel.getTabById(currentTabId);
        if (currentTab == null) return false;

        List<Tab> relatedTabs = getRelatedTabsForId(currentTabId);
        // This implicitly covers the isSolitaryChild check as well!
        if (relatedTabs == null || relatedTabs.size() <= 1) return false;
        RecyclerView.LayoutManager layoutManager = recyclerView.getLayoutManager();

        boolean isFirstInGroup = currentTab.getId() == relatedTabs.get(0).getId();
        boolean isLastInGroup =
                currentTab.getId() == relatedTabs.get(relatedTabs.size() - 1).getId();

        if (dy > 0 && isLastInGroup) {
            // Dragging down does not require crossing the group header, so it uses a smaller
            // threshold.
            int downThreshold = viewHolder.itemView.getHeight() / 4;
            if (y > viewHolder.itemView.getTop() + downThreshold) {
                // Check if the dragged tab is at the bottom of the RecyclerView viewport.
                // Since this early return skips ItemTouchHelper's bounds scrolling logic,
                // we manually track this to prevent list layout shifts later.
                boolean isChildAtBottom = false;
                if (layoutManager != null) {
                    int maxBottom = layoutManager.getDecoratedBottom(viewHolder.itemView);
                    if (maxBottom >= recyclerView.getHeight() - recyclerView.getPaddingBottom()) {
                        isChildAtBottom = true;
                    }
                }

                NestedTabReorderUtils.ungroupTab(tabModel, currentTab, true);

                // If ungrouping pushes the new standalone tab off-screen at the bottom,
                // instruct RecyclerView to scroll to it, keeping it pinned under the user's finger.
                if (isChildAtBottom && layoutManager != null) {
                    int childIndex = mModel.indexFromTabId(currentTab.getId());
                    if (childIndex != TabModel.INVALID_TAB_INDEX) {
                        layoutManager.scrollToPosition(childIndex);
                    }
                }
                return true;
            }
        } else if (dy < 0 && isFirstInGroup) {
            // Dragging up requires crossing the group header which sits above the first tab.
            int upThreshold = viewHolder.itemView.getHeight() / 2;
            if (y < viewHolder.itemView.getTop() - upThreshold) {
                // Check if the group header is abutting the top of the RecyclerView padding.
                // Since this early return skips ItemTouchHelper's bounds scrolling logic,
                // we manually track this so we can anchor the scroll to the new tab position.
                boolean isHeaderAtTop = false;
                if (layoutManager != null) {
                    for (int i = 0; i < recyclerView.getChildCount(); i++) {
                        View child = recyclerView.getChildAt(i);
                        // TODO(crbug.com/518307037): Use the TabModel directly instead.
                        RecyclerView.ViewHolder childViewHolder =
                                recyclerView.getChildViewHolder(child);
                        if (childViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP
                                && groupId.equals(getTabGroupId(childViewHolder))) {
                            if (layoutManager.getDecoratedTop(child)
                                    <= recyclerView.getPaddingTop()) {
                                isHeaderAtTop = true;
                            }
                            break;
                        }
                    }
                }

                NestedTabReorderUtils.ungroupTab(tabModel, currentTab, false);

                // If ungrouping prepends the new tab natively off-screen at the top,
                // manually scroll to the new tab. This forces the group header to visually shift
                // down, rather than overlapping the new tab and causing an immediate re-grouping.
                if (isHeaderAtTop && layoutManager != null) {
                    int childIndex = mModel.indexFromTabId(currentTab.getId());
                    if (childIndex != TabModel.INVALID_TAB_INDEX) {
                        layoutManager.scrollToPosition(childIndex);
                    }
                }
                return true;
            }
        }

        return false;
    }

    /**
     * Determines whether the view holder represents a tab that is the only child of its group.
     *
     * <p>When a group contains only one child, dragging that child behaves identically to dragging
     * the tab group header itself (i.e. it moves the entire group rather than ungrouping the
     * child).
     */
    private boolean isSolitaryChild(RecyclerView.ViewHolder viewHolder) {
        if (viewHolder instanceof ViewHolder simpleViewHolder) {
            return NestedTabReorderUtils.isSolitaryChild(
                    mCurrentTabModelSupplier.get(), simpleViewHolder.model);
        }
        return false;
    }

    private @Nullable Token getTabGroupId(RecyclerView.ViewHolder viewHolder) {
        if (viewHolder instanceof ViewHolder simpleViewHolder) {
            return NestedTabReorderUtils.getTabGroupId(simpleViewHolder.model);
        }
        return null;
    }

    private int getCurrentSelectedTabId() {
        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return Tab.INVALID_TAB_ID;
        Tab currentTab = tabModel.getTabAt(tabModel.index());
        return currentTab != null ? currentTab.getId() : Tab.INVALID_TAB_ID;
    }

    private int getTabId(RecyclerView.ViewHolder viewHolder) {
        return assumeNonNull(((ViewHolder) viewHolder).model).get(TabProperties.TAB_ID);
    }

    private void selectTab(RecyclerView.ViewHolder viewHolder, @TabSelectionType int type) {
        if (viewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP) {
            return;
        }
        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return;

        int tabId = getTabId(viewHolder);
        Tab tab = tabModel.getTabById(tabId);
        selectTabInternal(tabModel, tab, type);
    }

    /**
     * Selects an appropriate tab to represent the group when the group header is interacted with.
     * If a tab within this group is already the currently selected tab in the model, that selection
     * is preserved. Otherwise, it defaults to selecting the first tab in the group.
     *
     * @param viewHolder The group header's view holder.
     */
    private void selectTabForGroup(RecyclerView.ViewHolder viewHolder) {
        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return;

        int tabId = getTabId(viewHolder);
        List<Tab> relatedTabs = getRelatedTabsForId(tabId);
        if (relatedTabs == null || relatedTabs.isEmpty()) return;

        Tab tabToSelect = relatedTabs.get(0);
        int currentIndex = tabModel.index();
        if (currentIndex != TabModel.INVALID_TAB_INDEX) {
            Tab currentSelectedTab = tabModel.getTabAt(currentIndex);
            if (currentSelectedTab != null && relatedTabs.contains(currentSelectedTab)) {
                tabToSelect = currentSelectedTab;
            }
        }

        selectTabInternal(tabModel, tabToSelect, TabSelectionType.FROM_DRAG);
    }

    private void selectTabInternal(
            TabModel tabModel, @Nullable Tab tab, @TabSelectionType int type) {
        if (tab == null) return;

        if (TabMultiSelectHelper.hasMultipleTabsSelected(tabModel)) {
            // TODO(crbug.com/544185227): Support batch drag and drop of multi-selected tabs.
            // Currently, we fallback to a standard single-tab drag by clearing
            // the multi-selection state if the user drags a highlighted item.
            tabModel.clearMultiSelection(/* notifyObservers= */ true);
        }

        int index = tabModel.indexOf(tab);
        if (index != TabModel.INVALID_TAB_INDEX && index != tabModel.index()) {
            tabModel.setIndex(index, type);
        }
    }

    private void setDraggingY(RecyclerView.ViewHolder viewHolder, @Nullable Float draggingY) {
        if (viewHolder instanceof ViewHolder simpleViewHolder) {
            PropertyModel model = simpleViewHolder.model;
            if (model != null) {
                model.set(TabProperties.DRAGGING_Y, draggingY);
            }
        }
    }

    /**
     * Creates an {@link RecyclerView.OnItemTouchListener} that detects mouse drags and initiates
     * instant dragging.
     *
     * @param itemTouchHelper The {@link ItemTouchHelper2} to trigger drags on.
     * @return A new {@link RecyclerView.OnItemTouchListener} instance.
     */
    public RecyclerView.OnItemTouchListener createMouseDragDetector(
            ItemTouchHelper2 itemTouchHelper) {
        return new RecyclerView.SimpleOnItemTouchListener() {
            private float mStartX;
            private float mStartY;
            private RecyclerView.@Nullable ViewHolder mActiveViewHolder;
            private boolean mTrackingMouseDrag;

            @Override
            public boolean onInterceptTouchEvent(RecyclerView rv, MotionEvent e) {
                if (!e.isFromSource(InputDevice.SOURCE_MOUSE)) {
                    return false;
                }
                int action = e.getActionMasked();
                switch (action) {
                    case MotionEvent.ACTION_DOWN:
                        // First frame that mouse was pressed.
                        // Reset state.
                        mTrackingMouseDrag = false;
                        mActiveViewHolder = null;

                        // Only respond to the primary button (left click) for selection and
                        // dragging.
                        if (e.getButtonState() != MotionEvent.BUTTON_PRIMARY) {
                            return false;
                        }
                        View child = rv.findChildViewUnder(e.getX(), e.getY());
                        if (child != null) {
                            // Check if click was on action button (close button)
                            View actionButton = child.findViewById(R.id.action_button);
                            if (actionButton != null
                                    && actionButton.getVisibility() == View.VISIBLE) {
                                int[] buttonPos = new int[2];
                                actionButton.getLocationInWindow(buttonPos);
                                int[] rvPos = new int[2];
                                rv.getLocationInWindow(rvPos);

                                float relativeX = e.getX() - (buttonPos[0] - rvPos[0]);
                                float relativeY = e.getY() - (buttonPos[1] - rvPos[1]);

                                if (relativeX >= 0
                                        && relativeX < actionButton.getWidth()
                                        && relativeY >= 0
                                        && relativeY < actionButton.getHeight()) {
                                    // Clicked on close button, don't drag.
                                    return false;
                                }
                            }

                            // NOTE: getChildViewHolder() can return Tab Group Headers
                            // (UiType.TAB_GROUP). Headers don't have their own tab ID; they use a
                            // child's tab ID to represent themselves. setIndex() could end up
                            // switching the active web page when a user just clicks a header. If we
                            // observe this happening, we should filter this out for group headers.
                            mActiveViewHolder = rv.getChildViewHolder(child);
                            if (mActiveViewHolder != null) {
                                mStartX = e.getX();
                                mStartY = e.getY();
                                mTrackingMouseDrag = true;

                                // Select the tab immediately, unless the user is holding Ctrl/Shift
                                // to perform a multi-select operation (which should be handled by
                                // onClick).
                                MotionEventInfo info = MotionEventInfo.fromMotionEvent(e);
                                if (!info.hasCtrlOrMeta() && !info.hasShift()) {
                                    selectTab(mActiveViewHolder, TabSelectionType.FROM_USER);
                                }
                            }
                        }
                        break;
                    case MotionEvent.ACTION_MOVE:
                        // Track a drag.
                        if (mTrackingMouseDrag && mActiveViewHolder != null) {
                            float dx = e.getX() - mStartX;
                            float dy = e.getY() - mStartY;
                            float distanceSquared = dx * dx + dy * dy;
                            if (distanceSquared > mMouseDragThresholdSquared) {
                                itemTouchHelper.startDrag(mActiveViewHolder);
                                mTrackingMouseDrag = false;
                                mActiveViewHolder = null;
                                if (mOnDragStartCallback != null) {
                                    mOnDragStartCallback.run();
                                }
                            }
                        }
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        // Mouse was released.
                        mTrackingMouseDrag = false;
                        mActiveViewHolder = null;
                        break;
                }
                return false;
            }
        };
    }

    /**
     * Starts throttling undo group snackbars if a throttle is configured and not already
     * throttling.
     */
    private void startThrottling() {
        if (mUndoBarThrottle != null && mUndoBarThrottleToken == TokenHolder.INVALID_TOKEN) {
            mUndoBarThrottleToken = mUndoBarThrottle.startThrottling();
        }
    }

    /**
     * Stops throttling undo group snackbars and resets the throttle token if throttling is active.
     */
    private void stopThrottling() {
        if (mUndoBarThrottle != null && mUndoBarThrottleToken != TokenHolder.INVALID_TOKEN) {
            mUndoBarThrottle.stopThrottling(mUndoBarThrottleToken);
            mUndoBarThrottleToken = TokenHolder.INVALID_TOKEN;
        }
    }

    /**
     * Evaluates the drag outcome by diffing the drop state against the snapshot from drag start.
     */
    private @DragDropResult int computeDragDropResult(RecyclerView.ViewHolder viewHolder) {
        if (mIsOSNewWindowDrop
                || viewHolder.getBindingAdapterPosition() == RecyclerView.NO_POSITION) {
            return DragDropResult.DRAGGED_OUT;
        }

        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return DragDropResult.ABORTED_NO_CHANGE;

        Tab currentTab = tabModel.getTabById(mDragStartTabId);
        if (currentTab == null) {
            return DragDropResult.DRAGGED_OUT;
        }

        Token currentGroupId = currentTab.getTabGroupId();
        int currentModelIndex = tabModel.indexOf(currentTab);

        if (currentGroupId != null && !Objects.equals(mDragStartGroupId, currentGroupId)) {
            return DragDropResult.GROUPED;
        }
        if (mDragStartGroupId != null && currentGroupId == null) {
            return DragDropResult.UNGROUPED;
        }
        if (currentModelIndex != mDragStartTabModelIndex) {
            return DragDropResult.REORDERED;
        }
        return DragDropResult.ABORTED_NO_CHANGE;
    }

    private void pruneExpiredGroupTimestamps(long now) {
        for (int i = mGroupedTabTimestamps.size() - 1; i >= 0; i--) {
            if (now - mGroupedTabTimestamps.valueAt(i) > MAX_UNGROUP_TRACKING_DURATION_MS) {
                mGroupedTabTimestamps.removeAt(i);
            }
        }
    }

    /** Sets the tab grid item long press orchestrator for testing. */
    void setTabGridItemLongPressOrchestratorForTesting(
            TabGridItemLongPressOrchestrator orchestrator) {
        mTabGridItemLongPressOrchestrator = orchestrator;
    }

    /** Returns the drag out listener for testing. */
    @Nullable OnDragOutListener getOnDragOutListenerForTesting() {
        return mOnDragOutListener;
    }

    /** Returns the drag start callback for testing. */
    @Nullable Runnable getOnDragStartCallbackForTesting() {
        return mOnDragStartCallback;
    }

    /** Returns the long-press DP cancellation threshold for testing. */
    float getLongPressDpCancelThresholdForTesting() {
        return mLongPressDpCancelThreshold;
    }
}
