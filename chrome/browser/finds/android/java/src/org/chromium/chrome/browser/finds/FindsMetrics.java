// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics for Finds Notifications. */
@NullMarked
public class FindsMetrics {
    @VisibleForTesting
    public static final String OPT_IN_HISTOGRAM = "Notifications.ChromeFinds.OptInEvent";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.

    // ACCEPTED_RE_OPT_IN handles the case where the notification channel already exists and the
    // user previously opted in, but then manually disabled it in the settings, and is prompted
    // once more and accepts the opt-in. This is tracked differently in the metrics due to the
    // fact that the re-opt-in accept does NOT mean that the finds notifications are turned on
    // due to the manual action that the user must take in the settings page.

    // LINT.IfChange(ChromeFindsOptInEvent)
    @IntDef({
        FindsOptInEvent.SHOWN,
        FindsOptInEvent.ACCEPTED_FIRST_TIME,
        FindsOptInEvent.ACCEPTED_RE_OPT_IN,
        FindsOptInEvent.DECLINED,
        FindsOptInEvent.SNACKBAR_ACTION_CLICKED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface FindsOptInEvent {
        int SHOWN = 0;
        int ACCEPTED_FIRST_TIME = 1;
        int ACCEPTED_RE_OPT_IN = 2;
        int DECLINED = 3;
        int SNACKBAR_ACTION_CLICKED = 4;
        int NUM_ENTRIES = 5;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/notifications/enums.xml:ChromeFindsOptInEvent)

    /** Record that the opt-in bottom sheet was shown. */
    public static void recordOptInShown() {
        recordEvent(FindsOptInEvent.SHOWN);
    }

    /**
     * Record that the opt-in button was clicked.
     *
     * @param firstTime Whether this was the first time the user opted in.
     */
    public static void recordOptInAccepted(boolean firstTime) {
        recordEvent(
                firstTime
                        ? FindsOptInEvent.ACCEPTED_FIRST_TIME
                        : FindsOptInEvent.ACCEPTED_RE_OPT_IN);
    }

    /** Record that the opt-out button was clicked. */
    public static void recordOptOutClicked() {
        recordEvent(FindsOptInEvent.DECLINED);
    }

    /** Record that the opted in snackbar action button was clicked. */
    public static void recordSnackbarActionClicked() {
        recordEvent(FindsOptInEvent.SNACKBAR_ACTION_CLICKED);
    }

    private static void recordEvent(@FindsOptInEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                OPT_IN_HISTOGRAM, event, FindsOptInEvent.NUM_ENTRIES);
    }
}
