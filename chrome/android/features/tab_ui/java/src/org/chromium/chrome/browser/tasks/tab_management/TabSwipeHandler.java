// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.view.HapticFeedbackConstants;
import android.view.View;

import androidx.annotation.Px;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Handles swipe-to-dismiss gestures for tab management surfaces such as the Tab Grid. */
@NullMarked
public final class TabSwipeHandler {
    private static final String SWIPE_CLOSE_USER_ACTION_PREFIX = "MobileStackViewSwipeCloseTab.";
    // Matches ItemTouchHelper.Callback's default swipe threshold.
    private static final float DEFAULT_SWIPE_THRESHOLD_FRACTION = 0.5f;
    private static final float MIN_SWIPE_CARD_ALPHA = 0.2f;
    private static final float MAX_SWIPE_CARD_ALPHA_DECAY = 1f - MIN_SWIPE_CARD_ALPHA;

    private final TabListModel mModel;
    private final TabActionListener mTabClosedListener;
    private final String mComponentName;
    private final SettableNonNullObservableSupplier<Integer> mRecentlySwipedTabIdSupplier =
            ObservableSuppliers.createNonNull(Tab.INVALID_TAB_ID);

    private @Px float mSwipeToDismissThresholdPx;
    private boolean mIsSwipingToDismiss;

    /**
     * Constructs a {@link TabSwipeHandler} to manage swipe-to-dismiss gestures.
     *
     * @param tabListModel The {@link TabListModel} containing the tabs and cards.
     * @param tabClosedListener The {@link TabActionListener} to invoke when a tab or group is
     *     swiped.
     * @param componentName The component name used for user action recording.
     */
    public TabSwipeHandler(
            TabListModel tabListModel, TabActionListener tabClosedListener, String componentName) {
        mModel = tabListModel;
        mTabClosedListener = tabClosedListener;
        mComponentName = componentName;
    }

    /**
     * Sets the threshold distance required to dismiss a card on swipe.
     *
     * @param swipeToDismissThresholdPx The threshold distance in pixels.
     */
    public void setSwipeToDismissThresholdPx(@Px float swipeToDismissThresholdPx) {
        mSwipeToDismissThresholdPx = swipeToDismissThresholdPx;
    }

    /**
     * Returns an observable supplier for the ID of the most recently swiped tab.
     *
     * @return A {@link NonNullObservableSupplier} with the recent tab ID.
     */
    public NonNullObservableSupplier<Integer> getRecentlySwipedTabIdSupplier() {
        return mRecentlySwipedTabIdSupplier;
    }

    /**
     * Returns the swipe movement flags for a given {@link RecyclerView.ViewHolder}.
     *
     * @param viewHolder The {@link RecyclerView.ViewHolder} being queried.
     * @param isMouseInputSource {@code true} if input is from a mouse, {@code false} otherwise.
     * @return Movement flags for swiping (e.g. {@code START | END}) or {@code 0} if not swipeable.
     */
    public int getSwipeFlags(RecyclerView.ViewHolder viewHolder, boolean isMouseInputSource) {
        return isSwipeable(viewHolder, isMouseInputSource)
                ? (ItemTouchHelper.START | ItemTouchHelper.END)
                : 0;
    }

    private boolean isSwipeable(RecyclerView.ViewHolder viewHolder, boolean isMouseInputSource) {
        if (isMouseInputSource) {
            return false;
        }

        // The archived tabs message can't be dismissed.
        if (viewHolder.getItemViewType() == UiType.ARCHIVED_TABS_MESSAGE) {
            return false;
        }

        if (!(viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder)) {
            return false;
        }

        PropertyModel model = simpleViewHolder.model;
        if (model == null) {
            return false;
        }

        // Pinned tabs can't be dismissed.
        if (TabProperties.isPinnedTab(model)) {
            return false;
        }

        // Only cards that onSwiped() can dismiss are swipeable. Saved tab groups have no tab ID.
        return model.containsKey(TabProperties.TAB_ID) || model.get(CARD_TYPE) == MESSAGE;
    }

