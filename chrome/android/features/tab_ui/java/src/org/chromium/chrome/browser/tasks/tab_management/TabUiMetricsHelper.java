// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorShareAction.TabListEditorShareActionState;

/** Metrics helper class for the Tab UI module. */
public class TabUiMetricsHelper {
    /**
     * The last time the Tab Selection Editor was shown across all instances, null if never shown
     * before within an activity lifespan.
     */
    private static Long sLastShownTimestampMillis;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        TabListEditorActionMetricGroups.BOOKMARK,
        TabListEditorActionMetricGroups.CLOSE,
        TabListEditorActionMetricGroups.GROUP,
        TabListEditorActionMetricGroups.SELECT_ALL,
        TabListEditorActionMetricGroups.DESELECT_ALL,
        TabListEditorActionMetricGroups.SHARE_TAB,
        TabListEditorActionMetricGroups.SHARE_TABS,
        TabListEditorActionMetricGroups.UNGROUP,
        TabListEditorActionMetricGroups.PROVIDER_GROUP,
        TabListEditorActionMetricGroups.PROVIDER_UNGROUP,
        TabListEditorActionMetricGroups.UNSELECTED,
        TabListEditorActionMetricGroups.SELECTED
    })
    public @interface TabListEditorActionMetricGroups {
        int BOOKMARK = 0;
        int CLOSE = 1;
        int GROUP = 2;
        int SELECT_ALL = 3;
        int DESELECT_ALL = 4;
        int SHARE_TAB = 5;
        int SHARE_TABS = 6;
        int UNGROUP = 7;
        int PROVIDER_GROUP = 8;
        int PROVIDER_UNGROUP = 9;
        int UNSELECTED = 10;
        int SELECTED = 11;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        TabListEditorExitMetricGroups.CLOSED,
        TabListEditorExitMetricGroups.CLOSED_AUTOMATICALLY,
        TabListEditorExitMetricGroups.CLOSED_BY_USER
    })
    public @interface TabListEditorExitMetricGroups {
        int CLOSED = 0;
        int CLOSED_AUTOMATICALLY = 1;
        int CLOSED_BY_USER = 2;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        TabListEditorOpenMetricGroups.OPEN_FROM_GRID,
        TabListEditorOpenMetricGroups.OPEN_FROM_DIALOG
    })
    public @interface TabListEditorOpenMetricGroups {
        int OPEN_FROM_GRID = 0;
        int OPEN_FROM_DIALOG = 1;
    }

    // Histograms
    public static void recordEditorTimeSinceLastShownHistogram() {
        long timestampMillis = System.currentTimeMillis();
        if (sLastShownTimestampMillis != null) {
            RecordHistogram.recordTimesHistogram(
                    "Android.TabMultiSelectV2.TimeSinceLastShown",
                    timestampMillis - sLastShownTimestampMillis);
        }
        sLastShownTimestampMillis = timestampMillis;
    }

    public static void recordShareStateHistogram(@TabListEditorShareActionState int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.TabMultiSelectV2.SharingState",
                action,
                TabListEditorShareActionState.NUM_ENTRIES);
    }

    // Metrics
    public static void recordSelectionEditorActionMetrics(
            @TabListEditorActionMetricGroups int actionId) {
        switch (actionId) {
            case TabListEditorActionMetricGroups.BOOKMARK:
                RecordUserAction.record("TabMultiSelectV2.BookmarkTabs");
                break;
            case TabListEditorActionMetricGroups.CLOSE:
                RecordUserAction.record("TabMultiSelectV2.CloseTabs");
                break;
            case TabListEditorActionMetricGroups.GROUP:
                RecordUserAction.record("TabMultiSelectV2.GroupTabs");
                RecordUserAction.record("TabGroup.Created.TabMultiSelect");
                break;
            case TabListEditorActionMetricGroups.SELECT_ALL:
                RecordUserAction.record("TabMultiSelectV2.SelectAll");
                break;
            case TabListEditorActionMetricGroups.DESELECT_ALL:
                RecordUserAction.record("TabMultiSelectV2.DeselectAll");
                break;
            case TabListEditorActionMetricGroups.SHARE_TAB:
                RecordUserAction.record("TabMultiSelectV2.SharedTabAsTextList");
                break;
            case TabListEditorActionMetricGroups.SHARE_TABS:
                RecordUserAction.record("TabMultiSelectV2.SharedTabsListAsTextList");
                break;
            case TabListEditorActionMetricGroups.UNGROUP:
                RecordUserAction.record("TabMultiSelectV2.UngroupTabs");
                RecordUserAction.record("TabGridDialog.RemoveFromGroup.TabMultiSelect");
                break;
            case TabListEditorActionMetricGroups.PROVIDER_GROUP:
                RecordUserAction.record("TabMultiSelect.Done");
                RecordUserAction.record("TabGroup.Created.TabMultiSelect");
                break;
            case TabListEditorActionMetricGroups.PROVIDER_UNGROUP:
                RecordUserAction.record("TabGridDialog.RemoveFromGroup.TabMultiSelect");
                break;
            case TabListEditorActionMetricGroups.UNSELECTED:
                RecordUserAction.record("TabMultiSelect.TabUnselected");
                break;
            case TabListEditorActionMetricGroups.SELECTED:
                RecordUserAction.record("TabMultiSelect.TabSelected");
                break;
            default:
                assert false
                        : "Unexpected TabListEditorActionMetricGroups value "
                                + actionId
                                + " when calling recordSelectionEditorActionMetrics.";
        }
    }

    public static void recordSelectionEditorExitMetrics(
            @TabListEditorExitMetricGroups int actionId, Context context) {
        switch (actionId) {
            case TabListEditorExitMetricGroups.CLOSED:
                RecordUserAction.record("TabMultiSelectV2.Closed");
                break;
            case TabListEditorExitMetricGroups.CLOSED_AUTOMATICALLY:
                RecordUserAction.record("TabMultiSelectV2.ClosedAutomatically");
                break;
            case TabListEditorExitMetricGroups.CLOSED_BY_USER:
                RecordUserAction.record("TabMultiSelectV2.ClosedByUser");
                break;
            default:
                assert false
                        : "Unexpected TabListEditorExitMetricGroups value of "
                                + actionId
                                + " when calling recordSelectionEditorExitMetrics with V2 enabled.";
        }
    }

    public static void recordSelectionEditorOpenMetrics(
            @TabListEditorOpenMetricGroups int actionId, Context context) {
        switch (actionId) {
            case TabListEditorOpenMetricGroups.OPEN_FROM_GRID:
                RecordUserAction.record("TabMultiSelectV2.OpenFromGrid");
                break;
            case TabListEditorOpenMetricGroups.OPEN_FROM_DIALOG:
                RecordUserAction.record("TabMultiSelectV2.OpenFromDialog");
                break;
            default:
                assert false
                        : "Unexpected TabListEditorOpenMetricGroups value of "
                                + actionId
                                + " when calling recordSelectionEditorOpenMetrics with V2 enabled.";
        }
    }
}
