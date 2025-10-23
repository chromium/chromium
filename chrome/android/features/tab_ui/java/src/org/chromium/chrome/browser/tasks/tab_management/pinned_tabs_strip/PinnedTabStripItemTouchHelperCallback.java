// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Canvas;
import android.view.ViewConfiguration;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;

import java.util.function.Supplier;

/**
 * A {@link ItemTouchHelper2.SimpleCallback} implementation to host the logic for long press related
 * actions in the pinned tab strip. In future, drag to move operations will also be supported using
 * this callback.
 */
@NullMarked
public class PinnedTabStripItemTouchHelperCallback extends ItemTouchHelper2.SimpleCallback {
    private static final long LONGPRESS_DURATION_MS = ViewConfiguration.getLongPressTimeout();
    private final TabGridItemLongPressOrchestrator mTabGridItemLongPressOrchestrator;
    private final TabListModel mModel;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;

    /**
     * @param model The model representing the data in the RecyclerView.
     * @param recyclerViewSupplier Supplies the {@link RecyclerView} whose items are being observed
     *     for long presses.
     * @param onLongPress The listener to be notified when a long press is detected.
     */
    public PinnedTabStripItemTouchHelperCallback(
            Context context,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilter,
            TabListModel model,
            Supplier<RecyclerView> recyclerViewSupplier,
            OnLongPressTabItemEventListener onLongPress) {
        super(/* dragDirs= */ 0, /* swipeDirs= */ 0);
        int longPressDpThreshold =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.tab_list_editor_longpress_entry_threshold);

        mCurrentTabGroupModelFilterSupplier = tabGroupModelFilter;
        mModel = model;
        mTabGridItemLongPressOrchestrator =
                new TabGridItemLongPressOrchestrator(
                        recyclerViewSupplier,
                        model,
                        onLongPress,
                        longPressDpThreshold,
                        LONGPRESS_DURATION_MS);
    }

    @Override
    public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        return makeMovementFlags(ItemTouchHelper.START | ItemTouchHelper.END, 0);
    }

    @Override
    public boolean onMove(
            RecyclerView recyclerView,
            RecyclerView.ViewHolder fromViewHolder,
            RecyclerView.ViewHolder toViewHolder) {
        @TabId
        int currentTabId =
                assumeNonNull(((SimpleRecyclerViewAdapter.ViewHolder) fromViewHolder).model)
                        .get(TabProperties.TAB_ID);

        int destinationIndex = toViewHolder.getBindingAdapterPosition();
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        if (filter == null) return false;

        filter.moveRelatedTabs(currentTabId, destinationIndex);
        mModel.move(fromViewHolder.getBindingAdapterPosition(), destinationIndex);
        return true;
    }

    @Override
    public void onSwiped(RecyclerView.ViewHolder viewHolder, int direction) {}

    @Override
    public void onSelectedChanged(RecyclerView.@Nullable ViewHolder viewHolder, int actionState) {
        super.onSelectedChanged(viewHolder, actionState);
        if (viewHolder == null) return;
        mTabGridItemLongPressOrchestrator.onSelectedChanged(
                viewHolder.getBindingAdapterPosition(), actionState);
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
        mTabGridItemLongPressOrchestrator.processChildDisplacement(dX * dX + dY * dY);
    }

    @Override
    public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
        super.clearView(recyclerView, viewHolder);
        mTabGridItemLongPressOrchestrator.cancel();
    }
}
