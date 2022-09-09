// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_guide.notifications;

import android.content.Intent;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.StringRes;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoDeps;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Various utility methods needed by the feature notification guide and external clients to show
 * IPH.
 */
public final class FeatureNotificationUtils {
    /**
     * The {@link FeatureType} of the feature being promoed through the notification.
     */
    public static final String EXTRA_FEATURE_TYPE = "feature_notification_guide_feature_type";

    private static final String NOTIFICATION_PARAM_GUID_DEFAULT_BROWSER = "guid_default_browser";
    private static final String NOTIFICATION_PARAM_GUID_SIGN_IN = "guid_sign_in";
    private static final String NOTIFICATION_PARAM_GUID_INCOGNITO_TAB = "guid_incognito_tab";
    private static final String NOTIFICATION_PARAM_GUID_NTP_SUGGESTION_CARD =
            "guid_ntp_suggestion_card";
    private static final String NOTIFICATION_PARAM_GUID_VOICE_SEARCH = "guid_voice_search";

    /**
     * Helper method used by activities to check if the incoming intent is from a feature
     * notification click. Initiates a request to the IPH system to show the corresponding IPH.
     */
    public static void handleIntentIfApplicable(Intent intent) {
        int featureType =
                IntentUtils.safeGetIntExtra(intent, EXTRA_FEATURE_TYPE, FeatureType.INVALID);
        if (featureType == FeatureType.INVALID) return;

        TextBubble.dismissBubbles();
        new Handler().post(() -> {
            Tracker tracker =
                    TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
            tracker.setPriorityNotification(
                    FeatureNotificationUtils.getIPHFeatureForNotificationFeatureType(featureType));
        });
    }

    /**
     * Helper method to register an IPH show callback for the feature type to show the IPH.
     */
    public static void registerIPHCallback(@FeatureType int featureType, Runnable showIphCallback) {
        if (!ProfileManager.isInitialized()) return;
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        tracker.registerPriorityNotificationHandler(
                getIPHFeatureForNotificationFeatureType(featureType), showIphCallback);
    }

    /**
     * Unregisters any IPH callbacks associated with the feature type.
     */
    public static void unregisterIPHCallback(@FeatureType int featureType) {
        if (!ProfileManager.isInitialized()) return;
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        tracker.unregisterPriorityNotificationHandler(
                getIPHFeatureForNotificationFeatureType(featureType));
    }

    /**
     * @return Whether an IPH is currently scheduled to show. This means the user has tapped
     *         notifications and we are in the process of bringing up the target UI surface.
     */
    public static boolean willShowIPH(@FeatureType int featureType) {
        String iphFeature = getIPHFeatureForNotificationFeatureType(featureType);
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        return TextUtils.equals(tracker.getPendingPriorityNotification(), iphFeature);
    }

    /* package */ static String getNotificationTitle(@FeatureType int featureType) {
        return ContextUtils.getApplicationContext().getResources().getString(
                R.string.feature_notification_guide_notification_title);
    }

    /* package */ static String getNotificationMessage(@FeatureType int featureType) {
        @StringRes
        int stringId = 0;
        switch (featureType) {
            case FeatureType.DEFAULT_BROWSER:
                stringId = R.string.feature_notification_guide_notification_message_default_browser;
                break;
            case FeatureType.SIGN_IN:
                stringId = R.string.feature_notification_guide_notification_message_sign_in;
                break;
            case FeatureType.INCOGNITO_TAB:
                stringId = R.string.feature_notification_guide_notification_message_incognito_tab;
                break;
            case FeatureType.NTP_SUGGESTION_CARD:
                stringId =
                        R.string.feature_notification_guide_notification_message_ntp_suggestion_card;
                break;
            case FeatureType.VOICE_SEARCH:
                stringId = R.string.feature_notification_guide_notification_message_voice_search;
                break;
            default:
                assert false : "Found unknown feature type " + featureType;
                break;
        }
        return ContextUtils.getApplicationContext().getResources().getString(stringId);
    }

    private static String getIPHFeatureForNotificationFeatureType(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.DEFAULT_BROWSER:
                return FeatureConstants.FEATURE_NOTIFICATION_GUIDE_DEFAULT_BROWSER_PROMO_FEATURE;
            case FeatureType.SIGN_IN:
                return FeatureConstants.FEATURE_NOTIFICATION_GUIDE_SIGN_IN_HELP_BUBBLE_FEATURE;
            case FeatureType.INCOGNITO_TAB:
                return FeatureConstants
                        .FEATURE_NOTIFICATION_GUIDE_INCOGNITO_TAB_HELP_BUBBLE_FEATURE;
            case FeatureType.NTP_SUGGESTION_CARD:
                return FeatureConstants
                        .FEATURE_NOTIFICATION_GUIDE_NTP_SUGGESTION_CARD_HELP_BUBBLE_FEATURE;
            case FeatureType.VOICE_SEARCH:
                return FeatureConstants.FEATURE_NOTIFICATION_GUIDE_VOICE_SEARCH_HELP_BUBBLE_FEATURE;
            default:
                assert false : "Found unknown feature type " + featureType;
                break;
        }
        return FeatureConstants.FEATURE_NOTIFICATION_GUIDE_INCOGNITO_TAB_HELP_BUBBLE_FEATURE;
    }

    /* package */ static String getNotificationParamGuidForFeature(@FeatureType int featureType) {
        switch (featureType) {
            case FeatureType.DEFAULT_BROWSER:
                return NOTIFICATION_PARAM_GUID_DEFAULT_BROWSER;
            case FeatureType.SIGN_IN:
                return NOTIFICATION_PARAM_GUID_SIGN_IN;
            case FeatureType.INCOGNITO_TAB:
                return NOTIFICATION_PARAM_GUID_INCOGNITO_TAB;
            case FeatureType.NTP_SUGGESTION_CARD:
                return NOTIFICATION_PARAM_GUID_NTP_SUGGESTION_CARD;
            case FeatureType.VOICE_SEARCH:
                return NOTIFICATION_PARAM_GUID_VOICE_SEARCH;
            default:
                assert false : "Found unknown feature type " + featureType;
                return "";
        }
    }

    /* package */ static void closeNotification(String notificationGuid) {
        int notificationId = notificationGuid.hashCode();
        NotificationManagerProxy notificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        notificationManager.cancel(notificationId);
    }

    /**
     * Called to determine whether the feature should be skipped from notification as it is
     * ineligible or not available on the device.
     * @param featureType The given feature type.
     * @return True if the feature should be skipped, false otherwise.
     */
    public static boolean shouldSkipFeature(@FeatureType int featureType) {
        if (featureType == FeatureType.DEFAULT_BROWSER) {
            return !shouldShowDefaultBrowserPromo();
        } else if (featureType == FeatureType.NTP_SUGGESTION_CARD) {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            return !prefService.getBoolean(Pref.ARTICLES_LIST_VISIBLE);
        }
        return false;
    }

    private static boolean shouldShowDefaultBrowserPromo() {
        DefaultBrowserPromoDeps deps = DefaultBrowserPromoDeps.getInstance();
        return DefaultBrowserPromoUtils.shouldShowPromo(
                deps, ContextUtils.getApplicationContext(), true /* ignoreMaxCount */);
    }
}
