// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ViewFlipper;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.function.Supplier;

/** Bottom sheet content of the NTP customization. */
@NullMarked
public class NtpCustomizationBottomSheetContent implements BottomSheetContent {

    public static final float MAX_HEIGHT_RATIO = (float) (2.0 / 3);
    public static final int RECYCLER_VIEW_INVALID_HEIGHT = -1;
    private final View mContentView;
    private final BottomSheetController mBottomSheetController;
    private final Runnable mBackPressRunnable;
    private final Runnable mOnDestroyRunnable;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
            ObservableSuppliers.createNonNull(false);
    private Supplier<@Nullable Integer> mCurrentBottomSheetTypeSupplier;
    private final int mNtpCustomizationBottomSheetBottomPadding;
    private final boolean mIsLargeFormFactorUi;

    NtpCustomizationBottomSheetContent(
            View contentView,
            BottomSheetController bottomSheetController,
            Runnable backPressRunnable,
            Runnable onDestroy,
            Supplier<@Nullable Integer> currentBottomSheetTypeSupplier) {
        mContentView = contentView;
        mBottomSheetController = bottomSheetController;
        mBackPressRunnable = backPressRunnable;
        mOnDestroyRunnable = onDestroy;
        mCurrentBottomSheetTypeSupplier = currentBottomSheetTypeSupplier;
        mIsLargeFormFactorUi = mBottomSheetController.isLargeFormFactorUiEnabled(this);
        mNtpCustomizationBottomSheetBottomPadding =
                mContentView
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_customization_bottom_sheet_layout_padding_bottom);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        RecyclerView recyclerView = getActiveRecyclerView();
        if (recyclerView != null) {
            return recyclerView.computeVerticalScrollOffset();
        }

