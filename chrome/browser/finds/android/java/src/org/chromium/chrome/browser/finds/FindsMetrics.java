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

    // LINT.IfChange(ChromeFindsOptInEvent)
    @IntDef({
        FindsOptInEvent.SHOWN,
        FindsOptInEvent.ACCEPTED_FIRST_TIME,
        FindsOptInEvent.DECLINED,
        FindsOptInEvent.SNACKBAR_ACTION_CLICKED,
        FindsOptInEvent.DISMISSED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface FindsOptInEvent {
        int SHOWN = 0;
        int ACCEPTED_FIRST_TIME = 1;
        @Deprecated int ACCEPTED_RE_OPT_IN = 2;
        int DECLINED = 3;
        int SNACKBAR_ACTION_CLICKED = 4;
        int DISMISSED = 5;
        int NUM_ENTRIES = 6;
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
        // Only log if it is the first time (since we will never re-prompt
        // unless it is a testing configuration feature param to always
        // show opt-in bottom sheet).
        if (firstTime) {
            recordEvent(FindsOptInEvent.ACCEPTED_FIRST_TIME);
        }
    }

    /** Record that the opt-out button was clicked. */
    public static void recordOptOutClicked() {
        recordEvent(FindsOptInEvent.DECLINED);
    }

    /** Record that the opted in snackbar action button was clicked. */
    public static void recordSnackbarActionClicked() {
        recordEvent(FindsOptInEvent.SNACKBAR_ACTION_CLICKED);
    }

    /** Record that the opt-in bottom sheet was dismissed. */
    public static void recordOptInDismissed() {
        recordEvent(FindsOptInEvent.DISMISSED);
    }

    private static void recordEvent(@FindsOptInEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                OPT_IN_HISTOGRAM, event, FindsOptInEvent.NUM_ENTRIES);
    }
}
