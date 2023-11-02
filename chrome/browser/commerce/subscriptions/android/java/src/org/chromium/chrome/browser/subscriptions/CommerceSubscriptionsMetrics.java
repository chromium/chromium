// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Commerce Subscriptions Metrics.
 */
public class CommerceSubscriptionsMetrics {
    @VisibleForTesting
    public static final String SUBSCRIPTION_CHROME_MANAGED_COUNT_HISTOGRAM =
            "Commerce.Subscriptions.ChromeManaged.Count";
    @VisibleForTesting
    public static final String SUBSCRIPTION_USER_MANAGED_COUNT_HISTOGRAM =
            "Commerce.Subscriptions.UserManaged.Count";
    @VisibleForTesting
    public static final String ACCOUNT_WAA_STATUS_HISTOGRAM = "Commerce.SignIn.AccountWaaStatus";

    /**
     * The account web and app activity enabled status.
     *
     * Needs to stay in sync with AccountWaaStatusForCommerce in enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({AccountWaaStatus.SIGN_OUT, AccountWaaStatus.SIGN_IN_WAA_DISABLED,
            AccountWaaStatus.SIGN_IN_WAA_ENABLED, AccountWaaStatus.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AccountWaaStatus {
        int SIGN_OUT = 0;
        int SIGN_IN_WAA_DISABLED = 1;
        int SIGN_IN_WAA_ENABLED = 2;

        // Must be the last one.
        int NUM_ENTRIES = 3;
    }

    /**
     * Record the number of subscriptions per management type.
     */
    void recordSubscriptionCounts(List<CommerceSubscription> subscriptions) {
        int chromeManaged = 0;
        int userManaged = 0;
        for (CommerceSubscription subscription : subscriptions) {
            @SubscriptionManagementType
            String type = subscription.getManagementType();
            if (SubscriptionManagementType.CHROME_MANAGED.equals(type)) {
                chromeManaged++;
            } else if (SubscriptionManagementType.USER_MANAGED.equals(type)) {
                userManaged++;
            }
        }
        RecordHistogram.recordCount1000Histogram(
                SUBSCRIPTION_CHROME_MANAGED_COUNT_HISTOGRAM, chromeManaged);
        RecordHistogram.recordCount1000Histogram(
                SUBSCRIPTION_USER_MANAGED_COUNT_HISTOGRAM, userManaged);
    }

    void recordAccountWaaStatus() {
        RecordHistogram.recordEnumeratedHistogram(
                ACCOUNT_WAA_STATUS_HISTOGRAM, getAccountWaaStatus(), AccountWaaStatus.NUM_ENTRIES);
    }

    @AccountWaaStatus
    private int getAccountWaaStatus() {
        if (!isSignedIn()) {
            return AccountWaaStatus.SIGN_OUT;
        } else if (isWebAndAppActivityEnabled()) {
            return AccountWaaStatus.SIGN_IN_WAA_ENABLED;
        } else {
            return AccountWaaStatus.SIGN_IN_WAA_DISABLED;
        }
    }

    private boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC);
    }

    private boolean isWebAndAppActivityEnabled() {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        return prefService != null
                && prefService.getBoolean(Pref.WEB_AND_APP_ACTIVITY_ENABLED_FOR_SHOPPING);
    }
}
