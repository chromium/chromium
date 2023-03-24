// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataAction;

/**
 * A delegate class to record metrics associated with {@link QuickDeleteController}.
 */
public class QuickDeleteMetricsDelegate {
    /**
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     *
     * Must be kept in sync with the QuickDeleteAction in enums.xml.
     */
    @IntDef({QuickDeleteAction.MENU_ITEM_CLICKED, QuickDeleteAction.DELETE_CLICKED,
            QuickDeleteAction.CANCEL_CLICKED, QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY})
    public @interface QuickDeleteAction {
        int MENU_ITEM_CLICKED = 0;
        int DELETE_CLICKED = 1;
        int CANCEL_CLICKED = 2;
        int DIALOG_DISMISSED_IMPLICITLY = 3;
        // Always update MAX_VALUE to match the last item.
        int MAX_VALUE = DIALOG_DISMISSED_IMPLICITLY;
    }

    /**
     * A method to record the metrics of a {@link QuickDeleteAction}.
     *
     * @param quickDeleteAction action taken related to QuickDelete.
     */
    public static void recordHistogram(@QuickDeleteAction int quickDeleteAction) {
        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.QuickDelete", quickDeleteAction, QuickDeleteAction.MAX_VALUE);

        if (quickDeleteAction == QuickDeleteAction.DELETE_CLICKED) {
            RecordHistogram.recordEnumeratedHistogram("Privacy.ClearBrowsingData.Action",
                    ClearBrowsingDataAction.QUICK_DELETE_LAST15_MINUTES,
                    ClearBrowsingDataAction.MAX_VALUE);
        }
    }
}
