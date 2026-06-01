// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import android.content.Context;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnItemTouchListener;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Shared base touch helper callback containing common fields and helper checks for tab list
 * layouts.
 */
@NullMarked
public abstract class TabListItemTouchHelperCallback extends ItemTouchHelper2.SimpleCallback {
    protected static final long LONGPRESS_DURATION_MS = ViewConfiguration.getLongPressTimeout();

    protected final TabListModel mModel;
    protected final Supplier<TabModel> mCurrentTabModelSupplier;
    protected final SettableMonotonicObservableSupplier<RecyclerView> mRecyclerViewSupplier =
            ObservableSuppliers.createMonotonic();
    protected final float mLongPressDpCancelThreshold;

    // A bool to track whether an action such as swiping, group/ungroup and drag past a certain
    // threshold was attempted. This can determine if a longpress on the tab is the objective.
    protected boolean mActionAttempted;
    // A bool to track whether any action that is not a pure longpress hold-no-drag, was started.
    // This can determine if an unwanted following click from a pure longpress must be blocked.
    protected boolean mActionStarted;
    // A bool indicating whether the next release/up touch event should be blocked/consumed.
    protected boolean mShouldBlockAction;
    protected boolean mIsMouseInputSource;

    protected int mSelectedTabIndex = TabModel.INVALID_TAB_INDEX;
    protected int mCurrentActionState = ItemTouchHelper.ACTION_STATE_IDLE;

    // Orchestrates long-press vs drag timings for touch events to trigger context menus.
    protected @Nullable TabGridItemLongPressOrchestrator mTabGridItemLongPressOrchestrator;

    /**
     * Constructs a new base touch helper callback.
     *
     * @param context The Android Context used to load dimension resources.
     * @param model The model representing the list of tabs.
     * @param currentTabModelSupplier The supplier providing the active TabModel.
     */
    public TabListItemTouchHelperCallback(
            Context context, TabListModel model, Supplier<TabModel> currentTabModelSupplier) {
        super(0, 0);
        mModel = model;
        mCurrentTabModelSupplier = currentTabModelSupplier;
        mLongPressDpCancelThreshold =
                context.getResources().getDimensionPixelSize(R.dimen.long_press_cancel_threshold);
    }

    /**
     * Calculates the squared Euclidean distance of pointer travel.
     *
     * @param dX Displacement along the x-axis.
     * @param dY Displacement along the y-axis.
     * @return The squared magnitude of displacement.
     */
    protected static float calcMagnitudeSquared(float dX, float dY) {
        return dX * dX + dY * dY;
    }

    /**
     * Returns whether a touch action should be blocked. The state is defaulted to false and reset
     * to false after shouldBlockAction() is called (acts as a single-use consumer). This prevents
     * secondary click/touch events from firing immediately following a completed gesture.
     *
     * @return True if the next click event should be blocked; false otherwise.
     */
    public boolean shouldBlockAction() {
        boolean out = mShouldBlockAction;
        mShouldBlockAction = false;
        return out;
    }

    /**
     * Sets whether the active touch/drag input source is a mouse pointer. Used to suppress
     * finger-touch specific behaviors such as auto-scrolling.
     *
     * @param isMouseInputSource True if input is from a mouse; false otherwise.
     */
    public void setIsMouseInputSource(boolean isMouseInputSource) {
        mIsMouseInputSource = isMouseInputSource;
    }

    /**
     * Creates the pre-helper touch listener that configures the mouse input source state.
     *
     * @param callback The touch helper callback instance to notify.
     * @return A new OnItemTouchListener instance.
     */
    public static OnItemTouchListener createBeforeOnItemTouchListener(
            TabListItemTouchHelperCallback callback) {
        return new OnItemTouchListener() {
            @Override
            public boolean onInterceptTouchEvent(RecyclerView recyclerView, MotionEvent event) {
                // Detects if inputs are coming from a mouse or not. This is used to modify
                // behaviors of the TabGridItemTouchHelperCallback.
                callback.setIsMouseInputSource(event.getSource() == InputDevice.SOURCE_MOUSE);
                return false;
            }

            @Override
            public void onTouchEvent(RecyclerView recyclerView, MotionEvent event) {}

            @Override
            public void onRequestDisallowInterceptTouchEvent(boolean disallowIntercept) {}
        };
    }