    /**
     * Returns the swipe dismissal threshold as a fraction of the parent view's width.
     *
     * @param viewHolder The {@link RecyclerView.ViewHolder} being swiped.
     * @return The swipe threshold fraction, or {@code 0.5f} when parent width is unavailable.
     */
    public float getSwipeThreshold(RecyclerView.ViewHolder viewHolder) {
        assert mSwipeToDismissThresholdPx > 0;
        if (viewHolder.itemView.getParent() instanceof View parentView
                && parentView.getWidth() > 0) {
            return mSwipeToDismissThresholdPx / parentView.getWidth();
        }
        return DEFAULT_SWIPE_THRESHOLD_FRACTION;
    }

    /**
     * Handles drawing updates and haptic feedback during a swipe gesture.
     *
     * @param viewHolder The {@link RecyclerView.ViewHolder} being swiped.
     * @param dX The horizontal displacement of the swipe.
     */
    public void onChildDrawSwipe(RecyclerView.ViewHolder viewHolder, @Px float dX) {
        @Nullable PropertyModel cardModel = resolveCardModel(viewHolder);
        if (cardModel == null) {
            return;
        }

        assert mSwipeToDismissThresholdPx > 0;
        float progress = Math.abs(dX) / mSwipeToDismissThresholdPx;
        float alpha = Math.max(MIN_SWIPE_CARD_ALPHA, 1f - MAX_SWIPE_CARD_ALPHA_DECAY * progress);
        cardModel.set(CARD_ALPHA, alpha);

        boolean isOverSwipeThreshold = Math.abs(dX) >= mSwipeToDismissThresholdPx;
        if (isOverSwipeThreshold && !mIsSwipingToDismiss) {
            viewHolder.itemView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
        }
        mIsSwipingToDismiss = isOverSwipeThreshold;
    }

    /**
     * Handles the completion of a swipe gesture, closing the corresponding tab or group.
     *
     * @param viewHolder The swiped {@link RecyclerView.ViewHolder}.
     */
    public void onSwiped(RecyclerView.ViewHolder viewHolder) {
        SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder =
                (SimpleRecyclerViewAdapter.ViewHolder) viewHolder;

        PropertyModel model = assertNonNull(simpleViewHolder.model);

        if (model.containsKey(TabProperties.TAB_ID)) {
            int tabId = model.get(TabProperties.TAB_ID);
            mRecentlySwipedTabIdSupplier.set(tabId);
            mTabClosedListener.run(viewHolder.itemView, tabId, /* triggeringMotion= */ null);
            RecordUserAction.record(SWIPE_CLOSE_USER_ACTION_PREFIX + mComponentName);
        } else if (model.get(CARD_TYPE) == MESSAGE) {
            // TODO(crbug.com/40099080): Have a caller instead of simulating the close click. And
            // write unit test to verify the caller is called.
            viewHolder.itemView.findViewById(R.id.close_button).performClick();
            // TODO(crbug.com/40099080): UserAction swipe to dismiss.
        } else {
            assert false : "Unsupported swiped card model.";
        }
    }

    /** Resets threshold haptic tracking. Call when the {@link ItemTouchHelper} gesture ends. */
    public void resetSwipeState() {
        mIsSwipingToDismiss = false;
    }

    private @Nullable PropertyModel resolveCardModel(RecyclerView.ViewHolder viewHolder) {
        SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder =
                (SimpleRecyclerViewAdapter.ViewHolder) viewHolder;

        @Nullable PropertyModel viewHolderModel = simpleViewHolder.model;
        if (viewHolderModel == null) {
            return null;
        }

        if (TabProperties.isTabOrTabGroup(viewHolderModel)) {
            return viewHolderModel;
        } else if (viewHolderModel.containsKey(CARD_TYPE)
                && viewHolderModel.get(CARD_TYPE) == MESSAGE) {
            if (viewHolderModel.containsKey(MESSAGE_TYPE)) {
                int index =
                        mModel.lastIndexForMessageItemFromType(viewHolderModel.get(MESSAGE_TYPE));
                if (index >= 0 && index < mModel.size()) {
                    return mModel.get(index).model;
                }
            }
        }

        return null;
    }
}
