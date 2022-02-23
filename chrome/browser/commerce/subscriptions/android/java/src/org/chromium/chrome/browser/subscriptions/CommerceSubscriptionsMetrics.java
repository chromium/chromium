// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;

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
}