    /**
     * Creates the post-helper touch listener that intercepts and consumes ACTION_UP release events
     * when a touch block action is actively requested by the touch helper callback.
     *
     * @param callback The touch helper callback instance to query.
     * @return A new OnItemTouchListener instance.
     */
    public static OnItemTouchListener createAfterOnItemTouchListener(
            TabListItemTouchHelperCallback callback) {
        return new OnItemTouchListener() {
            @Override
            public boolean onInterceptTouchEvent(RecyclerView recyclerView, MotionEvent event) {
                // There can be an edge case when adding the block action logic where minimal
                // movement not picked up by the mItemTouchHelper can result in attempting to block
                // an action that did have a DRAG event. Actually, blocking the next event in this
                // can result in an unexpected event being consumed leading to an unexpected
                // sequence of MotionEvents. This bad sequence can then result in invalid UI &
                // click state for downstream touch handlers. This additional check ensures that for
                // a given action, if a block is requested it must be the UP motion that ends the
                // input.
                if (callback.shouldBlockAction()
                        && (event.getActionMasked() == MotionEvent.ACTION_UP
                                || event.getActionMasked() == MotionEvent.ACTION_POINTER_UP)) {
                    return true;
                }
                return false;
            }

            @Override
            public void onTouchEvent(RecyclerView recyclerView, MotionEvent event) {}

            @Override
            public void onRequestDisallowInterceptTouchEvent(boolean disallowIntercept) {
                // If a child component does not allow this recyclerView and any
                // parent components to intercept touch events, shouldBlockAction
                // should be called anyways to reset the tracking boolean.
                // Otherwise, the original intercept method will do the check.
                if (!disallowIntercept) return;
                callback.shouldBlockAction();
            }
        };
    }

    /**
     * Checks if a given ViewHolder is backed by a Tab model type rather than a message card.
     *
     * @param viewHolder The card ViewHolder to inspect.
     * @return True if the card represents a tab; false otherwise.
     */
    protected boolean hasTabPropertiesModel(RecyclerView.@Nullable ViewHolder viewHolder) {
        if (viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder) {
            PropertyModel model = simpleViewHolder.model;
            assumeNonNull(model);
            return model.get(CARD_TYPE) == TAB;
        }
        return false;
    }

    /**
     * Checks if a given ViewHolder represents a pinned regular tab card. Pinned tabs are placed at
     * the top of the tab list and are bound by strict reordering boundaries.
     *
     * @param viewHolder The card ViewHolder to inspect.
     * @return True if the card represents a pinned tab; false otherwise.
     */
    protected boolean isPinnedRegularTab(RecyclerView.@Nullable ViewHolder viewHolder) {
        if (viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder) {
            PropertyModel model = simpleViewHolder.model;
            if (model != null && model.get(CARD_TYPE) == TAB) {
                return model.get(TabProperties.IS_PINNED);
            }
        }
        return false;
    }

    /**
     * Checks if a given ViewHolder represents a collaborative tab group.
     *
     * @param viewHolder The card ViewHolder to inspect.
     * @return True if the card represents a collaborative tab group; false otherwise.
     */
    protected boolean hasCollaboration(RecyclerView.@Nullable ViewHolder viewHolder) {
        if (viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder) {
            PropertyModel model = simpleViewHolder.model;
            assumeNonNull(model);
            if (model.get(CARD_TYPE) == TAB) {
                @Nullable TabGroupColorViewProvider provider =
                        model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
                return provider != null && provider.hasCollaborationId();
            }
        }
        return false;
    }

    /**
     * Returns the list of all tabs in the same group as the given tab ID.
     *
     * @param id The ID of the representative tab.
     * @return The list of related tabs.
     */
    protected List<Tab> getRelatedTabsForId(int id) {
        TabModel tabModel = mCurrentTabModelSupplier.get();
        return tabModel == null ? new ArrayList<>() : tabModel.getRelatedTabList(id);
    }

