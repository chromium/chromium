// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.widget.ButtonCompat;

/** Coordinator for the Glic bottom sheet promo. */
@NullMarked
public class GlicPromoCoordinator {

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final GlicPromoSheetContent mSheetContent;
    private final View mContentView;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @param onPositiveButtonClicked The action to execute when the positive button is clicked.
     * @param onDismissed The action to execute when the promo is dismissed.
     */
    public GlicPromoCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            Runnable onPositiveButtonClicked,
            Runnable onDismissed) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.glic_promo_bottom_sheet, /* root= */ null);
        mSheetContent = new GlicPromoSheetContent(mContentView, bottomSheetController, onDismissed);

        ButtonCompat positiveButtonView =
                mContentView.findViewById(R.id.glic_promo_positive_button);
        positiveButtonView.setOnClickListener(
                (view) -> {
                    onPositiveButtonClicked.run();
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                });

        ButtonCompat negativeButtonView =
                mContentView.findViewById(R.id.glic_promo_negative_button);
        negativeButtonView.setOnClickListener(
                (view) -> {
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                });
    }

    /** Shows the promo. The caller is responsible for all eligibility checks. */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
    }

    /** Cleans up the coordinator. */
    public void destroy() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ false);
        mSheetContent.destroy();
    }

    @NullMarked
    protected class GlicPromoSheetContent implements BottomSheetContent {
        private final View mContentView;
        private final BottomSheetController mController;
        private final BottomSheetObserver mBottomSheetOpenedObserver;
        private final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
                ObservableSuppliers.createNonNull(false);
        private final ScrollView mScrollView;

        GlicPromoSheetContent(
                View contentView, BottomSheetController controller, Runnable onDismissed) {
            mContentView = contentView;
            mController = controller;
            mScrollView = mContentView.findViewById(R.id.glic_promo_scrollview);
            mBottomSheetOpenedObserver =
                    new EmptyBottomSheetObserver() {
                        @Override
                        public void onSheetOpened(@StateChangeReason int reason) {
                            super.onSheetOpened(reason);
                            mBackPressStateChangedSupplier.set(true);
                        }

                        @Override
                        public void onSheetClosed(@StateChangeReason int reason) {
                            super.onSheetClosed(reason);
                            mBackPressStateChangedSupplier.set(false);
                            onDismissed.run();
                            mBottomSheetController.removeObserver(mBottomSheetOpenedObserver);
                        }
                    };
            mBottomSheetController.addObserver(mBottomSheetOpenedObserver);
        }

        @Override
        public View getContentView() {
            return mContentView;
        }

        @Nullable
        @Override
        public View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            if (mScrollView != null) {
                return mScrollView.getScrollY();
            }
            return 0;
        }

        @Override
        public void destroy() {
            mBottomSheetController.removeObserver(mBottomSheetOpenedObserver);
        }

        @Override
        public int getPriority() {
            return BottomSheetContent.ContentPriority.HIGH;
        }

        @Override
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public boolean handleBackPress() {
            mController.hideContent(mSheetContent, /* animate= */ true);
            return true;
        }

        @Override
        public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
            return mBackPressStateChangedSupplier;
        }

        @Override
        public void onBackPressed() {
            mController.hideContent(mSheetContent, /* animate= */ true);
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public String getSheetContentDescription(Context context) {
            return context.getString(R.string.educational_tip_glic_title);
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.educational_tip_glic_title;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return R.string.educational_tip_glic_title;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.educational_tip_glic_title;
        }
    }

    // For testing methods.

    GlicPromoSheetContent getBottomSheetContentForTesting() {
        return mSheetContent;
    }

    View getViewForTesting() {
        return mContentView;
    }
}
