// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_CONTROLS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_ICONS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_TEXT;

import android.os.Build;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.messages.MessageAutodismissDurationProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesMetrics;

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
        mAutodismissDurationMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE, "autodismiss_duration_ms",
                10 * (int) DateUtils.SECOND_IN_MILLIS);

        mAutodismissDurationWithA11yMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                "autodismiss_duration_with_a11y_ms", 30 * (int) DateUtils.SECOND_IN_MILLIS);
    }

    @Override
    public long get(@MessageIdentifier int messageIdentifier, long customDuration) {
        long nonA11yDuration = Math.max(mAutodismissDurationMs, customDuration);
        long finchControlledDuration = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                FEATURE_SPECIFIC_FINCH_CONTROLLED_DURATION_PREFIX
                        + MessagesMetrics.messageIdentifierToHistogramSuffix(messageIdentifier),
                -1);
        if (finchControlledDuration > 0) {
            nonA11yDuration = Math.max(finchControlledDuration, nonA11yDuration);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            // crbug.com/1312548: To have a minimum duration even if the system has a default value.
            return Math.max(mAutodismissDurationWithA11yMs,
                    ChromeAccessibilityUtil.get().getRecommendedTimeoutMillis((int) nonA11yDuration,
                            FLAG_CONTENT_ICONS | FLAG_CONTENT_CONTROLS | FLAG_CONTENT_TEXT));
        }
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                ? Math.max(mAutodismissDurationWithA11yMs, nonA11yDuration)
                : nonA11yDuration;
    }

    @VisibleForTesting
    public void setDefaultAutodismissDurationMsForTesting(long duration) {
        mAutodismissDurationMs = duration;
    }

    public void setDefaultAutodismissDurationWithA11yMsForTesting(long duration) {
        mAutodismissDurationWithA11yMs = duration;
    }
}
