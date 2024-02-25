// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;

/** A delegate class to record metrics associated with {@link QuickDeleteController}. */
public class QuickDeleteMetricsDelegate {
    public static final String HISTOGRAM_NAME = "Privacy.QuickDelete";

    /**
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     *
     * Must be kept in sync with the QuickDeleteAction in enums.xml.
     */
    @IntDef({
        QuickDeleteAction.MENU_ITEM_CLICKED,
        QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY,
        QuickDeleteAction.DELETE_CLICKED,
        QuickDeleteAction.CANCEL_CLICKED,
        QuickDeleteAction.TAB_SWITCHER_MENU_ITEM_CLICKED,
        QuickDeleteAction.MORE_OPTIONS_CLICKED,
        QuickDeleteAction.MY_ACTIVITY_LINK_CLICKED,
        QuickDeleteAction.SEARCH_HISTORY_LINK_CLICKED,
        QuickDeleteAction.LAST_15_MINUTES_SELECTED,
        QuickDeleteAction.LAST_HOUR_SELECTED,
        QuickDeleteAction.LAST_DAY_SELECTED,
        QuickDeleteAction.LAST_WEEK_SELECTED,
        QuickDeleteAction.FOUR_WEEKS_SELECTED,
        QuickDeleteAction.ALL_TIME_SELECTED
    })
    public @interface QuickDeleteAction {
        int MENU_ITEM_CLICKED = 0;
        int DELETE_CLICKED = 1;
        int CANCEL_CLICKED = 2;
        int DIALOG_DISMISSED_IMPLICITLY = 3;
        int TAB_SWITCHER_MENU_ITEM_CLICKED = 4;
        int MORE_OPTIONS_CLICKED = 5;
        int MY_ACTIVITY_LINK_CLICKED = 6;
        int SEARCH_HISTORY_LINK_CLICKED = 7;

        // Time period selections
        int LAST_15_MINUTES_SELECTED = 8;
        int LAST_HOUR_SELECTED = 9;
        int LAST_DAY_SELECTED = 10;
        int LAST_WEEK_SELECTED = 11;
        int FOUR_WEEKS_SELECTED = 12;
        int ALL_TIME_SELECTED = 13;

        // Always update MAX_VALUE to match the last item.
        int MAX_VALUE = ALL_TIME_SELECTED;
    }

    /**
     * A method to record the metrics of a {@link QuickDeleteAction}.
     *
     * @param quickDeleteAction action taken related to QuickDelete.
     */
    public static void recordHistogram(@QuickDeleteAction int quickDeleteAction) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_NAME, quickDeleteAction, QuickDeleteAction.MAX_VALUE);

        if (quickDeleteAction == QuickDeleteAction.DELETE_CLICKED) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Privacy.DeleteBrowsingData.Action",
                    DeleteBrowsingDataAction.QUICK_DELETE,
                    DeleteBrowsingDataAction.MAX_VALUE);
        }
    }
}
