// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
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
 * View responsible for displaying the Tracking Protection notice UI (Tracking protection launch for
 * 100% of users).
 */
class TrackingProtectionOnboardingView {
    private static final String TRACKING_PROTECTION_HELP_CENTER =
            "https://support.google.com/chrome/?p=tracking_protection";
    private static final int AUTODISMISS_DURATION_ONE_DAY = 24 * 60 * 60 * 1000;
    private final Context mContext;
    private final MessageDispatcher mMessageDispatcher;
    private final SettingsLauncher mSettingsLauncher;

    private PropertyModel mMessage;

    /**
     * Creates a TrackingProtectionNoticeView.
     *
     * @param context The application context.
     * @param messageDispatcher The message dispatcher for enqueuing messages.
     * @param settingsLauncher The settings launcher for opening tracking protection settings.
     *     logic.
     */
    public TrackingProtectionOnboardingView(
            Context context,
            MessageDispatcher messageDispatcher,
            SettingsLauncher settingsLauncher) {
        mContext = context;
        mMessageDispatcher = messageDispatcher;
        mSettingsLauncher = settingsLauncher;
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
            Supplier<Integer> noticePrimaryActioSupplier,
            @NoticeType int noticeType) {
        Resources resources = mContext.getResources();
        mMessage =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TRACKING_PROTECTION_NOTICE)
                        .with(MessageBannerProperties.TITLE, getTitleString())
                        .with(MessageBannerProperties.DESCRIPTION, getBodyStrings(noticeType))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(
                                        R.string
                                                .tracking_protection_full_onboarding_notice_ack_button_label))
                        .with(
                                MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE,
                                new SecondaryMenuButtonDelegate())
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.ic_eye_crossed)
                        .with(
                                MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                R.drawable.ic_settings_gear_24dp)
                        .with(
                                MessageBannerProperties.DISMISSAL_DURATION,
                                AUTODISMISS_DURATION_ONE_DAY)
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

    private String getTitleString() {
        Resources resources = mContext.getResources();
        return resources.getString(R.string.tracking_protection_full_onboarding_notice_title);
    }

    private String getBodyStrings(int noticeType) {
        Resources resources = mContext.getResources();
        return switch (noticeType) {
            case NoticeType.MODE_B_ONBOARDING -> resources.getString(
                    R.string.tracking_protection_onboarding_notice_body);
            case NoticeType.FULL3PCD_ONBOARDING -> resources.getString(
                    R.string.tracking_protection_full_onboarding_notice_body);
            case NoticeType.FULL3PCD_ONBOARDING_WITH_IPP -> resources.getString(
                    R.string.tracking_protection_full_onboarding_ipp_notice_body);
            default -> resources.getString(
                    R.string.tracking_protection_full_onboarding_all_notice_body);
        };
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
            ListItem settingsItem =
                    getMenuItem(
                            SETTINGS_ITEM_ID,
                            res.getString(
                                    R.string
                                            .tracking_protection_full_onboarding_notice_settings_button_label));
            ListItem learnMoreItem =
                    getMenuItem(
                            LEARN_MORE_ITEM_ID,
                            res.getString(
                                    R.string
                                            .tracking_protection_full_onboarding_notice_learn_more_button_label),
                            res.getString(
                                    R.string
                                            .tracking_protection_full_onboarding_notice_learn_more_button_a11y_label));

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
            return (clickedItem) -> {
                int clickedItemID = clickedItem.get(ListMenuItemProperties.MENU_ITEM_ID);

                if (clickedItemID == SETTINGS_ITEM_ID) {
                    mSettingsLauncher.launchSettingsActivity(
                            mContext, TrackingProtectionSettings.class);
                } else if (clickedItemID == LEARN_MORE_ITEM_ID) {
                    new CctHandler(mContext)
                            .prepareIntent(TRACKING_PROTECTION_HELP_CENTER)
                            .openUrlInCct();
                }

                mMessageDispatcher.dismissMessage(mMessage, DismissReason.SECONDARY_ACTION);
            };
        }
    }
}