    /**
     * Intercepts mouse-input sources to cancel default auto-scroll animations during drag moves.
     */
    @Override
    public void onMoved(
            final RecyclerView recyclerView,
            final RecyclerView.ViewHolder viewHolder,
            int fromPos,
            final RecyclerView.ViewHolder target,
            int toPos,
            int x,
            int y) {
        // If this is a mouse input we don't want to force the auto-scroll behavior that happens
        // inside the default super implementation. Early returning here will just cancel the drag.
        if (mIsMouseInputSource) return;
        super.onMoved(recyclerView, viewHolder, fromPos, target, toPos, x, y);
    }

    /**
     * Calculates out-of-bounds scroll speeds during drag reordering. Suppresses all auto-scroll
     * speed interpolation when input is from a mouse source.
     *
     * @param recyclerView The active RecyclerView container.
     * @param viewSize The width or height of the scrollable view depending on orientation.
     * @param viewSizeOutOfBounds The amount of pixels dragged out of the bounds.
     * @param totalSize The total scrollable range size.
     * @param msSinceStartScroll Elapsed time since the scroll operation was initiated.
     * @return The calculated scroll distance increment; 0 if mouse-input is active.
     */
    @Override
    public int interpolateOutOfBoundsScroll(
            RecyclerView recyclerView,
            int viewSize,
            int viewSizeOutOfBounds,
            int totalSize,
            long msSinceStartScroll) {
        if (mIsMouseInputSource) return 0;

        return super.interpolateOutOfBoundsScroll(
                recyclerView, viewSize, viewSizeOutOfBounds, totalSize, msSinceStartScroll);
    }

    /**
     * Safe boundary adjustment to ensure pinned and non-pinned tabs never mix in the model. If the
     * tab being moved is pinned, ensures it doesn't move past the last pinned tab index. If it is
     * non-pinned, ensures it doesn't move before the first non-pinned tab index.
     *
     * @param tabModel The active TabModel instance.
     * @param fromTabId The ID of the tab being moved.
     * @param newIndex The proposed destination index.
     * @return The adjusted destination index respecting pinning boundaries.
     */
    protected int adjustIndexBasedOnPinning(TabModel tabModel, int fromTabId, int newIndex) {
        // Get the tab being moved.
        Tab fromTab = tabModel.getTabById(fromTabId);
        if (fromTab != null) {

            // Determine the index of the last pinned tab.
            int lastPinnedIndex = tabModel.findFirstNonPinnedTabIndex() - 1;

            if (fromTab.getIsPinned()) {
                // If the moved tab is pinned, ensure it doesn't move beyond the last pinned index.
                if (newIndex > lastPinnedIndex) {
                    newIndex = lastPinnedIndex;
                }
            } else {
                // If the moved tab is not pinned, ensure it doesn't move before the first
                // non-pinned index.
                if (newIndex <= lastPinnedIndex) {
                    newIndex = lastPinnedIndex + 1;
                }
            }
        }
        return newIndex;
    }

    /**
     * Sets the listener for long-press actions to trigger context menus.
     *
     * @param listener the handler for longpress actions.
     */
    void setOnLongPressTabItemEventListener(
            TabGridItemLongPressOrchestrator.@Nullable OnLongPressTabItemEventListener listener) {
        assert mTabGridItemLongPressOrchestrator == null;
        if (listener != null) {
            setTabGridItemLongPressOrchestrator(
                    new TabGridItemLongPressOrchestrator(
                            mRecyclerViewSupplier,
                            mModel,
                            listener,
                            mLongPressDpCancelThreshold,
                            LONGPRESS_DURATION_MS));
        }
    }

    @VisibleForTesting
    void setTabGridItemLongPressOrchestrator(TabGridItemLongPressOrchestrator orchestrator) {
        mTabGridItemLongPressOrchestrator = orchestrator;
    }

    void setSelectedTabIndexForTesting(int index) {
        var oldValue = mSelectedTabIndex;
        mSelectedTabIndex = index;
        ResettersForTesting.register(() -> mSelectedTabIndex = oldValue);
    }

    void setCurrentActionStateForTesting(int actionState) {
        var oldValue = mCurrentActionState;
        mCurrentActionState = actionState;
        ResettersForTesting.register(() -> mCurrentActionState = oldValue);
    }

    public void setShouldBlockActionForTesting(boolean block) {
        var oldValue = mShouldBlockAction;
        mShouldBlockAction = block;
        ResettersForTesting.register(() -> mShouldBlockAction = oldValue);
    }
}
