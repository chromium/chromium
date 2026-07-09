// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator;
import org.chromium.chrome.browser.tasks.tab_management.TabListItemTouchHelperCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.function.Supplier;

/**
 * A {@link TabListItemTouchHelperCallback} implementation to host the logic for swipe and drag
 * related actions in vertical tab list layout.
 */
@NullMarked
public class VerticalTabListItemTouchHelperCallback extends TabListItemTouchHelperCallback {
    private static final long CONTEXT_MENU_ORCHESTRATOR_DELAY_MS = 10L;
    private final int mMouseDragThresholdSquared;
    private final Set<Integer> mDraggedChildTabIds = new HashSet<>();
    private final List<Integer> mSelectedGroupTabIds = new ArrayList<>();
    private RecyclerView.@Nullable ViewHolder mSelectedViewHolder;

    /**
     * @param context The Android context.
     * @param model The {@link TabListModel} for the tab list.
     * @param currentTabModelSupplier Supplier for the current {@link TabModel}.
     */
    public VerticalTabListItemTouchHelperCallback(
            Context context, TabListModel model, Supplier<TabModel> currentTabModelSupplier) {
        super(context, model, currentTabModelSupplier);
        int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop() / 4;
        mMouseDragThresholdSquared = touchSlop * touchSlop;
    }

    /**
     * Returns the movement flags for the given view holder. Regular and child tabs can move
     * vertically, while pinned tabs can move horizontally as well.
     */
    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        if (!hasTabPropertiesModel(viewHolder)) return 0;

        // Regular and child tabs can move vertically.
        int dragFlags = ItemTouchHelper.UP | ItemTouchHelper.DOWN;
        // Pinned tabs can also move horizontally.
        if (isPinnedRegularTab(viewHolder)) {
            dragFlags |= ItemTouchHelper.LEFT | ItemTouchHelper.RIGHT;
        }
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
        Rect selectedBounds = new Rect();
        getBoundingBox(selected, selectedBounds);
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

        int distance =
                toViewHolder.getBindingAdapterPosition()
                        - fromViewHolder.getBindingAdapterPosition();

        if (!isStandaloneTab && !isGroup) {
            // This is a non-solitary child tab.
            Token destGroupId = getTabGroupId(toViewHolder);
            boolean isDestGroupHeader =
                    toViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP;

            if (!Objects.equals(currentGroupId, destGroupId)
                    || (isDestGroupHeader && Objects.equals(currentGroupId, destGroupId))) {
                boolean trailing = distance > 0;
                Tab currentTab = tabModel.getTabById(currentTabId);
                if (currentTab != null) {
                    tabModel.getTabUngrouper().ungroupTabs(List.of(currentTab), trailing, false);
                }
                return true;
            }
        }

        if (isStandaloneTab) {
            // Intercept swaps between a standalone tab and a tab group.
            Token destGroupId = getTabGroupId(toViewHolder);
            if (destGroupId != null) {
                boolean isDestGroupHeader =
                        toViewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP;
                boolean isDestGroupCollapsed =
                        isDestGroupHeader
                                && assumeNonNull(((ViewHolder) toViewHolder).model)
                                        .get(TabProperties.IS_COLLAPSED);

                if (!isDestGroupCollapsed) {
                    boolean isDraggingDown = distance > 0;

                    Tab currentTab = tabModel.getTabById(currentTabId);
                    Tab destinationTab = tabModel.getTabById(destinationTabId);

                    if (currentTab != null && destinationTab != null) {
                        // Handle grouping when a standalone tab intersects any part of a group.
                        Integer indexInGroup = 0;
                        if (!isDestGroupHeader) {
                            List<Tab> destRelatedTabs = getRelatedTabsForId(destinationTabId);
                            if (destRelatedTabs != null) {
                                boolean isDraggingUp = distance < 0;
                                boolean isTargetLowestTab =
                                        destRelatedTabs.get(destRelatedTabs.size() - 1).getId()
                                                == destinationTabId;
                                if (isDraggingUp && isTargetLowestTab) {
                                    indexInGroup = null;
                                } else {
                                    indexInGroup = destRelatedTabs.indexOf(destinationTab);
                                    if (isDraggingDown) {
                                        indexInGroup++;
                                    }
                                }
                            }
                        }
                        tabModel.mergeListOfTabsToGroup(
                                List.of(currentTab),
                                destinationTab,
                                indexInGroup,
                                TabGroupMergeNotificationType.NOTIFY_ALWAYS);
                        return true;
                    }
                }
            }
        }

        int destinationIndex;
        boolean isTraversingGroup =
                isGroup || (isStandaloneTab && getTabGroupId(toViewHolder) != null);
        if (isTraversingGroup) {
            // Tab groups should maintain the boundaries of target tab groups
            // so they do not split other groups during drags.
            List<Tab> destinationTabGroup = getRelatedTabsForId(destinationTabId);
            destinationIndex =
                    distance >= 0
                            ? TabGroupUtils.getLastTabModelIndexForList(
                                    tabModel, destinationTabGroup)
                            : TabGroupUtils.getFirstTabModelIndexForList(
                                    tabModel, destinationTabGroup);
        } else {
            //  - Child tabs should reorder inside tab groups.
            //  - Standalone tabs use this logic too, but only when not intersecting with a group.
            Tab destinationTab = tabModel.getTabById(destinationTabId);
            destinationIndex =
                    destinationTab != null
                            ? tabModel.indexOf(destinationTab)
                            : TabModel.INVALID_TAB_INDEX;
        }

