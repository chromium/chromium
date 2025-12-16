// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.ViewFlipper;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import com.airbnb.lottie.LottieAnimationView;
import com.airbnb.lottie.LottieDrawable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.notifications.scheduler.TipsNotificationsFeatureType;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.notifications.tips.TipsPromoProperties.ScreenType;
import org.chromium.chrome.browser.quick_delete.QuickDeleteController;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment.HighlightedOption;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Coordinator to manage the promo for the Tips Notifications feature. */
@NullMarked
public class TipsPromoCoordinator {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(FeatureTipPromoEventType)
    @IntDef({
        FeatureTipPromoEventType.SHOWN,
        FeatureTipPromoEventType.DISMISSED,
        FeatureTipPromoEventType.ACCEPTED,
        FeatureTipPromoEventType.DETAIL_PAGE_CLICKED,
        FeatureTipPromoEventType.DETAIL_PAGE_BACK_BUTTON,
        FeatureTipPromoEventType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeatureTipPromoEventType {
        int SHOWN = 0;
        int DISMISSED = 1;
        int ACCEPTED = 2;
        int DETAIL_PAGE_CLICKED = 3;
        int DETAIL_PAGE_BACK_BUTTON = 4;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/notifications/enums.xml:TipsNotificationsFeatureTipPromoEventType)

    private final ComponentCallbacks mComponentCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration configuration) {
                    TipsUtils.scaleBottomSheetImageLogoByWidth(
                            mContext, configuration, mContentView, R.id.main_page_logo);
                }

                @Override
                public void onLowMemory() {}
            };

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
    private final @TipsNotificationsFeatureType int mFeatureType;
    private LensController mLensController;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @param quickDeleteController The controller to for the quick delete dialog.
     * @param windowAndroid The current WindowAndroid.
     * @param isIncognito Whether the current context is incognito.
     * @param featureType The {@link TipsNotificationsFeatureType} to show.
     */
    public TipsPromoCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            QuickDeleteController quickDeleteController,
            WindowAndroid windowAndroid,
            boolean isIncognito,
            @TipsNotificationsFeatureType int featureType) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mQuickDeleteController = quickDeleteController;
        mWindowAndroid = windowAndroid;
        mIsIncognito = isIncognito;
        mPropertyModel = TipsPromoProperties.createDefaultModel();
        mLensController = LensController.getInstance();
        mFeatureType = featureType;

        mContentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.tips_promo_bottom_sheet, /* root= */ null);
        mSheetContent =
                new TipsPromoSheetContent(
                        mContentView, mPropertyModel, mBottomSheetController, featureType);

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

        // Fire an event for the original setup.
        mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    /** Cleans up resources. */
    public void destroy() {
        mChangeProcessor.destroy();
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
    }

    /** Shows the promo. The caller is responsible for all eligibility checks. */
    public void showBottomSheet() {
        FeatureTipPromoData data = TipsUtils.getFeatureTipPromoDataForType(mContext, mFeatureType);
        mPropertyModel.set(TipsPromoProperties.FEATURE_TIP_PROMO_DATA, data);
        mPropertyModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.MAIN_SCREEN);
        setupButtonClickHandlers(mFeatureType);
        setupDetailPageSteps(data.detailPageSteps);
        onShowPromoForFeatureType(mFeatureType, data.mainPageLogoViewRes);
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
    }

    private void setupButtonClickHandlers(@TipsNotificationsFeatureType int featureType) {
        // The button click handlers are setup such that from the MAIN_SCREEN, the details button
        // will link it to the DETAILS_SCREEN. That is the only secondary screen accessible from the
        // MAIN_SCREEN. From there, only the back button and system backpress can go back to the
        // MAIN_SCREEN from the DETAIL_SCREEN as the only final destination.
        mPropertyModel.set(
                TipsPromoProperties.BACK_BUTTON_CLICK_LISTENER,
                (view) -> {
                    mPropertyModel.set(TipsPromoProperties.CURRENT_SCREEN, ScreenType.MAIN_SCREEN);
                    recordFeatureTipPromoEventType(
                            featureType, FeatureTipPromoEventType.DETAIL_PAGE_BACK_BUTTON);
                });
        mPropertyModel.set(
                TipsPromoProperties.DETAILS_BUTTON_CLICK_LISTENER,
                (view) -> {
                    mPropertyModel.set(
                            TipsPromoProperties.CURRENT_SCREEN, ScreenType.DETAIL_SCREEN);
                    recordFeatureTipPromoEventType(
                            featureType, FeatureTipPromoEventType.DETAIL_PAGE_CLICKED);
                });
        mPropertyModel.set(
                TipsPromoProperties.SETTINGS_BUTTON_CLICK_LISTENER,
                (view) -> {
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                    performFeatureAction(featureType);
                    recordFeatureTipPromoEventType(featureType, FeatureTipPromoEventType.ACCEPTED);
                });
    }

    private void setupDetailPageSteps(List<String> steps) {
        LinearLayout stepsContainer =
                (LinearLayout) mContentView.findViewById(R.id.steps_container);
        stepsContainer.removeAllViews();
        for (int i = 0; i < steps.size(); i++) {
            View stepView =
                    LayoutInflater.from(mContext)
                            .inflate(
                                    R.layout.tips_promo_step_item,
                                    stepsContainer,
                                    /* attachToRoot= */ false);
            // TODO(crbug.com/454724965): Translate the step number set for all languages.
            TextView stepNumber = (TextView) stepView.findViewById(R.id.step_number);
            stepNumber.setText(String.valueOf(i + 1));
            TextView stepContent = (TextView) stepView.findViewById(R.id.step_content);
            stepContent.setText(steps.get(i));
            stepsContainer.addView(stepView);
        }

        if (LocalizationUtils.isLayoutRtl()) {
            // Flip the image horizontally, so that the arrow points the right way for RTL.
            ImageView backArrow = mContentView.findViewById(R.id.details_page_back_button);
            backArrow.setScaleX(-1);
        }
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
                LensMetrics.recordClicked(LensEntryPoint.TIPS_NOTIFICATIONS);
                mLensController.startLens(
                        mWindowAndroid,
                        new LensIntentParams.Builder(
                                        LensEntryPoint.TIPS_NOTIFICATIONS, mIsIncognito)
                                .build());
                break;
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                SettingsNavigationFactory.createSettingsNavigation()
                        .startSettings(
                                mContext,
                                AddressBarSettingsFragment.class,
                                AddressBarSettingsFragment.createArguments(
                                        HighlightedOption.BOTTOM_TOOLBAR));
                break;
            default:
                assert false : "Invalid feature type: " + featureType;
        }
    }

    private void onShowPromoForFeatureType(
            @TipsNotificationsFeatureType int featureType, int logoViewRes) {
        LottieAnimationView logoView = mContentView.findViewById(R.id.main_page_logo);
        recordFeatureTipPromoEventType(featureType, FeatureTipPromoEventType.SHOWN);
        switch (featureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                logoView.setImageResource(logoViewRes);
                break;
            case TipsNotificationsFeatureType.QUICK_DELETE:
                logoView.setAnimation(logoViewRes);
                logoView.setRepeatCount(LottieDrawable.INFINITE);
                logoView.playAnimation();
                break;
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                logoView.setImageResource(logoViewRes);
                LensMetrics.recordShown(LensEntryPoint.TIPS_NOTIFICATIONS, /* isShown= */ true);
                break;
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                logoView.setImageResource(logoViewRes);
                break;
            default:
                assert false : "Invalid feature type: " + featureType;
        }
    }

    private String featureTypeToSuffix(@TipsNotificationsFeatureType int featureType) {
        switch (featureType) {
            case TipsNotificationsFeatureType.ENHANCED_SAFE_BROWSING:
                return ".EnhancedSafeBrowsing";
            case TipsNotificationsFeatureType.QUICK_DELETE:
                return ".QuickDelete";
            case TipsNotificationsFeatureType.GOOGLE_LENS:
                return ".GoogleLens";
            case TipsNotificationsFeatureType.BOTTOM_OMNIBOX:
                return ".BottomOmnibox";
            default:
                assert false : "Invalid feature type: " + featureType;
                return "";
        }
    }

    private void recordFeatureTipPromoEventType(
            @TipsNotificationsFeatureType int featureType,
            @FeatureTipPromoEventType int eventType) {
        String histogramName = "Notifications.Tips.FeatureTipPromo.EventType";
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, eventType, FeatureTipPromoEventType.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                histogramName + featureTypeToSuffix(featureType),
                eventType,
                FeatureTipPromoEventType.NUM_ENTRIES);
    }

    @NullMarked
    protected class TipsPromoSheetContent implements BottomSheetContent {
        private final View mContentView;
        private final PropertyModel mModel;
        private final BottomSheetController mController;
        private final BottomSheetObserver mBottomSheetOpenedObserver;
        private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
                new ObservableSupplierImpl<>();
        private final @TipsNotificationsFeatureType int mFeatureTipType;
        private final ScrollView mScrollView;

        TipsPromoSheetContent(
                View contentView,
                PropertyModel model,
                BottomSheetController controller,
                @TipsNotificationsFeatureType int featureTipType) {
            mContentView = contentView;
            mModel = model;
            mController = controller;
            mFeatureTipType = featureTipType;
            mScrollView = mContentView.findViewById(R.id.main_page_scrollview);

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
                                recordFeatureTipPromoEventType(
                                        mFeatureTipType, FeatureTipPromoEventType.DISMISSED);
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
            int currentScreen = mModel.get(TipsPromoProperties.CURRENT_SCREEN);

            if (currentScreen == ScreenType.MAIN_SCREEN) {
                if (mScrollView != null) {
                    // Calculate the scroll position of the scrollview and make sure it is
                    // non-zero, otherwise allows swipe to dismiss on the bottom sheet.
                    return mScrollView.getScrollY();
                }
            }

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
                    recordFeatureTipPromoEventType(
                            mFeatureTipType, FeatureTipPromoEventType.DETAIL_PAGE_BACK_BUTTON);
                    break;
                case ScreenType.MAIN_SCREEN:
                    mController.hideContent(this, /* animate= */ true);
                    recordFeatureTipPromoEventType(
                            mFeatureTipType, FeatureTipPromoEventType.DISMISSED);
                    break;
                default:
                    assert false : "Invalid screen type: " + currentScreen;

                    mController.hideContent(this, /* animate= */ true);
            }
        }
    }

    // For testing methods.

    TipsPromoSheetContent getBottomSheetContentForTesting() {
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

    void triggerConfigurationChangeForTesting(Configuration configuration) {
        mComponentCallbacks.onConfigurationChanged(configuration);
    }
}
