// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import android.text.format.DateUtils;

import org.chromium.components.messages.MessageAutodismissDurationProvider;
import org.chromium.components.messages.MessageIdentifier;
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
    private long mAutodismissDurationMs;
    private long mAutodismissDurationWithA11yMs;

    public ChromeMessageAutodismissDurationProvider() {
        mAutodismissDurationMs = 10 * (int) DateUtils.SECOND_IN_MILLIS;
        mAutodismissDurationWithA11yMs = 30 * (int) DateUtils.SECOND_IN_MILLIS;
    }

    @Override
    public long get(@MessageIdentifier int messageIdentifier, long customDuration) {
        long nonA11yDuration = Math.max(mAutodismissDurationMs, customDuration);
        long minDuration =
                AccessibilityState.isPerformGesturesEnabled()
                        ? mAutodismissDurationWithA11yMs
                        : nonA11yDuration;

        return AccessibilityState.getRecommendedTimeoutMillis(
                (int) minDuration, (int) nonA11yDuration);
    }

    public void setDefaultAutodismissDurationMsForTesting(long duration) {
        mAutodismissDurationMs = duration;
    }

    public void setDefaultAutodismissDurationWithA11yMsForTesting(long duration) {
        mAutodismissDurationWithA11yMs = duration;
    }
}
