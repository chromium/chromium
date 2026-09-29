// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.base.CallbackUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarPrefs;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for the Glic bottom sheet promo. */
@NullMarked
public class GlicPromoCoordinator {
    // LINT.IfChange(GlicPromoBottomSheetDismissReason)
    @IntDef({
        DismissReason.ACCEPT,
        DismissReason.REJECT,
        DismissReason.DISMISS,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DismissReason {
        int ACCEPT = 0;
        int REJECT = 1;
        int DISMISS = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:GlicPromoBottomSheetDismissReason)

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final GlicPromoSheetContent mSheetContent;
    private final View mContentView;
    private boolean mDecisionRecorded;

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
                    recordDismissReason(DismissReason.ACCEPT);
                    onPositiveButtonClicked.run();
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                });

        ButtonCompat negativeButtonView =
                mContentView.findViewById(R.id.glic_promo_negative_button);
        negativeButtonView.setOnClickListener(
                (view) -> {
                    recordDismissReason(DismissReason.REJECT);
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                });
    }

    private void recordDismissReason(@DismissReason int reason) {
        if (mDecisionRecorded) {
            return;
        }
        mDecisionRecorded = true;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.GlicPromoBottomSheet.DismissReason", reason, DismissReason.NUM_ENTRIES);
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

    /**
     * Determines whether the Glic promo should be shown and shows it if eligible.
     *
     * @param activity The host {@link Activity}.
     * @param profile The current {@link Profile}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @return The {@link GlicPromoCoordinator} if the promo was shown, or null otherwise.
     */
    public static @Nullable GlicPromoCoordinator maybeShowPromo(
            Activity activity,
            @Nullable Profile profile,
            BottomSheetController bottomSheetController) {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
                || CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_STARTUP_PROMOS)) {
            return null;
        }

        if (profile == null
                || activity == null
                || activity.isFinishing()
                || activity.isDestroyed()) {
            return null;
        }

        boolean hasEvaluatedGlicPromo =
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);

        if (!GlicEnabling.isEnabledByFlags() && hasEvaluatedGlicPromo) {
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
            return null;
        }

        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.GLIC, "adaptive-toolbar-auto-pin", true)) {
            return null;
        }

        // When the Android Bottom Bar is enabled the promo is not required as the button is
        // available by default.
        boolean glicEnabled = GlicEnabling.isEnabledForProfile(profile);
        boolean bottomBarEnabled = BottomBarConfigUtils.isBottomBarEnabled(activity);
        if (!glicEnabled || bottomBarEnabled) {
            return null;
        }

        if (hasEvaluatedGlicPromo) {
            return null;
        }
        boolean isGlicPinned =
                AdaptiveToolbarPrefs.getCustomizationSetting() == AdaptiveToolbarButtonVariant.GLIC;
        boolean isToolbarPinned =
                AdaptiveToolbarPrefs.getCustomizationSetting() != AdaptiveToolbarButtonVariant.AUTO;
        // We use wouldTriggerHelpUi and notifyEvent manually instead of shouldTriggerHelpUi
        // to avoid locking the IPH session and blocking other IPHs from showing.
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        boolean shouldPinGlic =
                tracker.wouldTriggerHelpUi(
                        FeatureConstants.ADAPTIVE_BUTTON_PIN_GLIC_TOOLBAR_BUTTON_FEATURE);
        tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_GLIC_IPH_TRIGGER);

        // Auto-enable the Glic button and bypass the promo if:
        // 1. Glic is already pinned to the toolbar.
        // 2. The feature engagement tracker recommends pinning Glic AND the user has not
        //    manually customized the toolbar with a different button (to avoid overriding
        //    the user's explicit preference).
        if (isGlicPinned || (shouldPinGlic && !isToolbarPinned)) {
            enableGlicButton();
            return null;
        }

        if (!ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.GLIC, "glic-bottom-sheet-promo", true)) {
            return null;
        }

        return showPromo(activity, bottomSheetController);
    }

    private static GlicPromoCoordinator showPromo(
            Activity activity, BottomSheetController bottomSheetController) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, false);

        Runnable onAccepted = GlicPromoCoordinator::enableGlicButton;
        Runnable onDismissed = CallbackUtils.emptyRunnable();

        GlicPromoCoordinator coordinator =
                new GlicPromoCoordinator(activity, bottomSheetController, onAccepted, onDismissed);
        coordinator.showBottomSheet();
        return coordinator;
    }

    private static void enableGlicButton() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED, true);
        AdaptiveToolbarPrefs.saveToolbarButtonManualOverride(AdaptiveToolbarButtonVariant.GLIC);
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
                    new BottomSheetObserver() {
                        @Override
                        public void onSheetOpened(@StateChangeReason int reason) {
                            mBackPressStateChangedSupplier.set(true);
                        }

                        @Override
                        public void onSheetClosed(@StateChangeReason int reason) {
                            mBackPressStateChangedSupplier.set(false);
                            recordDismissReason(DismissReason.DISMISS);
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
