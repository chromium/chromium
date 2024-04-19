// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/** Helper class for metrics related to page summary. */
public class PageSummaryMetrics {

    // These values must match the PageSummaryShareSheetVisibility enum in enums.xml.
    // Only add new values at the end, right before COUNT.
    @IntDef({
        ShareActionVisibility.SHOWN,
        ShareActionVisibility.NOT_SHOWN_MODEL_NOT_AVAILABLE,
        ShareActionVisibility.NOT_SHOWN_ALREADY_RUNNING,
        ShareActionVisibility.NOT_SHOWN_TAB_NOT_VALID,
        ShareActionVisibility.NOT_SHOWN_URL_NOT_VALID,
        ShareActionVisibility.NOT_SHOWN_PROFILE_NOT_SUPPORTED,
        ShareActionVisibility.NOT_SHOWN_TAB_NOT_SUPPORTED,
        ShareActionVisibility.COUNT,
    })
    @interface ShareActionVisibility {
        int SHOWN = 0;
        int NOT_SHOWN_MODEL_NOT_AVAILABLE = 1;
        int NOT_SHOWN_ALREADY_RUNNING = 2;
        int NOT_SHOWN_TAB_NOT_VALID = 3;
        int NOT_SHOWN_URL_NOT_VALID = 4;
        int NOT_SHOWN_PROFILE_NOT_SUPPORTED = 5;
        int NOT_SHOWN_TAB_NOT_SUPPORTED = 6;
        int COUNT = 7;
    }

    // These values must match the PageSummarySheetEvents enum in enums.xml.
    // Only add new values at the end, right before COUNT.
    @IntDef({
        PageSummarySheetEvents.OPEN_SUMMARY_SHEET,
        PageSummarySheetEvents.CLOSE_SHEET_WHILE_INITIALIZING,
        PageSummarySheetEvents.CLOSE_SHEET_WHILE_LOADING,
        PageSummarySheetEvents.CLOSE_SHEET_ON_ERROR,
        PageSummarySheetEvents.CLOSE_SHEET_AFTER_SUCCESS,
        PageSummarySheetEvents.ADD_SUMMARY,
        PageSummarySheetEvents.REMOVE_SUMMARY,
        PageSummarySheetEvents.CLICK_POSITIVE_FEEDBACK,
        PageSummarySheetEvents.CLICK_NEGATIVE_FEEDBACK,
        PageSummarySheetEvents.NEGATIVE_FEEDBACK_TYPE_SELECTED,
        PageSummarySheetEvents.NEGATIVE_FEEDBACK_SHEET_DISMISSED,
        PageSummarySheetEvents.CLICK_LEARN_MORE,
        PageSummarySheetEvents.COUNT,
    })
    @interface PageSummarySheetEvents {
        int OPEN_SUMMARY_SHEET = 0;
        int CLOSE_SHEET_WHILE_INITIALIZING = 1;
        int CLOSE_SHEET_WHILE_LOADING = 2;
        int CLOSE_SHEET_ON_ERROR = 3;
        int CLOSE_SHEET_AFTER_SUCCESS = 4;
        int ADD_SUMMARY = 5;
        int REMOVE_SUMMARY = 6;
        int CLICK_POSITIVE_FEEDBACK = 7;
        int CLICK_NEGATIVE_FEEDBACK = 8;
        int NEGATIVE_FEEDBACK_TYPE_SELECTED = 9;
        int NEGATIVE_FEEDBACK_SHEET_DISMISSED = 10;
        int CLICK_LEARN_MORE = 11;
        int COUNT = 12;
    }

    static final String SHARE_SHEET_VISIBILITY_HISTOGRAM =
            "Android.PageSummary.Share.IsVisibleInShareSheet";
    static final String SUMMARY_SHEET_UI_EVENTS = "Sharing.AndroidPageSummary.SheetEvents";

    /** Records UI events related to the page summary sharing flow. */
    public static void recordSummarySheetEvent(@PageSummarySheetEvents int event) {
        RecordHistogram.recordEnumeratedHistogram(
                SUMMARY_SHEET_UI_EVENTS, event, PageSummarySheetEvents.COUNT);
    }

    /**
     * Records whether the page summary option is shown on a share sheet, and if not the reason why.
     */
    public static void recordShareSheetVisibility(@ShareActionVisibility int shareSheetVisibility) {
        RecordHistogram.recordEnumeratedHistogram(
                SHARE_SHEET_VISIBILITY_HISTOGRAM,
                shareSheetVisibility,
                ShareActionVisibility.COUNT);
    }
}
