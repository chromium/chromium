// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/** Flag configuration for Price Tracking Notification experience. */
public class PriceTrackingNotificationConfig {
    private static final String NOTIFICATION_TIMEOUT_PARAM = "notification_timeout_ms";

    // Gets the timeout of the price drop notification.
    public static int getNotificationTimeoutMs() {
        int defaultTimeout = (int) TimeUnit.HOURS.toMillis(3);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, NOTIFICATION_TIMEOUT_PARAM,
                    defaultTimeout);
        }
        return defaultTimeout;
    }
}