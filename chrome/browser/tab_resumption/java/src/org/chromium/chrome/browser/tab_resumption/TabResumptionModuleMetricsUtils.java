// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/** Utilities for recording tab resumption module metrics. */
public class TabResumptionModuleMetricsUtils {

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // Information on the tile clicked by the user. The values must be consistent with
    // MagicStack.Clank.TabResumption.ClickInfo in enums.xml.
    @IntDef({
        ClickInfo.SINGLE_TILE_FIRST,
        ClickInfo.DOUBLE_TILE_FIRST,
        ClickInfo.DOUBLE_TILE_SECOND,
        ClickInfo.NUM_ENTRIES
    })
    @interface ClickInfo {
        int SINGLE_TILE_FIRST = 0;
        int DOUBLE_TILE_FIRST = 1;
        int DOUBLE_TILE_SECOND = 2;
        int NUM_ENTRIES = 3;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // Reasons for why the tab resumption module is not shown. The values must be consistent with
    // MagicStack.Clank.TabResumption.ModuleNotShownReason in enums.xml.
    @IntDef({
        ModuleNotShownReason.NO_SUGGESTIONS,
        ModuleNotShownReason.FEATURE_DISABLED,
        ModuleNotShownReason.NOT_SIGNED_IN,
        ModuleNotShownReason.NOT_SYNC,
        ModuleNotShownReason.NUM_ENTRIES
    })
    @interface ModuleNotShownReason {
        int NO_SUGGESTIONS = 0;
        int FEATURE_DISABLED = 1;
        int NOT_SIGNED_IN = 2;
        int NOT_SYNC = 3;
        int NUM_ENTRIES = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // Configuration of the tab resumption module when it's shown and suggestions become stable.
    // The values must be consistent with MagicStack.Clank.TabResumption.ModuleShowConfig in
    // enums.xml.
    @IntDef({
        ModuleShowConfig.SINGLE_TILE_FOREIGN,
        ModuleShowConfig.DOUBLE_TILE_FOREIGN,
        ModuleShowConfig.NUM_ENTRIES
    })
    @interface ModuleShowConfig {
        int SINGLE_TILE_FOREIGN = 0;
        int DOUBLE_TILE_FOREIGN = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * Decision on whether or not to show the tab resumption module, attached with reason if the
     * decision is to not show.
     */
    public static class ModuleVisibility {
        public final boolean value;

        // Useful only if `value` is false.
        public final @ModuleNotShownReason int notShownReason;

        ModuleVisibility(boolean value, @ModuleNotShownReason int notShownReason) {
            this.value = value;
            this.notShownReason = notShownReason;
        }
    }

    static final String HISTOGRAM_CLICK_INFO = "MagicStack.Clank.TabResumption.ClickInfo";
    static final String HISTOGRAM_MODULE_NOT_SHOWN_REASON =
            "MagicStack.Clank.TabResumption.ModuleNotShownReason";
    static final String HISTOGRAM_MODULE_SHOW_CONFIG =
            "MagicStack.Clank.TabResumption.ModuleShowConfig";
    static final String HISTOGRAM_STABILITY_DELAY = "MagicStack.Clank.TabResumption.StabilityDelay";

    /** Maps specification of a clicked tile to a ClickInfo for logging. */
    static @ClickInfo int computeClickInfo(int tileCount, int tileIndex) {
        assert tileIndex >= 0 && tileIndex < tileCount;
        if (tileCount == 1) {
            return ClickInfo.SINGLE_TILE_FIRST;
        }

        assert tileCount == 2;
        return tileIndex == 0 ? ClickInfo.DOUBLE_TILE_FIRST : ClickInfo.DOUBLE_TILE_SECOND;
    }

    /** Records info (encoded tile count and index) on a clicked tile. */
    static void recordClickInfo(@ClickInfo int clickInfo) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_CLICK_INFO, clickInfo, ClickInfo.NUM_ENTRIES);
    }

    /** Records the reason on why the tab resumption module is not shown. */
    static void recordModuleNotShownReason(@ModuleNotShownReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_MODULE_NOT_SHOWN_REASON, reason, ModuleNotShownReason.NUM_ENTRIES);
    }

    /** Records the configuration of the tab resumption module if it's shown. */
    static void recordModuleShowConfig(@ModuleShowConfig int config) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_MODULE_SHOW_CONFIG, config, ModuleShowConfig.NUM_ENTRIES);
    }

    /**
     * Records how long it takes to show STABLE suggestions via "slow path", including network
     * delay, computation time, and rendering.
     */
    static void recordStabilityDelay(long stabilityDelay) {
        // The recorded values are typically small (< 1 second). TabResumptionModuleMediator also
        // imposes stability after a timeout of STABILITY_TIMEOUT_MS, after which delay logging is
        // moot. This timeout is well below the max bucket of 10 seconds.
        RecordHistogram.recordTimesHistogram(HISTOGRAM_STABILITY_DELAY, stabilityDelay);
    }
}
