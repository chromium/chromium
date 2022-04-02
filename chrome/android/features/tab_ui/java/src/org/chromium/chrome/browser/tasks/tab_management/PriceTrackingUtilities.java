// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceConfig;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.ModelType;

import java.util.concurrent.TimeUnit;

/**
 * A class to handle price tracking-related features.
 */
public class PriceTrackingUtilities {
    @VisibleForTesting
    public static final String PRICE_TRACKING_PARAM = "enable_price_tracking";
    @VisibleForTesting
    public static final String PRICE_NOTIFICATION_PARAM = "enable_price_notification";
    @VisibleForTesting
    public static final String ALLOW_DISABLE_PRICE_ANNOTATIONS_PARAM =
            "allow_disable_price_annotations";
    public static final String TRACK_PRICES_ON_TABS =
            ChromePreferenceKeys.PRICE_TRACKING_TRACK_PRICES_ON_TABS;
    @VisibleForTesting
    public static final String PRICE_WELCOME_MESSAGE_CARD =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD;
    @VisibleForTesting
    public static final String PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT;
    @VisibleForTesting
    public static final String PRICE_ALERTS_MESSAGE_CARD =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD;
    @VisibleForTesting
    public static final String PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT;
    private static final String PRICE_ANNOTATIONS_ENABLED_METRICS_WINDOW_DURATION_PARAM =
            "price_annotations_enabled_metrics_window_duration_ms";

    @VisibleForTesting
    public static final SharedPreferencesManager SHARED_PREFERENCES_MANAGER =
            SharedPreferencesManager.getInstance();

    private static Boolean sIsSignedInAndSyncEnabledForTesting;

