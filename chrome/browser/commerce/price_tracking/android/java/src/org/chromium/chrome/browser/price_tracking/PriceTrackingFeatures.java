// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.signin.identitymanager.ConsentLevel;

import java.util.concurrent.TimeUnit;

/** Flag configuration for price tracking features. */
public class PriceTrackingFeatures {
    @VisibleForTesting
    public static final String PRICE_TRACKING_PARAM = "enable_price_tracking";
    @VisibleForTesting
    public static final String PRICE_NOTIFICATION_PARAM = "enable_price_notification";
    @VisibleForTesting
    public static final String ALLOW_DISABLE_PRICE_ANNOTATIONS_PARAM =
            "allow_disable_price_annotations";
    @VisibleForTesting
    public static final String PRICE_DROP_IPH_ENABLED_PARAM = "enable_price_drop_iph";
    private static final String PRICE_DROP_BADGE_ENABLED_PARAM = "enable_price_drop_badge";
    private static final String PRICE_ANNOTATIONS_ENABLED_METRICS_WINDOW_DURATION_PARAM =
            "price_annotations_enabled_metrics_window_duration_ms";

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

    /**
     * @return Whether the price drop notification is eligible to work.
     */
    public static boolean isPriceDropNotificationEligible() {
        return isPriceTrackingEligible() && getPriceTrackingNotificationsEnabled();
    }

    private static boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC);
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

    public static boolean isPriceDropIphEnabled() {
        if (FeatureList.isInitialized()) {
            return isPriceTrackingEligible()
                    && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.COMMERCE_PRICE_TRACKING, PRICE_DROP_IPH_ENABLED_PARAM,
                            false);
        }
        return isPriceTrackingEligible();
    }

    public static boolean isPriceDropBadgeEnabled() {
        if (FeatureList.isInitialized()) {
            return isPriceTrackingEligible()
                    && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                            PRICE_DROP_BADGE_ENABLED_PARAM, false);
        }
        return isPriceTrackingEligible();
    }
}