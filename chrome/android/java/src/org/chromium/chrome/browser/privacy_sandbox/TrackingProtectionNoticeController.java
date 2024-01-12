// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.privacy_sandbox.TrackingProtectionSettings;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Controller for the Notice message for Tracking Protection. */
public class TrackingProtectionNoticeController {
    private Context mContext;
    private ActivityTabProvider mActivityTabProvider;
    private ActivityTabTabObserver mActivityTabTabObserver;
    private MessageDispatcher mMessageDispatcher;
    private SettingsLauncher mSettingsLauncher;
    private static final String TRACKING_PROTECTION_HELP_CENTER =
            "https://support.google.com/chrome/?p=tracking_protection";

    // Setting an indefinite message auto dismiss duration is not possible,
    // hence we provide a value high enough to maintain the message visible.
    private static final int AUTODISMISS_DURATION_ONE_DAY = 24 * 60 * 60 * 1000;
    private static final int AUTODISMISS_DURATION_8_SECONDS = 8 * 1000;
    private PropertyModel mMessage;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        NoticeControllerEvent.CONTROLLER_CREATED,
        NoticeControllerEvent.ACTIVE_TAB_CHANGED,
        NoticeControllerEvent.NAVIGATION_FINISHED,
        NoticeControllerEvent.CONTROLLER_NO_LONGER_OBSERVING,
        NoticeControllerEvent.NON_SECURE_CONNECTION,
        NoticeControllerEvent.NOTICE_ALREADY_SHOWING,
        NoticeControllerEvent.NOTICE_REQUESTED_AND_SHOWN,
        NoticeControllerEvent.NOTICE_REQUESTED_BUT_NOT_SHOWN
    })
    public @interface NoticeControllerEvent {
        int CONTROLLER_CREATED = 0;
        int ACTIVE_TAB_CHANGED = 1;
        int NAVIGATION_FINISHED = 2;
        int CONTROLLER_NO_LONGER_OBSERVING = 3;
        int NON_SECURE_CONNECTION = 4;
        int NOTICE_ALREADY_SHOWING = 5;
        int NOTICE_REQUESTED_AND_SHOWN = 6;
        int NOTICE_REQUESTED_BUT_NOT_SHOWN = 7;

        int COUNT = 8;
    }

    public static final String NOTICE_CONTROLLER_EVENT_HISTOGRAM =
            "PrivacySandbox.TrackingProtection.Onboarding.NoticeControllerEvent";

    private static void logNoticeControllerEvent(@NoticeControllerEvent int action) {
        RecordHistogram.recordEnumeratedHistogram(
                NOTICE_CONTROLLER_EVENT_HISTOGRAM, action, NoticeControllerEvent.COUNT);
    }

    /**
     * Checks whether the Tracking Protection Notice should be shown.
     *
     * @return boolean value indicating if the Notice should be shown.
     */
    public static boolean shouldShowNotice() {
        return TrackingProtectionBridge.getRequiredNotice() != NoticeType.NONE;
    }

    /**
     * Creates and initializes the Notice controller. Registers an {@link ActivityTabTabObserver}
     * that will attempt to show the 3PCD Notice on an eligible tab in one of the following cases:
     * 1. An already loaded tab page is SECURE. 2. The newly loaded page on the current tab is
     * SECURE.
     *
     * @param activityTabProvider The provider of the current activity tab.
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the message.
     */
    public static TrackingProtectionNoticeController create(
            Context context,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher,
            @NonNull SettingsLauncher settingsLauncher) {
        return new TrackingProtectionNoticeController(
                context, activityTabProvider, messageDispatcher, settingsLauncher);
    }

    private TrackingProtectionNoticeController(
            Context context,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher,
            SettingsLauncher settingsLauncher) {
        mContext = context;
        mActivityTabProvider = activityTabProvider;
        mMessageDispatcher = messageDispatcher;
        mSettingsLauncher = settingsLauncher;

        createActivityTabTabObserver(tab -> showNotice());
    }

    private void showNotice() {
        if (mMessageDispatcher == null) return;

        Resources resources = mContext.getResources();

        if (mMessage != null) {
            logNoticeControllerEvent(NoticeControllerEvent.NOTICE_ALREADY_SHOWING);
        }

        if (getNoticeType() == NoticeType.SILENT_ONBOARDING) {
            TrackingProtectionBridge.noticeShown(getNoticeType());
            destroy();
            return;
        }

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.TRACKING_PROTECTION_NOTICE_REQUEST_TRACKING)) {
            // At this point, we're enqueuing the message, aka requesting the notice.
            TrackingProtectionBridge.noticeRequested(getNoticeType());
        }

        mMessage =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TRACKING_PROTECTION_NOTICE)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(
                                        getNoticeType() == NoticeType.ONBOARDING
                                                ? R.string
                                                        .tracking_protection_onboarding_notice_title
                                                : R.string
                                                        .tracking_protection_offboarding_notice_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                resources.getString(
                                        getNoticeType() == NoticeType.ONBOARDING
                                                ? R.string
                                                        .tracking_protection_onboarding_notice_body
                                                : R.string
                                                        .tracking_protection_offboarding_notice_body))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(
                                        getNoticeType() == NoticeType.ONBOARDING
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
                                getNoticeType() == NoticeType.ONBOARDING
                                        ? AUTODISMISS_DURATION_ONE_DAY
                                        : AUTODISMISS_DURATION_8_SECONDS)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    TrackingProtectionBridge.noticeActionTaken(
                                            getNoticeType(), NoticeAction.GOT_IT);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(MessageBannerProperties.ON_DISMISSED, onNoticeDismissed())
                        .with(MessageBannerProperties.ON_FULLY_VISIBLE, onNoticeShown())
                        .build();
        mMessageDispatcher.enqueueWindowScopedMessage(mMessage, /* highPriority= */ true);

        destroy();
    }

    private static Callback<Boolean> onNoticeShown() {
        return (shown) -> {
            if (shown) {
                TrackingProtectionBridge.noticeShown(getNoticeType());
                logNoticeControllerEvent(NoticeControllerEvent.NOTICE_REQUESTED_AND_SHOWN);
            }
        };
    }

    private static Callback<Integer> onNoticeDismissed() {
        return (dismissReason) -> {
            switch (dismissReason) {
                case DismissReason.GESTURE:
                    TrackingProtectionBridge.noticeActionTaken(
                            getNoticeType(), NoticeAction.CLOSED);
                    break;
                case DismissReason.PRIMARY_ACTION:
                case DismissReason.SECONDARY_ACTION:
                    // No need to report these actions: they are already recorded in their
                    // respective handlers.
                    break;
                default:
                    TrackingProtectionBridge.noticeActionTaken(getNoticeType(), NoticeAction.OTHER);
            }
        };
    }

    private void createActivityTabTabObserver(Callback showNoticeCallback) {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(mActivityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        logNoticeControllerEvent(NoticeControllerEvent.ACTIVE_TAB_CHANGED);
                        maybeShowNotice(tab);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        logNoticeControllerEvent(NoticeControllerEvent.NAVIGATION_FINISHED);
                        maybeShowNotice(tab);
                    }

                    private void maybeShowNotice(Tab tab) {
                        if (tab == null || tab.isIncognito()) return;

                        int securityLevel =
                                SecurityStateModel.getSecurityLevelForWebContents(
                                        tab.getWebContents());

                        // TODO(b/304202327): Offboarding notice should skip the non secure pages
                        // check.
                        if (shouldShowNotice()
                                && (ChromeFeatureList.isEnabled(
                                                ChromeFeatureList
                                                        .TRACKING_PROTECTION_ONBOARDING_SKIP_SECURE_PAGE_CHECK)
                                        || securityLevel == ConnectionSecurityLevel.SECURE)) {
                            showNoticeCallback.onResult(tab);
                        } else if (shouldShowNotice()) {
                            if (securityLevel != ConnectionSecurityLevel.SECURE) {
                                logNoticeControllerEvent(
                                        NoticeControllerEvent.NON_SECURE_CONNECTION);
                            }
                            logNoticeControllerEvent(
                                    NoticeControllerEvent.NOTICE_REQUESTED_BUT_NOT_SHOWN);
                        }
                    }
                };
        logNoticeControllerEvent(NoticeControllerEvent.CONTROLLER_CREATED);
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
                                    getNoticeType() == NoticeType.ONBOARDING
                                            ? R.string
                                                    .tracking_protection_onboarding_notice_settings_button_label
                                            : R.string
                                                    .tracking_protection_offboarding_notice_settings_button_label));
            ListItem learnMoreItem =
                    getMenuItem(
                            LEARN_MORE_ITEM_ID,
                            res.getString(
                                    getNoticeType() == NoticeType.ONBOARDING
                                            ? R.string
                                                    .tracking_protection_onboarding_notice_learn_more_button_label
                                            : R.string
                                                    .tracking_protection_offboarding_notice_learn_more_button_label),
                            res.getString(
                                    getNoticeType() == NoticeType.ONBOARDING
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
            return (clickedItem) -> {
                int clickedItemID = clickedItem.get(ListMenuItemProperties.MENU_ITEM_ID);

                if (clickedItemID == SETTINGS_ITEM_ID) {
                    if (getNoticeType() == NoticeType.ONBOARDING) {
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

                    TrackingProtectionBridge.noticeActionTaken(
                            getNoticeType(),
                            org.chromium.chrome.browser.privacy_sandbox.NoticeAction.SETTINGS);
                } else if (clickedItemID == LEARN_MORE_ITEM_ID) {
                    openUrlInCct(TRACKING_PROTECTION_HELP_CENTER);
                    TrackingProtectionBridge.noticeActionTaken(
                            getNoticeType(),
                            org.chromium.chrome.browser.privacy_sandbox.NoticeAction.LEARN_MORE);
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

    private static @NoticeType int getNoticeType() {
        return TrackingProtectionBridge.getRequiredNotice();
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            logNoticeControllerEvent(NoticeControllerEvent.CONTROLLER_NO_LONGER_OBSERVING);
        }
    }
}
