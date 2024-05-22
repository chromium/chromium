// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.privacy_sandbox.TrackingProtectionSettings;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View responsible for displaying the Tracking Protection notice UI for Mode B (Tracking protection
 * launch for 1% of users).
 */
class TrackingProtectionModeBOnboardingView {
    private static final String TRACKING_PROTECTION_HELP_CENTER =
            "https://support.google.com/chrome/?p=tracking_protection";
    private static final int AUTODISMISS_DURATION_ONE_DAY = 24 * 60 * 60 * 1000;
    private static final int AUTODISMISS_DURATION_8_SECONDS = 8 * 1000;
    private final Context mContext;
    private final MessageDispatcher mMessageDispatcher;
    private final SettingsLauncher mSettingsLauncher;
    private final TrackingProtectionBridge mTrackingProtectionBridge;

    private PropertyModel mMessage;

    /**
     * Creates a TrackingProtectionNoticeView.
     *
     * @param context The application context.
     * @param messageDispatcher The message dispatcher for enqueuing messages.
     * @param settingsLauncher The settings launcher for opening tracking protection settings.
     * @param trackingProtectionBridge The bridge to interact with Tracking Protection business
     *     logic.
     */
    public TrackingProtectionModeBOnboardingView(
            Context context,
            MessageDispatcher messageDispatcher,
            SettingsLauncher settingsLauncher,
            TrackingProtectionBridge trackingProtectionBridge) {
        mContext = context;
        mMessageDispatcher = messageDispatcher;
        mSettingsLauncher = settingsLauncher;
        mTrackingProtectionBridge = trackingProtectionBridge;
    }