        if (destinationIndex == TabModel.INVALID_TAB_INDEX) return false;

        destinationIndex = adjustIndexBasedOnPinning(tabModel, currentTabId, destinationIndex);

        // Track the current UI position to correctly clean up visual selection on drop
        mSelectedTabIndex = toViewHolder.getBindingAdapterPosition();

        // Perform basic list reordering by updating the TabModel immediately.
        // - Group headers use moveRelatedTabs() to fire didMoveTabGroup(),
        //   which TabListMediator observes to update top-level UI rows.
        // - Child tabs use moveTab() because they move within their group, firing
        //   didMoveWithinGroup() which TabListMediator observes.
        // - Standalone tabs use moveTab() since they are single elements.

        if (isGroup) {
            tabModel.moveRelatedTabs(currentTabId, destinationIndex);
        } else {
            tabModel.moveTab(currentTabId, destinationIndex);
        }
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
            mSelectedViewHolder = viewHolder;
            mSelectedGroupTabIds.clear();
            if (!hasTabPropertiesModel(viewHolder)) return;
            assumeNonNull(viewHolder);
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
                selectTab(viewHolder);
            }
        } else if (actionState == ItemTouchHelper.ACTION_STATE_IDLE) {
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
        super.onChildDraw(c, recyclerView, viewHolder, dX, dY, actionState, isCurrentlyActive);
        if (mTabGridItemLongPressOrchestrator != null && !mIsMouseInputSource) {
            float displacementSquared = calcMagnitudeSquared(dX, dY);
            mTabGridItemLongPressOrchestrator.processChildDisplacement(displacementSquared);
        }

        if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
            if (!hasTabPropertiesModel(viewHolder)) return;
            setDraggingY(viewHolder, isCurrentlyActive ? dY : null);

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

                        // Copy the translation/elevation from the dragged group header
                        // to the children.
                        if (recyclerView.getItemAnimator() != null) {
                            recyclerView.getItemAnimator().endAnimation(childViewHolder);
                        }
                        childView.setTranslationY(dY);
                        childView.setTranslationX(dX);

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
                            childView.setTranslationX(0f);
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
        // Safeguard finger lift: explicitly wipe out any running timer threads.
        if (mTabGridItemLongPressOrchestrator != null) {
            mTabGridItemLongPressOrchestrator.cancel();
        }
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
                            childView.setTranslationX(0f);
                        }
                    }
                }
            }
        }
        mDraggedChildTabIds.clear();
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

        boolean isFirstInGroup = currentTab.getId() == relatedTabs.get(0).getId();
        boolean isLastInGroup =
                currentTab.getId() == relatedTabs.get(relatedTabs.size() - 1).getId();

        if (dy > 0 && isLastInGroup) {
            // Dragging down does not require crossing the group header, so it uses a smaller
            // threshold.
            int downThreshold = viewHolder.itemView.getHeight() / 4;
            if (y > viewHolder.itemView.getTop() + downThreshold) {
                tabModel.getTabUngrouper().ungroupTabs(List.of(currentTab), true, false);
                return true;
            }
        } else if (dy < 0 && isFirstInGroup) {
            // Dragging up requires crossing the group header which sits above the first tab.
            int upThreshold = viewHolder.itemView.getHeight() / 2;
            if (y < viewHolder.itemView.getTop() - upThreshold) {
                tabModel.getTabUngrouper().ungroupTabs(List.of(currentTab), false, false);
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
        if (viewHolder.getItemViewType() == TabProperties.UiType.TAB) {
            Token groupId = getTabGroupId(viewHolder);
            if (groupId != null) {
                int tabId = getTabId(viewHolder);
                List<Tab> relatedTabs = getRelatedTabsForId(tabId);
                return relatedTabs != null && relatedTabs.size() == 1;
            }
        }
        return false;
    }

    private @Nullable Token getTabGroupId(RecyclerView.ViewHolder viewHolder) {
        if (viewHolder instanceof ViewHolder simpleViewHolder) {
            PropertyModel model = simpleViewHolder.model;
            if (model != null) {
                Token headerId = model.get(TabProperties.TAB_GROUP_HEADER_ID);
                if (headerId != null) return headerId;
                return model.get(TabProperties.TAB_GROUP_ID);
            }
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

    private void selectTab(RecyclerView.ViewHolder viewHolder) {
        if (viewHolder.getItemViewType() == TabProperties.UiType.TAB_GROUP) {
            return;
        }
        TabModel tabModel = mCurrentTabModelSupplier.get();
        if (tabModel == null) return;

        int tabId = getTabId(viewHolder);
        Tab tab = tabModel.getTabById(tabId);
        selectTabInternal(tabModel, tab, TabSelectionType.FROM_USER);
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

                                // Select the tab immediately.
                                selectTab(mActiveViewHolder);
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

    void setTabGridItemLongPressOrchestratorForTesting(
            TabGridItemLongPressOrchestrator orchestrator) {
        mTabGridItemLongPressOrchestrator = orchestrator;
    }
}
