// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.bottom_sheet_utils.DetailScreenScrollListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;

import java.util.Set;

/** This is a base class for the Touch to Fill View classes. */
public abstract class TouchToFillViewBase implements BottomSheetContent {
    private static final int MAX_FULLY_VISIBLE_CREDENTIAL_COUNT = 3;

    private final BottomSheetController mBottomSheetController;
    private final RelativeLayout mContentView;
    private final DetailScreenScrollListener mScrollListener;
    private Callback<Integer> mDismissHandler;
    private RecyclerView mSheetItemListView;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    assert mDismissHandler != null;
                    mDismissHandler.onResult(reason);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    super.onSheetStateChanged(newState, reason);
                    if (newState == BottomSheetController.SheetState.FULL) {
                        // The list of items should be scrollable in full state.
                        mSheetItemListView.suppressLayout(false);
                    } else if (newState == BottomSheetController.SheetState.HALF
                            && mScrollListener.isScrolledToTop()) {
                        // The list of items should not be scrollable when the sheet transitions
                        // into half state if it's scrolled to the top. If the list is currently
                        // scrolled away from the top, it should stay scrolled in half state until
                        // the user scrolls to the top.
                        mSheetItemListView.suppressLayout(true);
                    }
                    if (newState != BottomSheetController.SheetState.HIDDEN) return;
                    // This is a fail-safe for cases where onSheetClosed isn't triggered.
                    mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    /**
     * Used to access the handlebar to measure it.
     * @return the {@link View} representing the drag handlebar.
     */
    protected abstract View getHandlebar();

    /**
     * Returns the margin between the last item in the scrollable list and the footer.
     * @return the margin size in pixels.
     */
    protected abstract @Px int getConclusiveMarginHeightPx();

    /**
     * Used as a helper to measure the size of the sheet content.
     * @return the side margin of the content view.
     */
    protected abstract @Px int getSideMarginPx();

    /**
     * Used as a helper for the suggestion list height calculation.
     * @return the item types of the suggestions in the list on the {@link BottomSheet}.
     */
    protected abstract Set<Integer> listedItemTypes();

    /**
     * Used as a helper for the suggestion list height calculation.
     * @return the item type of the footer on the {@link BottomSheet}.
     */
    protected abstract int footerItemType();

    /**
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     * @param contentView The content of the bottom sheet.
     * @param suppressCollectionA11y Disables/enables setting the collection related a11y node info,
     *                               basically removing the "2 of 4" part in a regular RecycleView
     *                               item announcement. Setting it to `true` implies that the item
     *                               content description is updated accordingly for items that are
     *                               eligible for indexing from the UI perspective.
     */
    public TouchToFillViewBase(
            BottomSheetController bottomSheetController,
            RelativeLayout contentView,
            Boolean suppressCollectionA11y) {
        mBottomSheetController = bottomSheetController;
        mContentView = contentView;
        mSheetItemListView = getContentView().findViewById(R.id.sheet_item_list);

        mSheetItemListView.setLayoutManager(
                new LinearLayoutManager(
                        mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false) {
                    @Override
                    public boolean isAutoMeasureEnabled() {
                        return true;
                    }

                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            RecyclerView.Recycler recycler,
                            RecyclerView.State state,
                            AccessibilityNodeInfoCompat info) {
                        if (!suppressCollectionA11y) {
                            super.onInitializeAccessibilityNodeInfo(recycler, state, info);
                        }
                    }
                });

