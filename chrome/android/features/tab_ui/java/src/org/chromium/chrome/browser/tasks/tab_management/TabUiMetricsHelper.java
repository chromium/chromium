// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorShareAction.TabSelectionEditorShareActionState;

/**
 * Metrics helper class for the Tab UI module.
 */
public class TabUiMetricsHelper {
    /**
     * The last time the Tab Selection Editor was shown across all instances, null if never shown
     * before within an activity lifespan.
     */
    private static Long sLastShownTimestampMillis;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({TabSelectionEditorActionMetricGroups.BOOKMARK,
            TabSelectionEditorActionMetricGroups.CLOSE, TabSelectionEditorActionMetricGroups.GROUP,
            TabSelectionEditorActionMetricGroups.SELECT_ALL,
            TabSelectionEditorActionMetricGroups.DESELECT_ALL,
            TabSelectionEditorActionMetricGroups.SHARE_TAB,
            TabSelectionEditorActionMetricGroups.SHARE_TABS,
            TabSelectionEditorActionMetricGroups.UNGROUP,
            TabSelectionEditorActionMetricGroups.PROVIDER_GROUP,
            TabSelectionEditorActionMetricGroups.PROVIDER_UNGROUP,
            TabSelectionEditorActionMetricGroups.UNSELECTED,
            TabSelectionEditorActionMetricGroups.SELECTED})
    public @interface TabSelectionEditorActionMetricGroups {
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
    @IntDef({TabSelectionEditorExitMetricGroups.CLOSED,
            TabSelectionEditorExitMetricGroups.CLOSED_AUTOMATICALLY,
            TabSelectionEditorExitMetricGroups.CLOSED_BY_USER})
    public @interface TabSelectionEditorExitMetricGroups {
        int CLOSED = 0;
        int CLOSED_AUTOMATICALLY = 1;
        int CLOSED_BY_USER = 2;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({TabSelectionEditorOpenMetricGroups.OPEN_FROM_GRID,
            TabSelectionEditorOpenMetricGroups.OPEN_FROM_DIALOG})
    public @interface TabSelectionEditorOpenMetricGroups {
        int OPEN_FROM_GRID = 0;
        int OPEN_FROM_DIALOG = 1;
    }

    // Histograms
    public static void recordEditorTimeSinceLastShownHistogram() {
        long timestampMillis = System.currentTimeMillis();
        if (sLastShownTimestampMillis != null) {
            RecordHistogram.recordTimesHistogram("Android.TabMultiSelectV2.TimeSinceLastShown",
                    timestampMillis - sLastShownTimestampMillis);
        }
        sLastShownTimestampMillis = timestampMillis;
    }

    public static void recordShareStateHistogram(@TabSelectionEditorShareActionState int action) {
        RecordHistogram.recordEnumeratedHistogram("Android.TabMultiSelectV2.SharingState", action,
                TabSelectionEditorShareActionState.NUM_ENTRIES);
    }

    // Metrics
    public static void recordSelectionEditorActionMetrics(
            @TabSelectionEditorActionMetricGroups int actionId) {
        switch (actionId) {
            case TabSelectionEditorActionMetricGroups.BOOKMARK:
                RecordUserAction.record("TabMultiSelectV2.BookmarkTabs");
                break;
            case TabSelectionEditorActionMetricGroups.CLOSE:
                RecordUserAction.record("TabMultiSelectV2.CloseTabs");
                break;
            case TabSelectionEditorActionMetricGroups.GROUP:
                RecordUserAction.record("TabMultiSelectV2.GroupTabs");
                RecordUserAction.record("TabGroup.Created.TabMultiSelect");
                break;
            case TabSelectionEditorActionMetricGroups.SELECT_ALL:
                RecordUserAction.record("TabMultiSelectV2.SelectAll");
                break;
            case TabSelectionEditorActionMetricGroups.DESELECT_ALL:
                RecordUserAction.record("TabMultiSelectV2.DeselectAll");
                break;
            case TabSelectionEditorActionMetricGroups.SHARE_TAB:
                RecordUserAction.record("TabMultiSelectV2.SharedTabAsTextList");
                break;
            case TabSelectionEditorActionMetricGroups.SHARE_TABS:
                RecordUserAction.record("TabMultiSelectV2.SharedTabsListAsTextList");
                break;
            case TabSelectionEditorActionMetricGroups.UNGROUP:
                RecordUserAction.record("TabMultiSelectV2.UngroupTabs");
                RecordUserAction.record("TabGridDialog.RemoveFromGroup.TabMultiSelect");
                break;
            case TabSelectionEditorActionMetricGroups.PROVIDER_GROUP:
                RecordUserAction.record("TabMultiSelect.Done");
                RecordUserAction.record("TabGroup.Created.TabMultiSelect");
                break;
            case TabSelectionEditorActionMetricGroups.PROVIDER_UNGROUP:
                RecordUserAction.record("TabGridDialog.RemoveFromGroup.TabMultiSelect");
                break;
            case TabSelectionEditorActionMetricGroups.UNSELECTED:
                RecordUserAction.record("TabMultiSelect.TabUnselected");
                break;
            case TabSelectionEditorActionMetricGroups.SELECTED:
                RecordUserAction.record("TabMultiSelect.TabSelected");
                break;
            default:
                assert false : "Unexpected TabSelectionEditorActionMetricGroups value " + actionId
                               + " when calling recordSelectionEditorActionMetrics.";
        }
    }

