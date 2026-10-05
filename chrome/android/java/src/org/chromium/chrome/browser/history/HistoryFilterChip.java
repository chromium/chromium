// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.Supplier;

/**
 * Manages a single filter chip in the history UI and its associated {@link FilterSheetCoordinator}
 * bottom sheet.
 */
@NullMarked
class HistoryFilterChip {
    private static final int MIN_ITEMS_TO_SHOW = 2;

    private final @IdRes int mChipViewId;
    private final @StringRes int mTitleResId;
    private final boolean mCanShow;
    private final Callback<@Nullable FilterItem> mOnSelectionChanged;
    private final @Nullable Runnable mOnSheetOpened;
    private final List<FilterItem> mItems = new ArrayList<>();

    private @Nullable ChipView mChipView;
    private @Nullable FilterSheetCoordinator mFilterSheet;
    private @Nullable FilterItem mSelectedItem;
    private boolean mEnabled = true;

    HistoryFilterChip(
            @IdRes int chipViewId,
            @StringRes int titleResId,
            boolean canShow,
            Callback<@Nullable FilterItem> onSelectionChanged,
            @Nullable Runnable onSheetOpened) {
        mChipViewId = chipViewId;
        mTitleResId = titleResId;
        mCanShow = canShow;
        mOnSelectionChanged = onSelectionChanged;
        mOnSheetOpened = onSheetOpened;
    }

    /** Binds the {@link ChipView} inside {@code container} and initializes its state. */
    void bindView(
            ViewGroup container,
            Activity activity,
            @Nullable Supplier<BottomSheetController> bottomSheetControllerSupplier,
            @Nullable Runnable hideSoftKeyboard) {
        mChipView = container.findViewById(mChipViewId);
        if (!mCanShow) {
            mChipView.setVisibility(View.GONE);
            return;
        }
        assert bottomSheetControllerSupplier != null;
        assert hideSoftKeyboard != null;
        mChipView.setOnClickListener(
                _ -> openSheet(activity, bottomSheetControllerSupplier, hideSoftKeyboard));
        mChipView.getPrimaryTextView().setText(mTitleResId);
        mChipView.addDropdownIcon();
        setChipEnabled(mEnabled);
        updateChipView();
    }

    /** Replaces the items displayed in the filter sheet. */
    void setItems(List<FilterItem> items) {
        assert mCanShow;
        mItems.clear();
        mItems.addAll(items);
        if (mFilterSheet != null) {
            mFilterSheet.updateItems(mItems);
        }
        updateVisibility();
    }

    /** Callback invoked when the filter sheet closes with a new selection. */
    @VisibleForTesting
    void onItemSelected(@Nullable FilterItem item) {
        if (Objects.equals(mSelectedItem, item)) return;
        mSelectedItem = item;
        updateChipView();
        mOnSelectionChanged.onResult(mSelectedItem);
    }

    /** Clears the current selection and resets the chip view to its default state. */
    void reset() {
        mSelectedItem = null;
        updateChipView();
    }

    /** Enables or disables the chip button. */
    void setChipEnabled(boolean enabled) {
        mEnabled = enabled;
        if (mChipView != null && mCanShow) {
            mChipView.setEnabled(enabled);
        }
    }

    /** Returns whether this filter chip should be visible. */
    boolean isVisible() {
        return mCanShow && (hasItems() || mSelectedItem != null);
    }

    private void openSheet(
            Activity activity,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            Runnable hideSoftKeyboard) {
        // Search mode starts with the soft keyboard open. Hide it first for the sheet
        // to appear at the bottom as expected.
        hideSoftKeyboard.run();
        if (mFilterSheet == null) {
            mFilterSheet =
                    new FilterSheetCoordinator(
                            activity,
                            activity.getWindow().getDecorView(),
                            bottomSheetControllerSupplier.get(),
                            this::onItemSelected,
                            mItems,
                            mTitleResId);
        }
        mFilterSheet.openSheet(mSelectedItem);
        if (mOnSheetOpened != null) {
            mOnSheetOpened.run();
        }
    }

    private boolean hasItems() {
        return mItems.size() >= MIN_ITEMS_TO_SHOW;
    }

    private void updateVisibility() {
        if (mChipView == null) return;
        mChipView.setVisibility(isVisible() ? View.VISIBLE : View.GONE);
    }

    private void updateChipView() {
        if (mChipView == null) return;
        if (mSelectedItem == null) {
            mChipView.getPrimaryTextView().setText(mTitleResId);
            mChipView.setSelected(false);
            mChipView.setIcon(ChipView.INVALID_ICON_ID, false);
        } else {
            mChipView.getPrimaryTextView().setText(mSelectedItem.label);
            mChipView.setSelected(true);
            mChipView.setIcon(R.drawable.ic_check_googblue_24dp, true);
        }
        updateVisibility();
    }

    ChipView getChipViewForTesting() {
        return assumeNonNull(mChipView);
    }

    void setFilterSheetForTesting(FilterSheetCoordinator filterSheet) {
        mFilterSheet = filterSheet;
    }

    @Nullable FilterItem getSelectedItemForTesting() {
        return mSelectedItem;
    }

    List<FilterItem> getItemsForTesting() {
        return mItems;
    }
}
