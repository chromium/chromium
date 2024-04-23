// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Class manage recording different metrics for share sheet. */
public final class ShareMetricsUtils {
    /**
     * The type of share custom actions, in sync with ShareCustomAction in enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        ShareCustomAction.INVALID,
        ShareCustomAction.COPY,
        ShareCustomAction.COPY_IMAGE,
        ShareCustomAction.COPY_TEXT,
        ShareCustomAction.COPY_URL,
        ShareCustomAction.LONG_SCREENSHOT,
        ShareCustomAction.PRINT,
        ShareCustomAction.QR_CODE,
        ShareCustomAction.SEND_TAB_TO_SELF,
        ShareCustomAction.COPY_HIGHLIGHT_WITHOUT_LINK,
        ShareCustomAction.COPY_IMAGE_WITH_LINK,
        ShareCustomAction.PAGE_INFO,
        ShareCustomAction.REMOVE_PAGE_INFO,
        ShareCustomAction.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShareCustomAction {
        // TODO(crbug.com/336372387): Can be removed if Chrome's share sheet is removed.
        int INVALID = -1; // Not recorded for metrics.

        int COPY = 0;
        int COPY_IMAGE = 1;
        int COPY_TEXT = 2;
        int COPY_URL = 3;
        int LONG_SCREENSHOT = 4;
        int PRINT = 5;
        int QR_CODE = 6;
        int SEND_TAB_TO_SELF = 7;
        int COPY_HIGHLIGHT_WITHOUT_LINK = 8;
        int COPY_IMAGE_WITH_LINK = 9;
        int PAGE_INFO = 10;
        int REMOVE_PAGE_INFO = 11;

        // Add new types here

        int NUM_ENTRIES = 12;
    }

    /**
     * Record the share feature usage for a certain user action type and the time it takes for user
     * to reach such action.
     *
     * @param actionType The type of custom action.
     * @param shareStartTime The time user started the share.
     */
    public static void recordShareUserAction(
            @ShareCustomAction int actionType, long shareStartTime) {
        RecordHistogram.recordEnumeratedHistogram(
                "Sharing.SharingHubAndroid.CustomAction",
                actionType,
                ShareCustomAction.NUM_ENTRIES);
        RecordHistogram.recordMediumTimesHistogram(
                "Sharing.SharingHubAndroid.TimeToCustomAction",
                System.currentTimeMillis() - shareStartTime);
    }
}
