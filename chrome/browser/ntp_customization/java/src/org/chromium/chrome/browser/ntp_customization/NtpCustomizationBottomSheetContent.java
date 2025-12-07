// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.SINGLE_THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

import java.util.function.Supplier;

/** Bottom sheet content of the NTP customization. */
@NullMarked
public class NtpCustomizationBottomSheetContent implements BottomSheetContent {

    public static final float MAX_HEIGHT_RATIO = (float) (2.0 / 3);
    public static final int RECYCLER_VIEW_INVALID_HEIGHT = -1;
    private final View mContentView;
    private final Runnable mBackPressRunnable;
    private final Runnable mOnDestroyRunnable;
    private ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier;
    private Supplier<@Nullable Integer> mCurrentBottomSheetTypeSupplier;
    private final Supplier<Integer> mContainerHeightSupplier;
    private final Supplier<Integer> mMaxSheetWidthSupplier;
    private final int mNtpCustomizationBottomSheetBottomPadding;

    NtpCustomizationBottomSheetContent(
            View contentView,
            Supplier<Integer> containerHeightSupplier,
            Supplier<Integer> maxSheetWidthSupplier,
            Runnable backPressRunnable,
            Runnable onDestroy,
            Supplier<@Nullable Integer> currentBottomSheetTypeSupplier) {
        mContentView = contentView;
        mContainerHeightSupplier = containerHeightSupplier;
        mMaxSheetWidthSupplier = maxSheetWidthSupplier;
        mBackPressRunnable = backPressRunnable;
        mBackPressStateChangedSupplier = new ObservableSupplierImpl<>();
        mOnDestroyRunnable = onDestroy;
        mCurrentBottomSheetTypeSupplier = currentBottomSheetTypeSupplier;
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

        return mContentView.findViewById(R.id.ntp_customization_view_flipper).getScrollY();
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
        float containerHeight = mContainerHeightSupplier.get();

        assert containerHeight != 0;

        RecyclerView recyclerView = getActiveRecyclerView();
        if (recyclerView != null) {
            int contentHeight = getContentHeight(recyclerView);
            if (contentHeight != RECYCLER_VIEW_INVALID_HEIGHT) {
                float contentRatio = (float) contentHeight / containerHeight;
                if (contentRatio > 0.5) {
                    return 0.5f;
                }
            }
        }

        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        float containerHeight = mContainerHeightSupplier.get();

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
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
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
                assumeNonNull(mCurrentBottomSheetTypeSupplier.get()));
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return NtpCustomizationUtils.getSheetFullHeightAccessibilityStringId(
                assumeNonNull(mCurrentBottomSheetTypeSupplier.get()));
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ntp_customization_main_bottom_sheet_closed;
    }

    /** Sets up ObservableSupplierImpl<Boolean> when opening the bottom sheet. */
    void onSheetOpened() {
        // Sets the value in the supplier to true to indicate that back press should be handled by
        // the bottom sheet.
        mBackPressStateChangedSupplier.set(true);
    }

    /** Sets up ObservableSupplierImpl<Boolean> when closing the bottom sheet. */
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
        int containerHeight = mContainerHeightSupplier.get();

        int widthSpec =
                View.MeasureSpec.makeMeasureSpec(
                        mMaxSheetWidthSupplier.get(), View.MeasureSpec.EXACTLY);
        int heightSpec =
                View.MeasureSpec.makeMeasureSpec(
                        containerHeight - mNtpCustomizationBottomSheetBottomPadding,
                        View.MeasureSpec.AT_MOST);
        mContentView.measure(widthSpec, heightSpec);

        Integer bottomSheetType = mCurrentBottomSheetTypeSupplier.get();
        View viewToPad = recyclerView;
        if (bottomSheetType != null && bottomSheetType == CHROME_COLORS) {
            viewToPad = mContentView.findViewById(R.id.chrome_colors_recycler_view_container);
        }

        float viewBottom = viewToPad.getBottom();
        if (viewBottom == 0) {
            return RECYCLER_VIEW_INVALID_HEIGHT;
        }

        float maxHeight = getMaxHeight();
        int viewBottomPadding = 0;
        if (viewBottom > maxHeight) {
            viewBottomPadding = (int) Math.ceil(viewBottom - maxHeight);
        }
        viewToPad.setPaddingRelative(
                viewToPad.getPaddingStart(),
                viewToPad.getPaddingTop(),
                viewToPad.getPaddingEnd(),
                viewBottomPadding);

        mContentView.measure(widthSpec, heightSpec);

        return mContentView.getMeasuredHeight();
    }

    /**
     * Calculates the maximum height the bottom sheet content should occupy, based on the container
     * height and a predefined maximum ratio.
     */
    private float getMaxHeight() {
        float containerHeight = mContainerHeightSupplier.get();
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
        if (bottomSheetType == THEME_COLLECTIONS) {
            return mContentView.findViewById(R.id.theme_collections_recycler_view);
        } else if (bottomSheetType == SINGLE_THEME_COLLECTION) {
            return mContentView.findViewById(R.id.single_theme_collection_recycler_view);
        } else if (bottomSheetType == CHROME_COLORS) {
            return mContentView.findViewById(R.id.chrome_colors_recycler_view);
        }
        return null;
    }

    void setBackPressStateChangedSupplierForTesting(ObservableSupplierImpl<Boolean> supplier) {
        mBackPressStateChangedSupplier = supplier;
    }

    void setCurrentBottomSheetTypeSupplierForTesting(Supplier<@Nullable Integer> supplier) {
        mCurrentBottomSheetTypeSupplier = supplier;
    }
}