    /**
     * Shows the tracking protection notice.
     *
     * @param noticeType The type of notice to show.
     * @param noticeRequestedCallback A callback to be called when the notice is requested.
     * @param noticeShownCallback A callback to be called when the notice is fully shown.
     * @param noticeActionTakenCallback A callback to be called when an action is taken on the
     *     notice.
     */
    public void showNotice(
            Callback<Boolean> noticeShownCallback,
            Callback<Integer> noticeDismissedCallback,
            Supplier<Integer> noticePrimaryActioSupplier) {
        Resources resources = mContext.getResources();
        int noticeType = mTrackingProtectionBridge.getRequiredNotice();
        mMessage =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TRACKING_PROTECTION_NOTICE)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(
                                        noticeType == NoticeType.ONBOARDING
                                                ? R.string
                                                        .tracking_protection_onboarding_notice_title
                                                : R.string
                                                        .tracking_protection_offboarding_notice_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                resources.getString(
                                        noticeType == NoticeType.ONBOARDING
                                                ? R.string
                                                        .tracking_protection_onboarding_notice_body
                                                : R.string
                                                        .tracking_protection_offboarding_notice_body))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(
                                        noticeType == NoticeType.ONBOARDING
                                                ? R.string
                                                        .tracking_protection_onboarding_notice_ack_button_label
                                                : R.string
                                                        .tracking_protection_offboarding_notice_ack_button_label))
                        .with(
                                MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE,
                                new SecondaryMenuButtonDelegate())
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.ic_eye_crossed)
                        .with(
                                MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                R.drawable.ic_settings_gear_24dp)
                        .with(
                                MessageBannerProperties.DISMISSAL_DURATION,
                                noticeType == NoticeType.ONBOARDING
                                        ? AUTODISMISS_DURATION_ONE_DAY
                                        : AUTODISMISS_DURATION_8_SECONDS)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                noticePrimaryActioSupplier::get)
                        .with(
                                MessageBannerProperties.ON_DISMISSED,
                                noticeDismissedCallback::onResult)
                        .with(
                                MessageBannerProperties.ON_FULLY_VISIBLE,
                                noticeShownCallback::onResult)
                        .build();
        mMessageDispatcher.enqueueWindowScopedMessage(mMessage, /* highPriority= */ true);
    }

    /**
     * Checks if the notice was already requested.
     *
     * @return true if the notice was requested, false otherwise.
     */
    public boolean wasNoticeRequested() {
        return mMessage != null;
    }

    private final class SecondaryMenuButtonDelegate implements ListMenuButtonDelegate {

        private static final int SETTINGS_ITEM_ID = 1;
        private static final int LEARN_MORE_ITEM_ID = 2;

        @Override
        public ListMenu getListMenu() {
            Resources res = mContext.getResources();
            int noticeType = mTrackingProtectionBridge.getRequiredNotice();
            ListItem settingsItem =
                    getMenuItem(
                            SETTINGS_ITEM_ID,
                            res.getString(
                                    noticeType == NoticeType.ONBOARDING
                                            ? R.string
                                                    .tracking_protection_onboarding_notice_settings_button_label
                                            : R.string
                                                    .tracking_protection_offboarding_notice_settings_button_label));
            ListItem learnMoreItem =
                    getMenuItem(
                            LEARN_MORE_ITEM_ID,
                            res.getString(
                                    noticeType == NoticeType.ONBOARDING
                                            ? R.string
                                                    .tracking_protection_onboarding_notice_learn_more_button_label
                                            : R.string
                                                    .tracking_protection_offboarding_notice_learn_more_button_label),
                            res.getString(
                                    noticeType == NoticeType.ONBOARDING
                                            ? R.string
                                                    .tracking_protection_onboarding_notice_learn_more_button_a11y_label
                                            : R.string
                                                    .tracking_protection_offboarding_notice_learn_more_button_a11y_label));

            MVCListAdapter.ModelList menuItems = new MVCListAdapter.ModelList();
            menuItems.add(settingsItem);
            menuItems.add(learnMoreItem);

            BasicListMenu listMenu =
                    BrowserUiListMenuUtils.getBasicListMenu(mContext, menuItems, onClickDelegate());

            return listMenu;
        }

        private ListItem getMenuItem(int itemID, String title) {
            return BrowserUiListMenuUtils.buildMenuListItem(title, itemID, 0, true);
        }

        private ListItem getMenuItem(int itemID, String title, String contentDescription) {
            return BrowserUiListMenuUtils.buildMenuListItem(
                    title, itemID, 0, contentDescription, true);
        }

        private Delegate onClickDelegate() {
            int noticeType = mTrackingProtectionBridge.getRequiredNotice();
            return (clickedItem) -> {
                int clickedItemID = clickedItem.get(ListMenuItemProperties.MENU_ITEM_ID);

                if (clickedItemID == SETTINGS_ITEM_ID) {
                    if (noticeType == NoticeType.ONBOARDING) {
                        mSettingsLauncher.launchSettingsActivity(
                                mContext, TrackingProtectionSettings.class);
                    } else {
                        Bundle fragmentArguments = new Bundle();
                        fragmentArguments.putString(
                                SingleCategorySettings.EXTRA_CATEGORY,
                                SiteSettingsCategory.preferenceKey(
                                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES));
                        fragmentArguments.putString(
                                SingleCategorySettings.EXTRA_TITLE,
                                mContext.getResources()
                                        .getString(R.string.third_party_cookies_page_title));
                        mSettingsLauncher.launchSettingsActivity(
                                mContext, SingleCategorySettings.class, fragmentArguments);
                    }

                    mTrackingProtectionBridge.noticeActionTaken(noticeType, NoticeAction.SETTINGS);
                } else if (clickedItemID == LEARN_MORE_ITEM_ID) {
                    openUrlInCct(TRACKING_PROTECTION_HELP_CENTER);
                    mTrackingProtectionBridge.noticeActionTaken(
                            noticeType, NoticeAction.LEARN_MORE);
                }

                mMessageDispatcher.dismissMessage(mMessage, DismissReason.SECONDARY_ACTION);
            };
        }
    }

    private void openUrlInCct(String url) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        mContext, customTabIntent.intent);
        intent.setPackage(mContext.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(mContext, intent);
    }
}
