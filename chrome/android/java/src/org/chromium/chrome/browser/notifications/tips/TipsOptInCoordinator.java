// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for the Tips Opt In bottom sheet. */
@NullMarked
public class TipsOptInCoordinator {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(OptInPromoEventType)
    @IntDef({
        OptInPromoEventType.SHOWN,
        OptInPromoEventType.ACCEPTED,
        OptInPromoEventType.IGNORED,
        OptInPromoEventType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface OptInPromoEventType {
        int SHOWN = 0;
        int ACCEPTED = 1;
        int IGNORED = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/notifications/enums.xml:TipsNotificationsOptInPromoEventType)

    private final ComponentCallbacks mComponentCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration configuration) {
                    TipsUtils.scaleBottomSheetImageLogoByWidth(
                            mContext, configuration, mContentView, R.id.opt_in_logo);
                }

                @Override
                public void onLowMemory() {}
            };

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final TipsOptInSheetContent mSheetContent;
    private final View mContentView;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     */
    public TipsOptInCoordinator(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;

        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tips_opt_in_bottom_sheet, /* root= */ null);
        mSheetContent = new TipsOptInSheetContent(mContentView, bottomSheetController);

        ButtonCompat positiveButtonView = mContentView.findViewById(R.id.opt_in_positive_button);
        positiveButtonView.setOnClickListener(
                (view) -> {
                    TipsUtils.launchTipsNotificationsSettings(mContext);
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                    recordOptInPromoEventType(OptInPromoEventType.ACCEPTED);
                });

        ButtonCompat negativeButtonView = mContentView.findViewById(R.id.opt_in_negative_button);
        negativeButtonView.setOnClickListener(
                (view) -> {
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                    recordOptInPromoEventType(OptInPromoEventType.IGNORED);
                });

        // Fire an event for the original setup.
        mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    /** Cleans up resources. */
    public void destroy() {
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
    }

    /** Shows the promo. The caller is responsible for all eligibility checks. */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);

        // Mark that the promo has been shown.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN, true);
        recordOptInPromoEventType(OptInPromoEventType.SHOWN);
    }

    private void recordOptInPromoEventType(@OptInPromoEventType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Notifications.Tips.OptInPromo.EventType", type, OptInPromoEventType.NUM_ENTRIES);
    }

    @NullMarked
    protected class TipsOptInSheetContent implements BottomSheetContent {
        private final View mContentView;
        private final BottomSheetController mController;
        private final BottomSheetObserver mBottomSheetOpenedObserver;
        private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
                new ObservableSupplierImpl<>();
        private final ScrollView mScrollView;

        TipsOptInSheetContent(View contentView, BottomSheetController controller) {
            mContentView = contentView;
            mController = controller;
            mScrollView = mContentView.findViewById(R.id.opt_in_scrollview);
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
                            mBottomSheetController.removeObserver(mBottomSheetOpenedObserver);

                            if (reason == StateChangeReason.SWIPE
                                    || reason == StateChangeReason.TAP_SCRIM) {
                                recordOptInPromoEventType(OptInPromoEventType.IGNORED);
                            }
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

        /**
         * The vertical scroll offset of the bottom sheet. The offset prevents scroll flinging from
         * dismissing the sheet.
         */
        @Override
        public int getVerticalScrollOffset() {
            if (mScrollView != null) {
                // Calculate the scroll position of the scrollview and make sure it is
                // non-zero, otherwise allows swipe to dismiss on the bottom sheet.
                return mScrollView.getScrollY();
            }

            return 0;
        }

        @Override
        public void destroy() {
            TipsOptInCoordinator.this.destroy();
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
            recordOptInPromoEventType(OptInPromoEventType.IGNORED);
            return true;
        }

        @Override
        public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
            return mBackPressStateChangedSupplier;
        }

        @Override
        public void onBackPressed() {
            mController.hideContent(mSheetContent, /* animate= */ true);
            recordOptInPromoEventType(OptInPromoEventType.IGNORED);
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public String getSheetContentDescription(Context context) {
            return context.getString(R.string.tips_opt_in_bottom_sheet_content_description);
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.tips_opt_in_bottom_sheet_closed_content_description;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return R.string.tips_opt_in_bottom_sheet_half_height_content_description;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.tips_opt_in_bottom_sheet_full_height_content_description;
        }
    }

    // For testing methods.

    TipsOptInSheetContent getBottomSheetContentForTesting() {
        return mSheetContent;
    }

    View getViewForTesting() {
        return mContentView;
    }

    void triggerConfigurationChangeForTesting(Configuration configuration) {
        mComponentCallbacks.onConfigurationChanged(configuration);
    }
}