    public static void recordSelectionEditorExitMetrics(
            @TabSelectionEditorExitMetricGroups int actionId, Context context) {
        if (TabUiFeatureUtilities.isTabSelectionEditorV2Enabled(context)) {
            switch (actionId) {
                case TabSelectionEditorExitMetricGroups.CLOSED:
                    RecordUserAction.record("TabMultiSelectV2.Closed");
                    break;
                case TabSelectionEditorExitMetricGroups.CLOSED_AUTOMATICALLY:
                    RecordUserAction.record("TabMultiSelectV2.ClosedAutomatically");
                    break;
                case TabSelectionEditorExitMetricGroups.CLOSED_BY_USER:
                    RecordUserAction.record("TabMultiSelectV2.ClosedByUser");
                    break;
                default:
                    assert false
                        : "Unexpected TabSelectionEditorExitMetricGroups value of "
                          + actionId
                          + " when calling recordSelectionEditorExitMetrics with V2 enabled.";
            }
        } else {
            switch (actionId) {
                case TabSelectionEditorExitMetricGroups.CLOSED:
                    // Since the equivalent metric is not recorded for V1, it will result in a
                    // no-op.
                    break;
                case TabSelectionEditorExitMetricGroups.CLOSED_AUTOMATICALLY:
                    // Since the equivalent metric is not recorded for V1, it will result in a
                    // no-op.
                    break;
                case TabSelectionEditorExitMetricGroups.CLOSED_BY_USER:
                    RecordUserAction.record("TabMultiSelect.Cancelled");
                    break;
                default:
                    assert false
                        : "Unexpected TabSelectionEditorExitMetricGroups value of "
                          + actionId
                          + " when calling recordSelectionEditorExitMetrics with V2 disabled.";
            }
        }
    }

    public static void recordSelectionEditorOpenMetrics(
            @TabSelectionEditorOpenMetricGroups int actionId, Context context) {
        if (TabUiFeatureUtilities.isTabSelectionEditorV2Enabled(context)) {
            switch (actionId) {
                case TabSelectionEditorOpenMetricGroups.OPEN_FROM_GRID:
                    RecordUserAction.record("TabMultiSelectV2.OpenFromGrid");
                    break;
                case TabSelectionEditorOpenMetricGroups.OPEN_FROM_DIALOG:
                    RecordUserAction.record("TabMultiSelectV2.OpenFromDialog");
                    break;
                default:
                    assert false
                        : "Unexpected TabSelectionEditorOpenMetricGroups value of "
                          + actionId
                          + " when calling recordSelectionEditorOpenMetrics with V2 enabled.";
            }
        } else {
            switch (actionId) {
                case TabSelectionEditorOpenMetricGroups.OPEN_FROM_DIALOG:
                    RecordUserAction.record("TabMultiSelect.OpenFromDialog");
                    break;
                default:
                    assert false
                        : "Unexpected TabSelectionEditorOpenMetricGroups value of "
                          + actionId
                          + " when calling recordSelectionEditorOpenMetrics with V2 disabled.";
            }
        }
    }
}