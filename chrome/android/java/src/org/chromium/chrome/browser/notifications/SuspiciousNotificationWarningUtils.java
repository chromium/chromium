// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper utils for replacing suspicious notifications with warnings. */
public class SuspiciousNotificationWarningUtils {
    @VisibleForTesting
    public static final String SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME =
            "SafeBrowsing.SuspiciousNotificationWarning.Interactions";

    /**
     * All user interactions that can happen when a warning is shown for a suspicious notification.
     * Must be kept in sync with SafetyCheckUnusedSitePermissionsModuleInteractions in
     * safe_browsing/enums.xml.
     */
    @IntDef({
        SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
        SuspiciousNotificationWarningInteractions.SHOW_ORIGINAL_NOTIFICATION,
        SuspiciousNotificationWarningInteractions.UNSUBSCRIBE,
        SuspiciousNotificationWarningInteractions.ALWAYS_ALLOW,
        SuspiciousNotificationWarningInteractions.DISMISS,
        SuspiciousNotificationWarningInteractions.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuspiciousNotificationWarningInteractions {
        int WARNING_SHOWN = 0;
        int SHOW_ORIGINAL_NOTIFICATION = 1;
        int UNSUBSCRIBE = 2;
        int ALWAYS_ALLOW = 3;
        int DISMISS = 4;
        int MAX_VALUE = DISMISS;
    }

    static void recordSuspiciousNotificationWarningInteractions(
            @SuspiciousNotificationWarningInteractions int value) {
        RecordHistogram.recordEnumeratedHistogram(
                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                value,
                SuspiciousNotificationWarningInteractions.MAX_VALUE);
    }
}
