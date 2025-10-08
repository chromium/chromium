// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.annotation.StringRes;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.ScreenType;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator to manage the promo for the Tips Notifications feature. */
@NullMarked
public class TipsPromoCoordinator {
    public static final int INVALID_TIPS_NOTIFICATION_FEATURE_TYPE = -1;

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final QuickDeleteController mQuickDeleteController;
    private final WindowAndroid mWindowAndroid;
    private final boolean mIsIncognito;
    private final TipsPromoSheetContent mSheetContent;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor mChangeProcessor;
    private final ViewFlipper mViewFlipperView;
    private final View mContentView;
    private LensController mLensController;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @param quickDeleteController The controller to for the quick delete dialog.
     */
    public TipsPromoCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            QuickDeleteController quickDeleteController,
            WindowAndroid windowAndroid,
            boolean isIncognito) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mQuickDeleteController = quickDeleteController;
        mWindowAndroid = windowAndroid;
        mIsIncognito = isIncognito;
        mPropertyModel = TipsPromoProperties.createDefaultModel();
        mLensController = LensController.getInstance();

        mContentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.tips_promo_bottom_sheet, /* root= */ null);
        mSheetContent =
                new TipsPromoSheetContent(mContentView, mPropertyModel, mBottomSheetController);

        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, mContentView, TipsPromoViewBinder::bind);

        mViewFlipperView =
                (ViewFlipper) mContentView.findViewById(R.id.tips_promo_bottom_sheet_view_flipper);
        mPropertyModel.addObserver(
                (source, propertyKey) -> {
                    if (TipsPromoProperties.CURRENT_SCREEN == propertyKey) {
                        mViewFlipperView.setDisplayedChild(
                                mPropertyModel.get(TipsPromoProperties.CURRENT_SCREEN));
                    }
                });
    }

    /** Cleans up resources. */
    public void destroy() {
        mChangeProcessor.destroy();
    }

    /**
     * Shows the promo. The caller is responsible for all eligibility checks.
     *
     * @param featureType The {@link TipsNotificationsFeatureType} to show.
     */
    public void showBottomSheet(@TipsNotificationsFeatureType int featureType) {
        FeatureTipPromoData data = TipsUtils.getFeatureTipPromoDataForType(mContext, featureType);
        mPropertyModel.set(TipsPromoProperties.FEATURE_TIP_PROMO_DATA, data);
        mPropertyModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.MAIN_SCREEN);
        mPropertyModel.set(
                TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER,
                (view) -> {
                    mPropertyModel.set(
                            TipsPromoProperties.CURRENT_SCREEN, ScreenType.DETAIL_SCREEN);
                });
        mPropertyModel.set(
                TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER,
                (view) -> {
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                    performFeatureAction(featureType);
                });
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
    }

    private void performFeatureAction(@TipsNotificationsFeatureType int featureType) {
        switch (featureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                Intent intent =
                        SettingsNavigationFactory.createSettingsNavigation()
                                .createSettingsIntent(
                                        mContext,
                                        SafeBrowsingSettingsFragment.class,
                                        SafeBrowsingSettingsFragment.createArguments(
                                                SettingsAccessPoint.TIPS_NOTIFICATIONS_PROMO));
                mContext.startActivity(intent);
                break;
            case TipsNotificationsFeatureType.QUICK_DELETE:
                mQuickDeleteController.showDialog();
                break;
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                mLensController.startLens(
                        mWindowAndroid,
                        new LensIntentParams.Builder(
                                        LensEntryPoint.TIPS_NOTIFICATIONS, mIsIncognito)
                                .build());
                break;
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                SettingsNavigationFactory.createSettingsNavigation()
                        .startSettings(mContext, AddressBarSettingsFragment.class);
                break;
            default:
                assert false : "Invalid feature type: " + featureType;
        }
    }

    private class TipsPromoSheetContent implements BottomSheetContent {
        private final View mContentView;
        private final PropertyModel mModel;
        private final BottomSheetController mController;
        private final BottomSheetObserver mBottomSheetOpenedObserver;
        private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
                new ObservableSupplierImpl<>();

        TipsPromoSheetContent(
                View contentView, PropertyModel model, BottomSheetController controller) {
            mContentView = contentView;
            mModel = model;
            mController = controller;

            mBottomSheetOpenedObserver =
                    new EmptyBottomSheetObserver() {
                        @Override
                        public void onSheetOpened(
                                @BottomSheetController.StateChangeReason int reason) {
                            super.onSheetOpened(reason);
                            mBackPressStateChangedSupplier.set(true);
                        }

                        @Override
                        public void onSheetClosed(
                                @BottomSheetController.StateChangeReason int reason) {
                            super.onSheetClosed(reason);
                            mBackPressStateChangedSupplier.set(false);
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
            return 0;
        }

        @Override
        public void destroy() {
            TipsPromoCoordinator.this.destroy();
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
            backPressOnCurrentScreen();
            return true;
        }

        @Override
        public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
            return mBackPressStateChangedSupplier;
        }

        @Override
        public void onBackPressed() {
            backPressOnCurrentScreen();
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public String getSheetContentDescription(Context context) {
            return context.getString(R.string.tips_promo_bottom_sheet_content_description);
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.tips_promo_bottom_sheet_closed_content_description;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return R.string.tips_promo_bottom_sheet_half_height_content_description;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.tips_promo_bottom_sheet_full_height_content_description;
        }

        private void backPressOnCurrentScreen() {
            @ScreenType int currentScreen = mModel.get(TipsPromoProperties.CURRENT_SCREEN);
            switch (currentScreen) {
                case ScreenType.DETAIL_SCREEN:
                    mModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.MAIN_SCREEN);
                    break;
                case ScreenType.MAIN_SCREEN:
                    mController.hideContent(this, /* animate= */ true);
                    break;
                default:
                    assert false : "Invalid screen type: " + currentScreen;

                    mController.hideContent(this, /* animate= */ true);
            }
        }
    }

    // For testing methods.

    BottomSheetContent getBottomSheetContentForTesting() {
        return mSheetContent;
    }

    PropertyModel getModelForTesting() {
        return mPropertyModel;
    }

    View getViewForTesting() {
        return mContentView;
    }

    void setLensControllerForTesting(LensController lensController) {
        mLensController = lensController;
    }
}