        View viewFlipperView = mContentView.findViewById(R.id.ntp_customization_view_flipper);
        if (viewFlipperView instanceof ViewFlipper viewFlipper) {
            View currentView = viewFlipper.getCurrentView();
            if (currentView != null) {
                return currentView.getScrollY();
            }
        }
        return viewFlipperView != null ? viewFlipperView.getScrollY() : 0;
    }

    @Override
    public void destroy() {
        mOnDestroyRunnable.run();
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public float getHalfHeightRatio() {
        if (mIsLargeFormFactorUi) {
            return HeightMode.DISABLED;
        }

        float containerHeight = getContainerHeight();

        assert containerHeight != 0;

        RecyclerView recyclerView = getActiveRecyclerView();
        if (recyclerView != null) {
            int contentHeight = getContentHeight(recyclerView);
            if (contentHeight != RECYCLER_VIEW_INVALID_HEIGHT) {
                float contentRatio = (float) contentHeight / containerHeight;
                if (contentRatio > 0.5) {
                    return Math.min(contentRatio, MAX_HEIGHT_RATIO);
                }
            }
        }

        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        if (mIsLargeFormFactorUi) {
            float containerHeight = getContainerHeight();
            if (containerHeight <= 0) {
                return BottomSheetContent.HeightMode.WRAP_CONTENT;
            }

            RecyclerView recyclerView = getActiveRecyclerView();
            if (recyclerView != null) {
                int widthSpec =
                        View.MeasureSpec.makeMeasureSpec(
                                mBottomSheetController.getMaxSheetWidth(),
                                View.MeasureSpec.EXACTLY);
                int maxContentHeight =
                        Math.max(
                                0,
                                (int) containerHeight - mNtpCustomizationBottomSheetBottomPadding);
                int heightSpec =
                        View.MeasureSpec.makeMeasureSpec(
                                maxContentHeight, View.MeasureSpec.AT_MOST);
                mContentView.measure(widthSpec, heightSpec);
                int contentHeight = mContentView.getMeasuredHeight();
                if (contentHeight >= maxContentHeight) {
                    return 1.0f;
                }
                return Math.min((float) contentHeight / containerHeight, 1.0f);
            }
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        float containerHeight = getContainerHeight();

        assert containerHeight != 0;

        RecyclerView recyclerView = getActiveRecyclerView();
        if (recyclerView != null) {
            int contentHeight = getContentHeight(recyclerView);
            if (contentHeight != RECYCLER_VIEW_INVALID_HEIGHT) {
                float contentRatio = (float) contentHeight / containerHeight;
                if (contentRatio > 0.5) {
                    return Math.min(contentRatio, MAX_HEIGHT_RATIO);
                }
            }
        }

        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public void onBackPressed() {
        mBackPressRunnable.run();
    }

    @Override
    public boolean handleBackPress() {
        mBackPressRunnable.run();
        return true;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public @Nullable String getSheetContentDescription(Context context) {
        return context.getString(
                NtpCustomizationUtils.getSheetContentDescription(
                        assumeNonNull(mCurrentBottomSheetTypeSupplier.get())));
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return NtpCustomizationUtils.getSheetHalfHeightAccessibilityStringId(
                mCurrentBottomSheetTypeSupplier.get());
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return NtpCustomizationUtils.getSheetFullHeightAccessibilityStringId(
                mCurrentBottomSheetTypeSupplier.get());
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // The accessibility string is hardcoded here because mCurrentBottomSheetTypeSupplier.get()
        // will always return null in this function since mCurrentBottomSheet is set to null upon
        // dismissal.
        return R.string.ntp_customization_main_bottom_sheet_closed;
    }

    /** Sets up the supplier when opening the bottom sheet. */
    void onSheetOpened() {
        // Sets the value in the supplier to true to indicate that back press should be handled by
        // the bottom sheet.
        mBackPressStateChangedSupplier.set(true);
    }

    /** Sets up the supplier when closing the bottom sheet. */
    void onSheetClosed() {
        // Sets the value in the supplier to false to indicate that back press should not be handled
        // by the bottom sheet.
        mBackPressStateChangedSupplier.set(false);
    }

    /**
     * Calculates the height of the content view and adjusts the RecyclerView's bottom padding to
     * ensure content doesn't overflow the maximum allowed height.
     *
     * @param recyclerView The RecyclerView currently displayed in the bottom sheet.
     * @return The measured height of the content view, or RECYCYCLER_VIEW_NOT_LAID_OUT if the
     *     RecyclerView has not been laid out yet.
     */
    private int getContentHeight(RecyclerView recyclerView) {
        int containerHeight = getContainerHeight();

        int widthSpec =
                View.MeasureSpec.makeMeasureSpec(
                        mBottomSheetController.getMaxSheetWidth(), View.MeasureSpec.EXACTLY);
        int heightSpec =
                View.MeasureSpec.makeMeasureSpec(
                        containerHeight - mNtpCustomizationBottomSheetBottomPadding,
                        View.MeasureSpec.AT_MOST);
        mContentView.measure(widthSpec, heightSpec);

        float viewBottom = recyclerView.getBottom();
        if (viewBottom == 0) {
            return RECYCLER_VIEW_INVALID_HEIGHT;
        }

        float maxHeight = getMaxHeight();
        int viewBottomPadding = 0;
        if (viewBottom > maxHeight) {
            viewBottomPadding = (int) Math.ceil(viewBottom - maxHeight);
        }
        recyclerView.setPaddingRelative(
                recyclerView.getPaddingStart(),
                recyclerView.getPaddingTop(),
                recyclerView.getPaddingEnd(),
                viewBottomPadding);

        mContentView.measure(widthSpec, heightSpec);

        return mContentView.getMeasuredHeight();
    }

    /**
     * Calculates the maximum height the bottom sheet content should occupy, based on the container
     * height and a predefined maximum ratio.
     */
    private float getMaxHeight() {
        float containerHeight = getContainerHeight();
        return MAX_HEIGHT_RATIO * containerHeight;
    }

    /** Retrieves the currently active RecyclerView based on the bottom sheet's state. */
    @Nullable RecyclerView getActiveRecyclerView() {
        Integer bottomSheetType = mCurrentBottomSheetTypeSupplier.get();
        if (bottomSheetType == null) {
            return null;
        }

        // TODO(crbug.com/423579377): Pass in a delegate here will make it easier to support other
        // bottom sheets later on.
        RecyclerView recyclerView = null;
        if (bottomSheetType == THEME_COLLECTIONS) {
            recyclerView = mContentView.findViewById(R.id.theme_collections_recycler_view);
        } else if (bottomSheetType == SINGLE_THEME_COLLECTION) {
            recyclerView = mContentView.findViewById(R.id.single_theme_collection_recycler_view);
        }

        if (recyclerView != null && mIsLargeFormFactorUi) {
            ViewGroup.LayoutParams lp = recyclerView.getLayoutParams();
            if (lp instanceof ConstraintLayout.LayoutParams params) {
                if (params.bottomToBottom != ConstraintLayout.LayoutParams.PARENT_ID) {
                    params.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID;
                    params.constrainedHeight = true;
                    recyclerView.setLayoutParams(params);
                }
            }
        }

        return recyclerView;
    }

    void setCurrentBottomSheetTypeSupplierForTesting(Supplier<@Nullable Integer> supplier) {
        mCurrentBottomSheetTypeSupplier = supplier;
    }

    private int getContainerHeight() {
        if (mIsLargeFormFactorUi) {
            int maxHeight = mBottomSheetController.getMaxSheetHeight();
            if (maxHeight > 0) return maxHeight;
        }
        return mBottomSheetController.getContainerHeight();
    }
}
