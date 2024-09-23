// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

/** Metrics helper class for the restore tabs feature. */
public class RestoreTabsMetricsHelper {
    private static int sPromoShownCount;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        RestoreTabsOnFREPromoShowResult.SHOWN,
        RestoreTabsOnFREPromoShowResult.NOT_ELIGIBLE,
        RestoreTabsOnFREPromoShowResult.NO_SYNCED_TABS,
        RestoreTabsOnFREPromoShowResult.NULL_PROFILE,
        RestoreTabsOnFREPromoShowResult.TAB_SYNC_DISABLED,
        RestoreTabsOnFREPromoShowResult.NUM_ENTRIES
    })
    public @interface RestoreTabsOnFREPromoShowResult {
        int SHOWN = 0;
        int NOT_ELIGIBLE = 1;
        int NO_SYNCED_TABS = 2;
        int NULL_PROFILE = 3;
        int TAB_SYNC_DISABLED = 4;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 5;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        RestoreTabsOnFREResultAction.ACCEPTED,
        RestoreTabsOnFREResultAction.DISMISSED_SCRIM,
        RestoreTabsOnFREResultAction.DISMISSED_BACKPRESS,
        RestoreTabsOnFREResultAction.DISMISSED_SWIPE,
        RestoreTabsOnFREResultAction.NUM_ENTRIES
    })
    public @interface RestoreTabsOnFREResultAction {
        int ACCEPTED = 0;
        int DISMISSED_SCRIM = 1;
        int DISMISSED_BACKPRESS = 2;
        int DISMISSED_SWIPE = 3;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        RestoreTabsOnFREBackPressType.SYSTEM_BACKPRESS,
        RestoreTabsOnFREBackPressType.BACK_BUTTON
    })
    public @interface RestoreTabsOnFREBackPressType {
        int SYSTEM_BACKPRESS = 0;
        int BACK_BUTTON = 1;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        RestoreTabsOnFRERestoredTabsResult.ALL,
        RestoreTabsOnFRERestoredTabsResult.SUBSET,
        RestoreTabsOnFRERestoredTabsResult.NONE,
        RestoreTabsOnFRERestoredTabsResult.NUM_ENTRIES
    })
    public @interface RestoreTabsOnFRERestoredTabsResult {
        int ALL = 0;
        int SUBSET = 1;
        int NONE = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        RestoreTabsOnFREDeviceRestoredFrom.DEFAULT,
        RestoreTabsOnFREDeviceRestoredFrom.NON_DEFAULT,
        RestoreTabsOnFREDeviceRestoredFrom.SINGLE_DEVICE,
        RestoreTabsOnFREDeviceRestoredFrom.NUM_ENTRIES
    })
    public @interface RestoreTabsOnFREDeviceRestoredFrom {
        int DEFAULT = 0;
        int NON_DEFAULT = 1;
        int SINGLE_DEVICE = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    // Histograms
    public static void recordPromoShowResultHistogram(@RestoreTabsOnFREPromoShowResult int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.RestoreTabsOnFRE.PromoShowResult2",
                action,
                RestoreTabsOnFREPromoShowResult.NUM_ENTRIES);
    }

    public static void recordResultActionHistogram(@RestoreTabsOnFREResultAction int action) {
        int count = RestoreTabsMetricsHelper.getPromoShownCount();
        assert count == 1 || count == 2;
        switch (count) {
            case 1:
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.RestoreTabsOnFRE.ResultActionFirstShow2",
                        action,
                        RestoreTabsOnFREResultAction.NUM_ENTRIES);
                break;
            case 2:
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.RestoreTabsOnFRE.ResultActionSecondShow2",
                        action,
                        RestoreTabsOnFREResultAction.NUM_ENTRIES);
                break;
        }
    }

    public static void recordSyncedDevicesCountHistogram(int count) {
        RecordHistogram.recordCount100Histogram(
                "Android.RestoreTabsOnFRE.SyncedDevicesCount", count);
    }

    public static void recordDeviceRestoredFromHistogram(
            @RestoreTabsOnFREDeviceRestoredFrom int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.RestoreTabsOnFRE.DeviceRestoredFrom",
                action,
                RestoreTabsOnFREDeviceRestoredFrom.NUM_ENTRIES);
    }

    public static void recordEligibleTabsForRestoreCountHistogram(int count) {
        RecordHistogram.recordCount100000Histogram(
                "Android.RestoreTabsOnFRE.EligibleTabsForRestoreCount", count);
    }

    public static void recordTabsRestoredCountHistogram(int count) {
        RecordHistogram.recordCount100000Histogram(
                "Android.RestoreTabsOnFRE.TabsRestoredCount", count);
    }

    public static void recordTabsRestoredPercentageHistogram(int percent) {
        RecordHistogram.recordPercentageHistogram(
                "Android.RestoreTabsOnFRE.TabsRestoredPercentage", percent);
    }

    public static void recordRestoredTabsResultHistogram(
            @RestoreTabsOnFRERestoredTabsResult int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.RestoreTabsOnFRE.RestoredTabsResult",
                action,
                RestoreTabsOnFRERestoredTabsResult.NUM_ENTRIES);
    }

    // Metrics
    public static void recordResultActionMetrics(@RestoreTabsOnFREResultAction int actionId) {
        switch (actionId) {
            case RestoreTabsOnFREResultAction.ACCEPTED:
                RecordUserAction.record("RestoreTabsOnFRE.PromoResultAccepted");
                break;
            case RestoreTabsOnFREResultAction.DISMISSED_SCRIM:
                RecordUserAction.record("RestoreTabsOnFRE.PromoResultDismissedByScrim");
                break;
            case RestoreTabsOnFREResultAction.DISMISSED_BACKPRESS:
                RecordUserAction.record("RestoreTabsOnFRE.PromoResultDismissedByBackpress");
                break;
            case RestoreTabsOnFREResultAction.DISMISSED_SWIPE:
                RecordUserAction.record("RestoreTabsOnFRE.PromoResultDismissedBySwipe");
                break;
            default:
                assert false
                        : "Unexpected RestoreTabsOnFREResultAction value of "
                                + actionId
                                + " when calling recordResultActionMetrics.";
        }
    }

    public static void recordBackPressTypeMetrics(@RestoreTabsOnFREBackPressType int actionId) {
        switch (actionId) {
            case RestoreTabsOnFREBackPressType.SYSTEM_BACKPRESS:
                RecordUserAction.record("RestoreTabsOnFRE.BackPressTypeSystemBackPress");
                break;
            case RestoreTabsOnFREBackPressType.BACK_BUTTON:
                RecordUserAction.record("RestoreTabsOnFRE.BackPressTypeBackButton");
                break;
            default:
                assert false
                        : "Unexpected RestoreTabsOnFREBackPressType value of "
                                + actionId
                                + " when calling recordBackPressTypeMetrics.";
        }
    }

    public static void recordNonDefaultDeviceSelectionMetrics() {
        RecordUserAction.record("RestoreTabsOnFRE.SelectedNonDefaultDevice");
    }

    public static void recordDeviceSelectionScreenMetrics() {
        RecordUserAction.record("RestoreTabsOnFRE.DeviceSelectionScreen");
    }

    public static void recordReviewTabsScreenMetrics() {
        RecordUserAction.record("RestoreTabsOnFRE.ReviewTabsScreen");
    }

    public static void recordRestoredViaPromoScreenMetrics() {
        RecordUserAction.record("RestoreTabsOnFRE.RestoredViaPromoScreen");
    }

    public static void recordRestoredViaReviewTabsScreenMetrics() {
        RecordUserAction.record("RestoreTabsOnFRE.RestoredViaReviewTabsScreen");
    }

    public static void setPromoShownCount(int count) {
        sPromoShownCount = count;
    }

    public static int getPromoShownCount() {
        return sPromoShownCount;
    }
}
