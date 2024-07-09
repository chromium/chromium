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
import org.chromium.chrome.browser.profiles.Profile;
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

/**
 * @deprecated use {@link TrackingProtectionOnboardingController}.
 *     <p>Controller for the Notice message for Tracking Protection.
 *     <p>TODO(b/341968245): remove all the usages of this class
 */
public class TrackingProtectionNoticeController {
    private final Context mContext;
    private final TrackingProtectionBridge mTrackingProtectionBridge;
    private final ActivityTabProvider mActivityTabProvider;
    private ActivityTabTabObserver mActivityTabTabObserver;
    private final MessageDispatcher mMessageDispatcher;
    private final SettingsLauncher mSettingsLauncher;
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
     * @param profile The {@link Profile} associated with the tracking protection state.
     * @return boolean value indicating if the Notice should be shown.
     */
    public static boolean shouldShowNotice(Profile profile) {
        return shouldShowNotice(new TrackingProtectionBridge(profile));
    }

    private static boolean shouldShowNotice(TrackingProtectionBridge trackingProtectionBridge) {
        return trackingProtectionBridge.getRequiredNotice(SurfaceType.BR_APP) != NoticeType.NONE;
    }

    /**
     * Creates and initializes the Notice controller. Registers an {@link ActivityTabTabObserver}
     * that will attempt to show the 3PCD Notice on an eligible tab in one of the following cases:
     * 1. An already loaded tab page is SECURE. 2. The newly loaded page on the current tab is
     * SECURE.
     *
     * @param profile The {@link Profile} associated with the tracking protection state.
     * @param activityTabProvider The provider of the current activity tab.
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the message.
     */
    public static TrackingProtectionNoticeController create(
            Context context,
            Profile profile,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher,
            @NonNull SettingsLauncher settingsLauncher) {
        return new TrackingProtectionNoticeController(
                context,
                new TrackingProtectionBridge(profile),
                activityTabProvider,
                messageDispatcher,
                settingsLauncher);
    }

    private TrackingProtectionNoticeController(
            Context context,
            TrackingProtectionBridge trackingProtectionBridge,
            ActivityTabProvider activityTabProvider,
            MessageDispatcher messageDispatcher,
            SettingsLauncher settingsLauncher) {
        mContext = context;
        mTrackingProtectionBridge = trackingProtectionBridge;
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

        if (getNoticeType() == NoticeType.MODE_B_SILENT_ONBOARDING) {
            mTrackingProtectionBridge.noticeShown(SurfaceType.BR_APP, getNoticeType());
            destroy();
            return;
        }

        mMessage =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TRACKING_PROTECTION_NOTICE)
                        .with(
                                MessageBannerProperties.TITLE,
                                resources.getString(
                                        R.string.tracking_protection_onboarding_notice_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                resources.getString(
                                        R.string.tracking_protection_onboarding_notice_body))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(
                                        R.string
                                                .tracking_protection_onboarding_notice_ack_button_label))
                        .with(
                                MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE,
                                new SecondaryMenuButtonDelegate())
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.ic_eye_crossed)
                        .with(
                                MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                R.drawable.ic_settings_gear_24dp)
                        .with(
                                MessageBannerProperties.DISMISSAL_DURATION,
                                getNoticeType() == NoticeType.MODE_B_ONBOARDING
                                        ? AUTODISMISS_DURATION_ONE_DAY
                                        : AUTODISMISS_DURATION_8_SECONDS)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    mTrackingProtectionBridge.noticeActionTaken(
                                            SurfaceType.BR_APP,
                                            getNoticeType(),
                                            NoticeAction.GOT_IT);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(MessageBannerProperties.ON_DISMISSED, onNoticeDismissed())
                        .with(MessageBannerProperties.ON_FULLY_VISIBLE, onNoticeShown())
                        .build();
        mMessageDispatcher.enqueueWindowScopedMessage(mMessage, /* highPriority= */ true);

        destroy();
    }

    private Callback<Boolean> onNoticeShown() {
        return (shown) -> {
            if (shown) {
                mTrackingProtectionBridge.noticeShown(SurfaceType.BR_APP, getNoticeType());
                logNoticeControllerEvent(NoticeControllerEvent.NOTICE_REQUESTED_AND_SHOWN);
            }
        };
    }

    private Callback<Integer> onNoticeDismissed() {
        return (dismissReason) -> {
            switch (dismissReason) {
                case DismissReason.GESTURE:
                    mTrackingProtectionBridge.noticeActionTaken(
                            SurfaceType.BR_APP, getNoticeType(), NoticeAction.CLOSED);
                    break;
                case DismissReason.PRIMARY_ACTION:
                case DismissReason.SECONDARY_ACTION:
                    // No need to report these actions: they are already recorded in their
                    // respective handlers.
                    break;
                case DismissReason.SCOPE_DESTROYED:
                    // When the scope is destroyed, we do nothing as we want the user to be
                    // shown the notice again.
                    break;
                default:
                    mTrackingProtectionBridge.noticeActionTaken(
                            SurfaceType.BR_APP, getNoticeType(), NoticeAction.OTHER);
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

                        if (shouldShowNotice(mTrackingProtectionBridge) &&
                                securityLevel == ConnectionSecurityLevel.SECURE) {
                              showNoticeCallback.onResult(tab);
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
                                    R.string
                                            .tracking_protection_onboarding_notice_settings_button_label));
            ListItem learnMoreItem =
                    getMenuItem(
                            LEARN_MORE_ITEM_ID,
                            res.getString(
                                    R.string
                                            .tracking_protection_onboarding_notice_learn_more_button_label),
                            res.getString(
                                    R.string
                                            .tracking_protection_onboarding_notice_learn_more_button_a11y_label));

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
                    if (getNoticeType() == NoticeType.MODE_B_ONBOARDING) {
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

                    mTrackingProtectionBridge.noticeActionTaken(
                            SurfaceType.BR_APP,
                            getNoticeType(),
                            org.chromium.chrome.browser.privacy_sandbox.NoticeAction.SETTINGS);
                } else if (clickedItemID == LEARN_MORE_ITEM_ID) {
                    openUrlInCct(TRACKING_PROTECTION_HELP_CENTER);
                    mTrackingProtectionBridge.noticeActionTaken(
                            SurfaceType.BR_APP,
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

    private @NoticeType int getNoticeType() {
        return mTrackingProtectionBridge.getRequiredNotice(SurfaceType.BR_APP);
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            logNoticeControllerEvent(NoticeControllerEvent.CONTROLLER_NO_LONGER_OBSERVING);
        }
    }
}
