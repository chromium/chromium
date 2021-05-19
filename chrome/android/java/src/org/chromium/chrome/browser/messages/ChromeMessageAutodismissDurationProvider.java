// Copyright 2021 The Chromium Authors. All rights reserved.
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

/**
 * Implementation of {@link MessageAutodismissDurationProvider}.
 */
public class ChromeMessageAutodismissDurationProvider
        implements MessageAutodismissDurationProvider {
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
    public long get(long customDuration) {
        long nonA11yDuration = Math.max(mAutodismissDurationMs, customDuration);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            return (long) ChromeAccessibilityUtil.get().getRecommendedTimeoutMillis(
                    (int) nonA11yDuration,
                    FLAG_CONTENT_ICONS | FLAG_CONTENT_CONTROLS | FLAG_CONTENT_TEXT);
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
