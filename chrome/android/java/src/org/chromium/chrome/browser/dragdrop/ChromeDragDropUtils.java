// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.text.format.DateUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Utility class for Chrome drag and drop implementations. */
public class ChromeDragDropUtils {
    private static final int MAX_TAB_TEARING_FAILURE_COUNT_PER_DAY = 10;

    /**
     * Records linear histogram Android.DragDrop.Tab.MaxInstanceFailureCount and saves related
     * SharedPreferences values.
     */
    public static void recordTabDragToCreateInstanceFailureCount() {
        var prefs = ChromeSharedPreferences.getInstance();
        // Check the failure count in a day for every unhandled dragged tab drop when max instances
        // are open.
        long timestamp =
                prefs.readLong(
                        ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS, 0);
        int failureCount =
                prefs.readInt(ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_COUNT, 0);
        long current = System.currentTimeMillis();

        boolean isNewDay = timestamp == 0 || current - timestamp > DateUtils.DAY_IN_MILLIS;
        if (isNewDay) {
            prefs.writeLong(
                    ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS, current);
            // Reset the count to 0 if it is the start of the next 24-hour period.
            failureCount = 0;
        }

        RecordHistogram.recordExactLinearHistogram(
                "Android.DragDrop.Tab.MaxInstanceFailureCount",
                failureCount + 1,
                MAX_TAB_TEARING_FAILURE_COUNT_PER_DAY + 1);
        prefs.writeInt(
                ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_COUNT, failureCount + 1);
    }
}
