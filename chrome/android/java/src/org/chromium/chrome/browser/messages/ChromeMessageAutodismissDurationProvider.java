// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.messages.MessageAutodismissDurationProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesMetrics;
import org.chromium.ui.accessibility.AccessibilityState;

/**
 * Implementation of {@link MessageAutodismissDurationProvider}.
 *
 * Use finch parameter "autodismiss_duration_ms_{@link MessageIdentifier}" to customize through
 * finch config, such as "autodismiss_duration_ms_SyncError" within the feature {@code
 * ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE}. The duration configured in this way will
 * take the highest priority over clients' configuration in code.
 */
public class ChromeMessageAutodismissDurationProvider
        implements MessageAutodismissDurationProvider {
    @VisibleForTesting
    static final String FEATURE_SPECIFIC_FINCH_CONTROLLED_DURATION_PREFIX =
            "autodismiss_duration_ms_";

    private long mAutodismissDurationMs;
    private long mAutodismissDurationWithA11yMs;

    public ChromeMessageAutodismissDurationProvider() {
        mAutodismissDurationMs =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                        "autodismiss_duration_ms",
                        10 * (int) DateUtils.SECOND_IN_MILLIS);

        mAutodismissDurationWithA11yMs =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                        "autodismiss_duration_with_a11y_ms",
                        30 * (int) DateUtils.SECOND_IN_MILLIS);
    }

    @Override
    public long get(@MessageIdentifier int messageIdentifier, long customDuration) {
        long nonA11yDuration = Math.max(mAutodismissDurationMs, customDuration);
        long finchControlledDuration =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                        FEATURE_SPECIFIC_FINCH_CONTROLLED_DURATION_PREFIX
                                + MessagesMetrics.messageIdentifierToHistogramSuffix(
                                        messageIdentifier),
                        -1);
        if (finchControlledDuration > 0) {
            nonA11yDuration = Math.max(finchControlledDuration, nonA11yDuration);
        }

        // If no a11y service that can perform gestures is enabled, use the set duration. Otherwise
        // multiply the duration by the recommended multiplier and use that with a minimum of 30s.
        return !AccessibilityState.isPerformGesturesEnabled()
                ? nonA11yDuration
                : (long)
                        AccessibilityState.getRecommendedTimeoutMillis(
                                (int) mAutodismissDurationWithA11yMs, (int) nonA11yDuration);
    }

    public void setDefaultAutodismissDurationMsForTesting(long duration) {
        mAutodismissDurationMs = duration;
    }

    public void setDefaultAutodismissDurationWithA11yMsForTesting(long duration) {
        mAutodismissDurationWithA11yMs = duration;
    }
}