    /**
     * @return whether or not price tracking is enabled.
     */
    public static boolean getPriceTrackingEnabled() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, PRICE_TRACKING_PARAM, false);
        }
        return false;
    }

    /**
     * @return whether or not price tracking notifications are enabled.
     */
    public static boolean getPriceTrackingNotificationsEnabled() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, PRICE_NOTIFICATION_PARAM, false);
        }
        return false;
    }

    /**
     * @return Whether the price tracking feature is eligible to work. Now it is used to determine
     *         whether the menu item "track prices" is visible and whether the tab has {@link
     *         TabProperties#SHOPPING_PERSISTED_TAB_DATA_FETCHER}.
     */
    public static boolean isPriceTrackingEligible() {
        if (sIsSignedInAndSyncEnabledForTesting != null) {
            return isPriceTrackingEnabled() && sIsSignedInAndSyncEnabledForTesting;
        }
        return isPriceTrackingEnabled() && isSignedIn() && isAnonymizedUrlDataCollectionEnabled();
    }

    /**
     * @return Whether the price tracking feature is enabled and available for use.
     */
    public static boolean isPriceTrackingEnabled() {
        return getPriceTrackingEnabled() || getPriceTrackingNotificationsEnabled();
    }

    // TODO(crbug.com/1307949): Clean up this api.
    @Deprecated
    public static void flipTrackPricesOnTabs() {
        final boolean enableTrackPricesOnTabs = SHARED_PREFERENCES_MANAGER.readBoolean(
                TRACK_PRICES_ON_TABS, isPriceTrackingEnabled());
        SHARED_PREFERENCES_MANAGER.writeBoolean(TRACK_PRICES_ON_TABS, !enableTrackPricesOnTabs);
    }

    /**
     * Update SharedPreferences when users turn on/off the feature tracking prices on tabs.
     */
    public static void setTrackPricesOnTabsEnabled(boolean enabled) {
        SHARED_PREFERENCES_MANAGER.writeBoolean(TRACK_PRICES_ON_TABS, enabled);
    }

    /**
     * @return Whether the track prices on tabs is turned on by users.
     */
    public static boolean isTrackPricesOnTabsEnabled() {
        return isPriceTrackingEligible()
                && SHARED_PREFERENCES_MANAGER.readBoolean(
                        TRACK_PRICES_ON_TABS, isPriceTrackingEnabled());
    }

    /**
     * Forbid showing the PriceWelcomeMessageCard any more.
     */
    public static void disablePriceWelcomeMessageCard() {
        SHARED_PREFERENCES_MANAGER.writeBoolean(PRICE_WELCOME_MESSAGE_CARD, false);
    }

    /**
     * @return Whether the PriceWelcomeMessageCard is enabled.
     */
    public static boolean isPriceWelcomeMessageCardEnabled() {
        return isPriceTrackingEligible()
                && SHARED_PREFERENCES_MANAGER.readBoolean(
                        PRICE_WELCOME_MESSAGE_CARD, isPriceTrackingEnabled());
    }

    /**
     * Increase the show count of PriceWelcomeMessageCard every time it shows in the tab switcher.
     */
    public static void increasePriceWelcomeMessageCardShowCount() {
        SHARED_PREFERENCES_MANAGER.writeInt(
                PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, getPriceWelcomeMessageCardShowCount() + 1);
    }

    /**
     * @return The show count of PriceWelcomeMessageCard.
     */
    public static int getPriceWelcomeMessageCardShowCount() {
        return SHARED_PREFERENCES_MANAGER.readInt(PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, 0);
    }

    /**
     * @return Whether the price drop notification is eligible to work.
     */
    public static boolean isPriceDropNotificationEligible() {
        return isPriceTrackingEligible() && getPriceTrackingNotificationsEnabled();
    }

    /**
     * Forbid showing the PriceAlertsMessageCard any more.
     */
    public static void disablePriceAlertsMessageCard() {
        SHARED_PREFERENCES_MANAGER.writeBoolean(PRICE_ALERTS_MESSAGE_CARD, false);
    }

    /**
     * @return Whether the PriceAlertsMessageCard is enabled. We don't show this message card if
     *         user can already receive price drop notifications, see {@link
     *         PriceDropNotificationManager#canPostNotification()}.
     */
    public static boolean isPriceAlertsMessageCardEnabled() {
        return isPriceDropNotificationEligible()
                && CommerceSubscriptionsServiceConfig.isImplicitSubscriptionsEnabled()
                && SHARED_PREFERENCES_MANAGER.readBoolean(
                        PRICE_ALERTS_MESSAGE_CARD, isPriceTrackingEnabled())
                && (!(new PriceDropNotificationManager()).canPostNotification());
    }

    /**
     * Increase the show count of PriceAlertsMessageCard every time it shows in the tab switcher.
     */
    public static void increasePriceAlertsMessageCardShowCount() {
        SHARED_PREFERENCES_MANAGER.writeInt(
                PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT, getPriceAlertsMessageCardShowCount() + 1);
    }

    /**
     * Decrease the show count of PriceAlertsMessageCard. Right now it is used to correct the show
     * count when PriceAlertsMessageCard is deprioritized by PriceWelcomeMessageCard.
     */
    public static void decreasePriceAlertsMessageCardShowCount() {
        SHARED_PREFERENCES_MANAGER.writeInt(
                PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT, getPriceAlertsMessageCardShowCount() - 1);
    }

    /**
     * @return The show count of PriceAlertsMessageCard.
     */
    public static int getPriceAlertsMessageCardShowCount() {
        return SHARED_PREFERENCES_MANAGER.readInt(PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT, 0);
    }

    /**
     * @return whether or not the user is in a state that allows them to use price tracking feature.
     *         Note: These checks can also be used in other commerce features.
     */
    public static boolean canFetchCommerceData() {
        return isSignedIn() && isOpenTabsSyncEnabled() && isAnonymizedUrlDataCollectionEnabled();
    }

    private static boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC);
    }

    private static boolean isOpenTabsSyncEnabled() {
        SyncService syncService = SyncService.get();
        return syncService != null && syncService.isSyncRequested()
                && syncService.getActiveDataTypes().contains(ModelType.SESSIONS);
    }

    private static boolean isAnonymizedUrlDataCollectionEnabled() {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    @VisibleForTesting
    public static void setIsSignedInAndSyncEnabledForTesting(Boolean isSignedInAndSyncEnabled) {
        sIsSignedInAndSyncEnabledForTesting = isSignedInAndSyncEnabled;
    }

    /**
     * @return if the {@link TabModel} is eligible for price tracking. Not all tab models are - for
     *         example incognito tabs are not eligible for price tracking.
     */
    public static boolean isTabModelPriceTrackingEligible(TabModel tabModel) {
        // Incognito Tabs are not eligible for price tracking.
        return !tabModel.getProfile().isOffTheRecord();
    }

    /**
     * @return how frequent we want to record metrics on whether user enables the price tracking
     *         annotations.
     */
    public static int getAnnotationsEnabledMetricsWindowDurationMilliSeconds() {
        int defaultDuration = (int) TimeUnit.DAYS.toMillis(1);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    PRICE_ANNOTATIONS_ENABLED_METRICS_WINDOW_DURATION_PARAM, defaultDuration);
        }
        return defaultDuration;
    }

    /**
     * @return whether we allow users to disable the price annotations feature.
     */
    public static boolean allowUsersToDisablePriceAnnotations() {
        if (FeatureList.isInitialized()) {
            return isPriceTrackingEligible()
                    && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                            ALLOW_DISABLE_PRICE_ANNOTATIONS_PARAM, true);
        }
        return isPriceTrackingEligible();
    }

    // TODO(crbug.com/1307949): Clean up price tracking menu.
    /**
     * @return whether we should show the PriceTrackingSettings menu item in grid tab switcher.
     */
    public static boolean shouldShowPriceTrackingMenu() {
        return false;
    }
}
