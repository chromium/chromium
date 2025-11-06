// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.ui.widget.ButtonCompat;

/** Coordinator for the Tips Opt In bottom sheet. */
@NullMarked
public class TipsOptInCoordinator {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final TipsOptInSheetContent mSheetContent;

    /**
     * Constructor.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The system {@link BottomSheetController}.
     */
    public TipsOptInCoordinator(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;

        View contentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.tips_opt_in_bottom_sheet, /* root= */ null);
        mSheetContent = new TipsOptInSheetContent(contentView);

        ButtonCompat positiveButtonView = contentView.findViewById(R.id.opt_in_positive_button);
        positiveButtonView.setOnClickListener(
                (view) -> {
                    launchNotificationSettings();
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                });

        ButtonCompat negativeButtonView = contentView.findViewById(R.id.opt_in_negative_button);
        negativeButtonView.setOnClickListener(
                (view) -> {
                    mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                });
    }

    /** Cleans up resources. */
    public void destroy() {}

    /** Shows the promo. The caller is responsible for all eligibility checks. */
    public void showBottomSheet() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);

        // Mark that the promo has been shown.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_PROMO_SHOWN, true);
    }

    private Intent getNotificationSettingsIntent() {
        Intent intent = new Intent();
        if (areAppNotificationsEnabled()) {
            intent.setAction(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, mContext.getPackageName());
            intent.putExtra(Settings.EXTRA_CHANNEL_ID, ChromeChannelDefinitions.ChannelId.TIPS);
        } else {
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, mContext.getPackageName());
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    private boolean areAppNotificationsEnabled() {
        return NotificationProxyUtils.areNotificationsEnabled();
    }

    private void createNotificationChannel() {
        new ChannelsInitializer(
                        BaseNotificationManagerProxyFactory.create(),
                        ChromeChannelDefinitions.getInstance(),
                        mContext.getResources())
                .ensureInitialized(ChromeChannelDefinitions.ChannelId.TIPS);
    }

    private void launchNotificationSettings() {
        // Make sure the channel is initialized before sending users to the settings.
        createNotificationChannel();
        mContext.startActivity(getNotificationSettingsIntent());
    }

    @NullMarked
    private class TipsOptInSheetContent implements BottomSheetContent {
        private final View mContentView;

        TipsOptInSheetContent(View contentView) {
            mContentView = contentView;
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
        public void onBackPressed() {}

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
}
