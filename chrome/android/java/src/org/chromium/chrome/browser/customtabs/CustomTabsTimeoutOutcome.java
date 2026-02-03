// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records a histogram that tracks the result of every CCT session where a timeout was configured.
 */
@NullMarked
public class CustomTabsTimeoutOutcome {
    public static final String HISTOGRAM_NAME = "CustomTabs.ResetTimeout.Outcome";

    // NOTE: This must be kept in sync with the definition |CustomTabsResetTimeoutOutcome|
    // in tools/metrics/histograms/metadata/custom_tabs/enums.xml.
    // LINT.IfChange(CustomTabsResetTimeoutOutcome)
    @IntDef({
        CustomTabsResetTimeoutOutcome.RETURNED_BEFORE_TIMEOUT,
        CustomTabsResetTimeoutOutcome.RESET_TRIGGERED_INTENT,
        CustomTabsResetTimeoutOutcome.RESET_TRIGGERED_FALLBACK,
        CustomTabsResetTimeoutOutcome.SESSION_CLOSED_MANUALLY,
        CustomTabsResetTimeoutOutcome.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CustomTabsResetTimeoutOutcome {
        int RETURNED_BEFORE_TIMEOUT = 0;
        int RESET_TRIGGERED_INTENT = 1;
        int RESET_TRIGGERED_FALLBACK = 2;
        int SESSION_CLOSED_MANUALLY = 3;
        int COUNT = 4;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/custom_tabs/enums.xml:CustomTabsResetTimeoutOutcome)

    /**
     * Records the outcome of a CCT session with a timeout.
     *
     * @param outcome The outcome to record.
     */
    public static void record(@CustomTabsResetTimeoutOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_NAME, outcome, CustomTabsResetTimeoutOutcome.COUNT);
    }
}
