// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for the Finds opt-in bottom sheet. */
@NullMarked
public class FindsOptInCoordinator {
    @IntDef({
        FindsOptInUserInteraction.DISMISSED,
        FindsOptInUserInteraction.ACCEPTED,
        FindsOptInUserInteraction.DECLINED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface FindsOptInUserInteraction {
        int DISMISSED = 0;
        int ACCEPTED = 1;
        int DECLINED = 2;
    }

    @VisibleForTesting public static final float LOTTIE_MAX_HEIGHT_RATIO = 0.30f;

    @VisibleForTesting
    // Matches ratio defined in finds_opt_in_lottie_animation.
    static final float LOTTIE_INTRINSIC_ASPECT_RATIO = 325.0f / 257.0f;

    private final Context mContext;
    private final Profile mProfile;
    private final BottomSheetController mBottomSheetController;
    private final SnackbarManager mSnackbarManager;
    private final FindsOptInBottomSheetContent mSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final ComponentCallbacks mComponentCallbacks;
    private final View mContentView;
    private final View mAnimationView;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
            ObservableSuppliers.createNonNull(false);
    // Tracks the user interaction with the opt-in promo, default value will be dismissal.
    private @FindsOptInUserInteraction int mUserInteractionType;

    /**
     * @param context The Android {@link Context}.
     * @param profile The {@link Profile} associated with the current user.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @param snackbarManager The system {@link SnackbarManager}.
     */
    public FindsOptInCoordinator(
            Context context,
            Profile profile,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager) {
        mContext = context;
        mProfile = profile;
        mBottomSheetController = bottomSheetController;
        mSnackbarManager = snackbarManager;
        mUserInteractionType = FindsOptInUserInteraction.DISMISSED;

        mContentView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.finds_opt_in_bottom_sheet, /* root= */ null);
        mAnimationView = mContentView.findViewById(R.id.finds_opt_in_lottie_animation);

        mComponentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration configuration) {
                        // When orientation changes, recalculate Lottie animation height.
                        scaleBottomSheetLottieAnimationByHeight(configuration);
                    }

                    @Override
                    public void onLowMemory() {}
                };
        // Initialize the Lottie animation's dimensions for the current screen configuration.
        mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());
        mContext.registerComponentCallbacks(mComponentCallbacks);

        mSheetContent =
                new FindsOptInBottomSheetContent(
                        mContentView,
                        this::onBackPressed,
                        this::destroy,
                        mBackPressStateChangedSupplier);

        ButtonCompat positiveButtonView = mContentView.findViewById(R.id.opt_in_positive_button);
        positiveButtonView.setOnClickListener(
                (view) -> {
                    onOptInAccepted();
                });

        ButtonCompat negativeButtonView = mContentView.findViewById(R.id.opt_in_negative_button);
        negativeButtonView.setOnClickListener(
                (view) -> {
                    onOptInDeclined();
                });

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetOpened(@StateChangeReason int reason) {
                        if (mBottomSheetController.getCurrentSheetContent() != mSheetContent) {
                            return;
                        }
                        super.onSheetOpened(reason);
                        mBackPressStateChangedSupplier.set(true);
                    }

                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        if (mBottomSheetController.getCurrentSheetContent() != mSheetContent) {
                            return;
                        }
                        super.onSheetClosed(reason);
                        mBackPressStateChangedSupplier.set(false);

                        if (mUserInteractionType == FindsOptInUserInteraction.DISMISSED) {
                            FindsMetrics.recordOptInDismissed();
                        } else if (mUserInteractionType == FindsOptInUserInteraction.ACCEPTED) {
                            // Only show the snackbar if both app-level notifications and finds
                            // channel are activated.
                            FindsUtils.areFindsNotificationsEnabled(
                                    isEnabled -> {
                                        if (isEnabled) showOptInSnackbar();
                                    });
                        }
                        // Reset mUserInteractionType.
                        mUserInteractionType = FindsOptInUserInteraction.DISMISSED;
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    @VisibleForTesting
    void onOptInAccepted() {
        mUserInteractionType = FindsOptInUserInteraction.ACCEPTED;
        FindsUtils.isFindsChannelCreated(
                (channelExists) -> {
                    boolean firstTime = !channelExists;
                    if (firstTime) {
                        // For first time opt-in, initialize the notification channel as enabled.
                        new ChannelsInitializer(
                                        BaseNotificationManagerProxyFactory.create(),
                                        ChromeChannelDefinitions.getInstance(),
                                        mContext.getResources())
                                .ensureInitialized(ChannelId.CHROME_FINDS);
                    }

                    // Verify if notifications are fully enabled (both app-level and channel).
                    // If not, redirect to settings so the user can finalize their opt-in.
                    // This handles first-time users with app-level disabled and testing
                    // configuration flow where app-level/channel notifications aren't enabled.
                    FindsUtils.areFindsNotificationsEnabled(
                            (enabled) -> {
                                if (!enabled) {
                                    FindsUtils.launchFindsNotificationSettings(mContext);
                                }
                                FindsMetrics.recordOptInAccepted(firstTime);
                                FindsUtils.setOptInPromoInteracted(mProfile);
                                dismiss();
                            });
                });
    }

    private void showOptInSnackbar() {
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                mContext.getString(R.string.chrome_finds_opt_in_snackbar_message),
                                new SnackbarController() {
                                    @Override
                                    public void onAction(@Nullable Object actionData) {
                                        FindsUtils.launchFindsNotificationSettings(mContext);
                                        FindsMetrics.recordSnackbarActionClicked();
                                    }
                                },
                                Snackbar.TYPE_NOTIFICATION,
                                Snackbar.UMA_CHROME_FINDS_OPT_IN)
                        .setAction(
                                mContext.getString(
                                        R.string.chrome_finds_opt_in_snackbar_action_text),
                                null));
    }

    @VisibleForTesting
    void onOptInDeclined() {
        mUserInteractionType = FindsOptInUserInteraction.DECLINED;
        // Initialize the Chrome Finds notification channel as disabled.
        new ChannelsInitializer(
                        BaseNotificationManagerProxyFactory.create(),
                        ChromeChannelDefinitions.getInstance(),
                        mContext.getResources())
                .ensureInitializedAndDisabled(ChannelId.CHROME_FINDS);
        FindsUtils.setOptInPromoInteracted(mProfile);
        FindsMetrics.recordOptOutClicked();
        dismiss();
    }

    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
    }

    /** Shows the Chrome Finds opt-in bottom sheet. */
    public void showBottomSheet() {
        // Reset in case of stale mUserInteractionType value.
        mUserInteractionType = FindsOptInUserInteraction.DISMISSED;
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
        FindsUtils.setOptInPromoSeen(mProfile);
        FindsMetrics.recordOptInShown();
    }

    View getContentViewForTesting() {
        return mContentView;
    }

    FindsOptInBottomSheetContent getSheetContentForTesting() {
        return mSheetContent;
    }

    int getUserInteractionTypeForTesting() {
        return mUserInteractionType;
    }

    private void onBackPressed() {
        dismiss();
    }

    private void dismiss() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }

    @VisibleForTesting
    void scaleBottomSheetLottieAnimationByHeight(Configuration configuration) {
        int screenHeightPixels = ViewUtils.dpToPx(mContext, configuration.screenHeightDp);
        ViewGroup.LayoutParams layoutParams = mAnimationView.getLayoutParams();

        // Calculate the maximum allowed height.
        int maxHeight = Math.round(screenHeightPixels * LOTTIE_MAX_HEIGHT_RATIO);

        // Calculate the required width to achieve the target maxHeight based on the
        // Lottie animation's intrinsic aspect ratio.
        int targetWidth = Math.round(maxHeight * LOTTIE_INTRINSIC_ASPECT_RATIO);

        // Ensure the animation doesn't exceed the bottom sheet max width/screen width minus
        // horizontal margins.
        int screenWidthPixels = ViewUtils.dpToPx(mContext, configuration.screenWidthDp);
        int horizontalMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.finds_opt_in_bottom_sheet_horizontal_margin);
        int maxWidth = screenWidthPixels - (horizontalMargin * 2);
        int maxSheetWidth = mBottomSheetController.getMaxSheetWidth();
        if (maxSheetWidth > 0) {
            maxWidth = Math.min(maxWidth, maxSheetWidth - (horizontalMargin * 2));
        }

        layoutParams.width = Math.min(targetWidth, maxWidth);
        mAnimationView.setLayoutParams(layoutParams);
    }
}
