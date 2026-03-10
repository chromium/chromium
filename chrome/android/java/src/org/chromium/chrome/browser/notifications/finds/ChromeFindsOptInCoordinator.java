// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.finds;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.notifications.finds.ChromeFindsUtils.ChromeFindsOptInState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.ui.widget.ButtonCompat;

/** Coordinator for the Chrome Finds opt-in bottom sheet. */
@NullMarked
public class ChromeFindsOptInCoordinator {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final SnackbarManager mSnackbarManager;
    private final ChromeFindsOptInBottomSheetContent mSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;
    private final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
            ObservableSuppliers.createNonNull(false);

    /**
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     * @param snackbarManager The system {@link SnackbarManager}.
     */
    public ChromeFindsOptInCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mSnackbarManager = snackbarManager;

        View contentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.chrome_finds_opt_in_bottom_sheet, /* root= */ null);
        mSheetContent =
                new ChromeFindsOptInBottomSheetContent(
                        contentView, this::onBackPressed, mBackPressStateChangedSupplier);

        ButtonCompat positiveButtonView = contentView.findViewById(R.id.opt_in_positive_button);
        positiveButtonView.setOnClickListener(
                (view) -> {
                    onOptInAccepted();
                    dismiss();
                });

        ButtonCompat negativeButtonView = contentView.findViewById(R.id.opt_in_negative_button);
        negativeButtonView.setOnClickListener(
                (view) -> {
                    onOptInDeclined();
                    dismiss();
                });

        mBottomSheetObserver =
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
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    @VisibleForTesting
    void onOptInAccepted() {
        ChromeFindsUtils.getOptInState(
                (state) -> {
                    if (state == ChromeFindsOptInState.FIRST_TIME) {
                        // For first time opt-in, initialize the notification channel as enabled.
                        new ChannelsInitializer(
                                        BaseNotificationManagerProxyFactory.create(),
                                        ChromeChannelDefinitions.getInstance(),
                                        mContext.getResources())
                                .ensureInitialized(ChannelId.CHROME_FINDS);

                        // If app-level notifications are disabled, we must direct the user to
                        // settings to enable them. Otherwise, we show the confirmation snackbar.
                        if (NotificationProxyUtils.areNotificationsEnabled()) {
                            showOptInSnackbar();
                        } else {
                            ChromeFindsUtils.launchFindsNotificationSettings(mContext);
                        }
                        ChromeFindsMetrics.recordOptInAccepted(/* firstTime= */ true);
                    } else if (state == ChromeFindsOptInState.MANUALLY_DISABLED) {
                        // Launch the notifications settings to direct the user to manually enable
                        // since once the channel has already been created, we cannot
                        // programmatically enable it.
                        ChromeFindsUtils.launchFindsNotificationSettings(mContext);
                        ChromeFindsMetrics.recordOptInAccepted(/* firstTime= */ false);
                    } else {
                        // The only other remaining state is ENABLED, which in theory should never
                        // occur but will be possible with always show opt-in enabled since the
                        // opt-in bottom sheet should not be shown otherwise.
                        showOptInSnackbar();
                        ChromeFindsMetrics.recordOptInAccepted(/* firstTime= */ false);
                    }
                });
    }

    private void showOptInSnackbar() {
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                mContext.getString(R.string.chrome_finds_opt_in_snackbar_message),
                                new SnackbarController() {
                                    @Override
                                    public void onAction(@Nullable Object actionData) {
                                        ChromeFindsUtils.launchFindsNotificationSettings(mContext);
                                        ChromeFindsMetrics.recordSnackbarActionClicked();
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
        // Initialize the Chrome Finds notification channel as disabled.
        new ChannelsInitializer(
                        BaseNotificationManagerProxyFactory.create(),
                        ChromeChannelDefinitions.getInstance(),
                        mContext.getResources())
                .ensureInitializedAndDisabled(ChannelId.CHROME_FINDS);
        // Write to ChromeSharedPreferences that the user has already declined the Chrome
        // Finds feature, the user should never see the opt-in bottom sheet again.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CHROME_FINDS_OPT_IN_PROMO_DECLINED, true);
        ChromeFindsMetrics.recordOptOutClicked();
    }

    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    /** Shows the Chrome Finds opt-in bottom sheet. */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
        ChromeFindsMetrics.recordOptInShown();
    }

    private void onBackPressed() {
        dismiss();
    }

    private void dismiss() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }
}
