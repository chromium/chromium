// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
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
        OptInPromoEventType.DECLINED,
        OptInPromoEventType.CLOSED_BY_SYSTEM,
        OptInPromoEventType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface OptInPromoEventType {
        int SHOWN = 0;
        int ACCEPTED = 1;
        int IGNORED = 2;
        int DECLINED = 3;
        int CLOSED_BY_SYSTEM = 4;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/notifications/enums.xml:TipsNotificationsOptInPromoEventType)

    @IntDef({
        TipsOptInUserInteraction.DISMISSED,
        TipsOptInUserInteraction.ACCEPTED,
        TipsOptInUserInteraction.DECLINED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TipsOptInUserInteraction {
        int DISMISSED = 0;
        int ACCEPTED = 1;
        int DECLINED = 2;
    }

    /** Where the sheet is in its lifecycle, so that each show records exactly one outcome. */
    @IntDef({PromoState.NOT_SHOWN, PromoState.OPEN, PromoState.HIDDEN, PromoState.CLOSED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** The sheet hasn't opened yet. */
        int NOT_SHOWN = 0;

        /** The sheet is open. */
        int OPEN = 1;

        /**
         * The sheet was hidden without a user choice, e.g. while the omnibox is focused or while a
         * higher priority sheet is showing, and may open again.
         */
        int HIDDEN = 2;

        /** The outcome has been recorded. */
        int CLOSED = 3;
    }

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
    private final SnackbarManager mSnackbarManager;
    private final SharedPreferencesManager mSharedPreferences;
    private final TipsOptInSheetContent mSheetContent;
    private final View mContentView;
    private @Nullable Tracker mTracker;
    private @TipsOptInUserInteraction int mUserInteractionType;
    private boolean mIsDestroyed;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @param snackbarManager The system {@link SnackbarManager}.
     * @param sharedPreferences The {@link SharedPreferencesManager} to use.
     * @param tracker Optional {@link Tracker} to notify when the bottom sheet is dismissed.
     */
    public TipsOptInCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager,
            SharedPreferencesManager sharedPreferences,
            @Nullable Tracker tracker) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mSnackbarManager = snackbarManager;
        mSharedPreferences = sharedPreferences;
        mTracker = tracker;
        mUserInteractionType = TipsOptInUserInteraction.DISMISSED;

        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tips_opt_in_bottom_sheet, /* root= */ null);
        mSheetContent = new TipsOptInSheetContent(mContentView, bottomSheetController);

        ButtonCompat positiveButtonView = mContentView.findViewById(R.id.opt_in_positive_button);
        positiveButtonView.setOnClickListener(_ -> onOptInAccepted());

        ButtonCompat negativeButtonView = mContentView.findViewById(R.id.opt_in_negative_button);
        negativeButtonView.setOnClickListener(_ -> onOptInDeclined());

        // Fire an event for the original setup.
        mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    /** Records the outcome if the sheet closed without a user choice, and cleans up resources. */
    public void destroy() {
        if (mIsDestroyed) return;

        mIsDestroyed = true;
        mSheetContent.onDestroyed();
        notifyTrackerDismissed();
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
    }

    private void notifyTrackerDismissed() {
        if (mTracker != null) {
            mTracker.dismissed(FeatureConstants.ANDROID_TIPS_OPT_IN_PROMO_FEATURE);
            mTracker = null;
        }
    }

    /**
     * Requests the promo. The caller is responsible for all eligibility checks. The show is counted
     * once the sheet actually opens, since a request can be queued behind another sheet or dropped.
     */
    public void showBottomSheet() {
        if (!mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true)) {
            notifyTrackerDismissed();
        }
    }

    /** Counts the show against the promo limits once the sheet opens. */
    private void onSheetShown() {
        // Increment the show count.
        int currentCount =
                mSharedPreferences.readInt(
                        ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT, 0);
        mSharedPreferences.writeInt(
                ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOW_COUNT, currentCount + 1);

        // Record the current timestamp.
        mSharedPreferences.writeLong(
                ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_LAST_SHOWN_TIMESTAMP,
                System.currentTimeMillis());

        recordOptInPromoEventType(OptInPromoEventType.SHOWN);
    }

    private void recordOptInPromoEventType(@OptInPromoEventType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Notifications.Tips.OptInPromo.EventType3", type, OptInPromoEventType.NUM_ENTRIES);
    }

    private boolean canHandleUserAction() {
        return !mIsDestroyed
                && mUserInteractionType == TipsOptInUserInteraction.DISMISSED
                && !mSheetContent.isClosed();
    }

    @VisibleForTesting
    void onOptInAccepted() {
        if (!canHandleUserAction()) return;

        mUserInteractionType = TipsOptInUserInteraction.ACCEPTED;
        // The snackbar is shown once the sheet has closed, see onSheetClosed().
        TipsUtils.acceptOptIn(
                mContext,
                mSharedPreferences,
                enabled -> {
                    recordOptInPromoEventType(OptInPromoEventType.ACCEPTED);
                    // The opt-in is persisted either way; don't open settings on top of whatever
                    // replaced the activity if it went away while the opt-in was in flight.
                    if (mIsDestroyed) return;

                    if (!enabled) {
                        // Send the user to the notification settings to finish opting in, e.g.
                        // when app-level notifications are off.
                        TipsUtils.launchTipsNotificationsSettings(mContext);
                    }
                    dismiss();
                });
    }

    @VisibleForTesting
    void onOptInDeclined() {
        if (!canHandleUserAction()) return;

        mUserInteractionType = TipsOptInUserInteraction.DECLINED;
        // Create the channel as disabled so Tips is listed, turned off, in the system notification
        // settings and the user can turn it on there later.
        new ChannelsInitializer(
                        BaseNotificationManagerProxyFactory.create(),
                        ChromeChannelDefinitions.getInstance(),
                        mContext.getResources())
                .ensureInitializedAndDisabled(ChannelId.TIPS_V2);

        TipsUtils.updateTipsNotificationsOptInEligibilityPref(mSharedPreferences);
        recordOptInPromoEventType(OptInPromoEventType.DECLINED);
        dismiss();
    }

    private void dismiss() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }

    @NullMarked
    protected class TipsOptInSheetContent implements BottomSheetContent {
        private static final BottomSheetType SHEET_TYPE = new BottomSheetType.Builder().build();

        private final View mContentView;
        private final BottomSheetController mController;
        private final BottomSheetObserver mBottomSheetOpenedObserver;
        private final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
                ObservableSuppliers.createNonNull(false);
        private final ScrollView mScrollView;
        private @PromoState int mPromoState = PromoState.NOT_SHOWN;

        TipsOptInSheetContent(View contentView, BottomSheetController controller) {
            mContentView = contentView;
            mController = controller;
            mScrollView = mContentView.findViewById(R.id.opt_in_scrollview);
            // The controller notifies observers about every sheet, so only react to this one.
            mBottomSheetOpenedObserver =
                    new BottomSheetObserver() {
                        @Override
                        public void onSheetOpened(@StateChangeReason int reason) {
                            if (mController.getCurrentSheetContent() != TipsOptInSheetContent.this
                                    || mPromoState == PromoState.CLOSED) {
                                return;
                            }
                            if (mPromoState == PromoState.NOT_SHOWN) {
                                onSheetShown();
                            }
                            mPromoState = PromoState.OPEN;
                            mBackPressStateChangedSupplier.set(true);
                        }

                        @Override
                        public void onSheetClosed(@StateChangeReason int reason) {
                            onClosed(reason);
                        }

                        @Override
                        public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                            // Another sheet took this one's place while it was hidden. It may
                            // never open again, so don't keep other in-product help blocked.
                            if (mPromoState == PromoState.HIDDEN
                                    && newContent != TipsOptInSheetContent.this) {
                                notifyTrackerDismissed();
                            }
                        }
                    };
            mController.addObserver(mBottomSheetOpenedObserver);
        }

        /** Records how the open sheet was closed. Does nothing if this sheet isn't open. */
        private void onClosed(@StateChangeReason int reason) {
            if (mPromoState != PromoState.OPEN) return;
            mBackPressStateChangedSupplier.set(false);

            if (mUserInteractionType == TipsOptInUserInteraction.DISMISSED) {
                boolean closedByUser =
                        reason == StateChangeReason.SWIPE
                                || reason == StateChangeReason.TAP_SCRIM
                                || reason == StateChangeReason.BACK_PRESS;
                if (!closedByUser) {
                    // The sheet may open again, so keep the tracker blocked. If it doesn't open
                    // again, the outcome is recorded and the tracker notified once it's destroyed,
                    // which happens right after a close that removes it for good.
                    mPromoState = PromoState.HIDDEN;
                    return;
                }
                recordOptInPromoEventType(OptInPromoEventType.IGNORED);
            } else if (mUserInteractionType == TipsOptInUserInteraction.ACCEPTED) {
                TipsUtils.areTipsNotificationsEnabled(
                        isEnabled -> {
                            if (isEnabled) {
                                TipsUtils.showOptInSnackbar(mContext, mSnackbarManager);
                            }
                        });
            }
            notifyTrackerDismissed();
            mPromoState = PromoState.CLOSED;
        }

        private boolean isClosed() {
            return mPromoState == PromoState.CLOSED;
        }

        private void onDestroyed() {
            // Covers a sheet hidden without a user choice, and one torn down while it's open
            // without an onSheetClosed() call, e.g. when the activity is destroyed.
            if (mPromoState == PromoState.HIDDEN
                    || (mPromoState == PromoState.OPEN
                            && mUserInteractionType == TipsOptInUserInteraction.DISMISSED)) {
                recordOptInPromoEventType(OptInPromoEventType.CLOSED_BY_SYSTEM);
            }
            mPromoState = PromoState.CLOSED;
            mController.removeObserver(mBottomSheetOpenedObserver);
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
        public BottomSheetType getSheetType() {
            return SHEET_TYPE;
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
            mController.hideContent(
                    mSheetContent, /* animate= */ true, StateChangeReason.BACK_PRESS);
            return true;
        }

        @Override
        public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
            return mBackPressStateChangedSupplier;
        }

        @Override
        public void onBackPressed() {
            mController.hideContent(
                    mSheetContent, /* animate= */ true, StateChangeReason.BACK_PRESS);
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