        mContentView.setOnGenericMotionListener((v, e) -> true); // Filter background interaction.

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);

        mScrollListener = new DetailScreenScrollListener(mBottomSheetController);
        mSheetItemListView.addOnScrollListener(mScrollListener);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    public void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     *
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @return True if the request was successful, false otherwise
     */
    public boolean setVisible(boolean isVisible) {
        if (isVisible) {
            remeasure();
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(this, true)) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
                return false;
            }
        } else {
            mBottomSheetController.hideContent(this, true);
        }
        return true;
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     *
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    public void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    /**
     * Returns the height of the full state. Must show the footer items permanently. For up to four
     * suggestions, the sheet usually cannot fill the screen.
     *
     * @return the full state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    protected @Px int getMaximumSheetHeightPx() {
        if (mSheetItemListView.getAdapter() == null) {
            // TODO(crbug.com/40843561): Assert this condition in setVisible. Should never happen.
            return BottomSheetContent.HeightMode.DEFAULT;
        }
        @Px int requiredMaxHeight = getHeightWhenFullyExtendedPx();
        if (requiredMaxHeight <= mBottomSheetController.getContainerHeight()) {
            return requiredMaxHeight;
        }
        remeasure();
        ViewUtils.requestLayout(mContentView, "TouchToFillView.getMaximumSheetHeightPx");
        return getHeightWhenFullyExtendedPx();
    }

    /**
     * Returns the height of the half state. Does not show the footer items. For 1 suggestion (plus
     * fill button), 2 or 3 suggestions, it shows all items fully. For 4+ suggestions, it shows the
     * first 3.5 suggestion to encourage scrolling.
     *
     * @return the half state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    protected @Px int getDesiredSheetHeightPx() {
        if (mSheetItemListView.getAdapter() == null) {
            // TODO(crbug.com/40843561): Assert this condition in setVisible. Should never happen.
            return BottomSheetContent.HeightMode.DEFAULT;
        }
        int height =
                getHeightWithMarginsPx(getHandlebar(), false)
                        + getSheetItemListHeightWithMarginsPx(true);
        return height;
    }

    private @Px int getHeightWhenFullyExtendedPx() {
        assert mContentView.getMeasuredHeight() > 0 : "ContentView hasn't been measured.";
        int height =
                getHeightWithMarginsPx(getHandlebar(), false)
                        + getSheetItemListHeightWithMarginsPx(false);
        return height;
    }

    private @Px int getSheetItemListHeightWithMarginsPx(boolean showOnlyInitialItems) {
        assert mSheetItemListView.getMeasuredHeight() > 0 : "Sheet item list hasn't been measured.";
        @Px int totalHeight = 0;
        int visibleItems = 0;
        for (int posInSheet = 0; posInSheet < mSheetItemListView.getChildCount(); posInSheet++) {
            View child = mSheetItemListView.getChildAt(posInSheet);
            if (isListedItem(child)) {
                // Counting how many clickable suggestions are displayed.
                visibleItems++;
            } else if (showOnlyInitialItems && isFooterItem(child)) {
                // If we want to show only the initial items, the footer should remain hidden.
                return totalHeight + getConclusiveMarginHeightPx();
            }
            if (showOnlyInitialItems && visibleItems > MAX_FULLY_VISIBLE_CREDENTIAL_COUNT) {
                // If the current item is the last to be shown, skip remaining elements and margins.
                totalHeight += getHeightWithMarginsPx(child, true);
                return totalHeight;
            }
            totalHeight += getHeightWithMarginsPx(child, false);
        }
        return totalHeight;
    }

    private static @Px int getHeightWithMarginsPx(View view, boolean shouldPeek) {
        assert view.getMeasuredHeight() > 0 : "View hasn't been measured.";
        return getMarginsPx(view, /* excludeBottomMargin= */ shouldPeek)
                + (shouldPeek ? view.getMeasuredHeight() / 2 : view.getMeasuredHeight());
    }

    private static @Px int getMarginsPx(View view, boolean excludeBottomMargin) {
        LayoutParams params = view.getLayoutParams();
        if (params instanceof MarginLayoutParams) {
            MarginLayoutParams marginParams = (MarginLayoutParams) params;
            return marginParams.topMargin + (excludeBottomMargin ? 0 : marginParams.bottomMargin);
        }
        return 0;
    }

    /** Measures the content of the bottom sheet. */
    protected void remeasure() {
        mContentView.measure(
                View.MeasureSpec.makeMeasureSpec(getInsetDisplayWidthPx(), MeasureSpec.AT_MOST),
                MeasureSpec.UNSPECIFIED);
        mSheetItemListView.measure(
                View.MeasureSpec.makeMeasureSpec(getInsetDisplayWidthPx(), MeasureSpec.AT_MOST),
                MeasureSpec.UNSPECIFIED);
    }

    private @Px int getInsetDisplayWidthPx() {
        return mContentView.getContext().getResources().getDisplayMetrics().widthPixels
                - 2 * getSideMarginPx();
    }

    private boolean isListedItem(View childInSheetView) {
        int posInAdapter = mSheetItemListView.getChildAdapterPosition(childInSheetView);
        return listedItemTypes()
                .contains(mSheetItemListView.getAdapter().getItemViewType(posInAdapter));
    }

    private boolean isFooterItem(View childInSheetView) {
        int posInAdapter = mSheetItemListView.getChildAdapterPosition(childInSheetView);
        return mSheetItemListView.getAdapter().getItemViewType(posInAdapter) == footerItemType();
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        // Skip the half state if a service requesting touch exploration is enabled.
        return AccessibilityState.isTouchExplorationEnabled();
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // WRAP_CONTENT would be the right fit but this disables the HALF state.
        return Math.min(getMaximumSheetHeightPx(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public float getHalfHeightRatio() {
        // Disable the half state when touch exploration is enabled.
        if (skipHalfStateOnScrollingDown()) return HeightMode.DISABLED;
        return Math.min(getDesiredSheetHeightPx(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public RecyclerView getSheetItemListView() {
        return mSheetItemListView;
    }
}
