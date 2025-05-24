// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/**
 * Utility for determining whether the user has disabled all of Chrome's notifications using the
 * system's per-application settings.
 *
 * <p>Enabling developers to show notifications with their own content creates a significant product
 * risk: one spammy notification too many and the user might disable notifications for all of
 * Chrome, which is obviously very bad. While we have a strong focus on providing clear attribution
 * and ways of revoking notifications for a particular website, measuring this is still important.
 */
@NullMarked
public class NotificationSystemStatusUtil {
    // Status codes returned by {@link #getAppNotificationStatus}.
    static final int APP_NOTIFICATIONS_STATUS_UNDETERMINABLE = 0;
    static final int APP_NOTIFICATIONS_STATUS_ENABLED = 2;
    static final int APP_NOTIFICATIONS_STATUS_DISABLED = 3;

    /**
     * Must be set to the maximum value of the above values, plus one.
     *
     * <p>If this value changes, kAppNotificationStatusBoundary in android_metrics_provider.cc must
     * also be updated.
     */
    private static final int APP_NOTIFICATIONS_STATUS_BOUNDARY = 4;

    /**
     * Determines whether notifications are enabled for the app represented by |context| and updates
     * the histogram "Notifications.AppNotiicationStatus". Notifications may be disabled because
     * either the user, or a management tool, has explicitly disallowed the Chrome App to display
     * notifications.
     */
    static void recordAppNotificationStatusHistogram() {
        RecordHistogram.recordEnumeratedHistogram(
                "Notifications.AppNotificationStatus",
                getAppNotificationStatus(),
                APP_NOTIFICATIONS_STATUS_BOUNDARY);
    }

    @CalledByNative
    @VisibleForTesting
    static int getAppNotificationStatus() {
        return NotificationProxyUtils.areNotificationsEnabled()
                ? APP_NOTIFICATIONS_STATUS_ENABLED
                : APP_NOTIFICATIONS_STATUS_DISABLED;
    }
}
