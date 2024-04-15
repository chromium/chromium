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

    static final String SHARE_SHEET_VISIBILITY_HISTOGRAM =
            "Android.PageSummary.Share.IsVisibleInShareSheet";

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
