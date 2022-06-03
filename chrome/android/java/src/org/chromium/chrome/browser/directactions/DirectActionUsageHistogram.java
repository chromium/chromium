// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * A histogram that tracks calls to {@link ChromeActivity#onPerformDirectAction} and, when possible,
 * the specific action that was performed.
 */
class DirectActionUsageHistogram {

    /** A map that convert known string ids to enum value for the histogram. */
    private static final Map<String, Integer> ACTION_ID_MAP;
    static {
        Map<String, Integer> map = new HashMap<>();
        map.put(ChromeDirectActionIds.GO_BACK, DirectActionId.GO_BACK);
        map.put(ChromeDirectActionIds.RELOAD, DirectActionId.RELOAD);
        map.put(ChromeDirectActionIds.GO_FORWARD, DirectActionId.GO_FORWARD);
        map.put(ChromeDirectActionIds.BOOKMARK_THIS_PAGE, DirectActionId.BOOKMARK_THIS_PAGE);
        map.put(ChromeDirectActionIds.DOWNLOADS, DirectActionId.DOWNLOADS);
        map.put(ChromeDirectActionIds.PREFERENCES, DirectActionId.PREFERENCES);
        map.put(ChromeDirectActionIds.OPEN_HISTORY, DirectActionId.OPEN_HISTORY);
        map.put(ChromeDirectActionIds.HELP, DirectActionId.HELP);
        map.put(ChromeDirectActionIds.NEW_TAB, DirectActionId.NEW_TAB);
        map.put(ChromeDirectActionIds.CLOSE_TAB, DirectActionId.CLOSE_TAB);
        map.put(ChromeDirectActionIds.CLOSE_ALL_TABS, DirectActionId.CLOSE_ALL_TABS);
        map.put(ChromeDirectActionIds.FIND_IN_PAGE, DirectActionId.FIND_IN_PAGE);
        ACTION_ID_MAP = Collections.unmodifiableMap(map);
    }

    /**
     * Enum defined in tools/metrics/histograms/enums.xml.
     */
    @VisibleForTesting
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({DirectActionId.GO_BACK, DirectActionId.RELOAD, DirectActionId.GO_FORWARD,
            DirectActionId.BOOKMARK_THIS_PAGE, DirectActionId.DOWNLOADS, DirectActionId.PREFERENCES,
            DirectActionId.OPEN_HISTORY, DirectActionId.HELP, DirectActionId.NEW_TAB,
            DirectActionId.CLOSE_TAB, DirectActionId.CLOSE_ALL_TABS, DirectActionId.FIND_IN_PAGE,
            DirectActionId.NUM_ENTRIES})
    @interface DirectActionId {
        /** No action was executed. */
        int UNKNOWN = 0;

        /** An action was executed that isn't in this enum. */
        int OTHER = 1;

        // Actions from ChromeDirectActionIds.
        int GO_BACK = 2;
        int RELOAD = 3;
        int GO_FORWARD = 4;
        int BOOKMARK_THIS_PAGE = 5;
        int DOWNLOADS = 6;
        int PREFERENCES = 7;
        int OPEN_HISTORY = 8;
        int HELP = 9;
        int NEW_TAB = 10;
        int CLOSE_TAB = 11;
        int CLOSE_ALL_TABS = 12;
        int FIND_IN_PAGE = 13;

        int NUM_ENTRIES = 14;
    }

    /**
     * Records an attempt to execute a direct action that was rejected as unknown.
     */
    static void recordUnknown() {
        record(DirectActionId.UNKNOWN);
    }

    /**
     * Records direct action usage.
     *
     * @param actionId The string id of the direct action that was executed.
     */
    static void record(String actionId) {
        @DirectActionId
        Integer histogramId = ACTION_ID_MAP.get(actionId);
        if (histogramId == null) histogramId = DirectActionId.OTHER;

        record(histogramId);
    }

    private static void record(@DirectActionId int actionId) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DirectAction.Perform", actionId, DirectActionId.NUM_ENTRIES);
    }
}
