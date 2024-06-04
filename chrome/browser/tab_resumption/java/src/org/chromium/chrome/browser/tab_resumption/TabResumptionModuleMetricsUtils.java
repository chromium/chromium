// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;

/** Utilities for recording tab resumption module metrics. */
public class TabResumptionModuleMetricsUtils {

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // Information on the tile clicked by the user. The values must be consistent with
    // MagicStack.Clank.TabResumption.ClickInfo in enums.xml.
    @IntDef({
        ClickInfo.FOREIGN_SINGLE_FIRST,
        ClickInfo.FOREIGN_FOREIGN_DOUBLE_FIRST,
        ClickInfo.FOREIGN_FOREIGN_DOUBLE_SECOND,
        ClickInfo.LOCAL_SINGLE_FIRST,
        ClickInfo.LOCAL_FOREIGN_DOUBLE_FIRST,
        ClickInfo.LOCAL_FOREIGN_DOUBLE_SECOND,
        ClickInfo.NUM_ENTRIES
    })
    @interface ClickInfo {
        int FOREIGN_SINGLE_FIRST = 0;
        int FOREIGN_FOREIGN_DOUBLE_FIRST = 1;
        int FOREIGN_FOREIGN_DOUBLE_SECOND = 2;
        int LOCAL_SINGLE_FIRST = 3;
        int LOCAL_FOREIGN_DOUBLE_FIRST = 4;
        int LOCAL_FOREIGN_DOUBLE_SECOND = 5;
        int NUM_ENTRIES = 6;
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
        ModuleShowConfig.DOUBLE_TILE_FOREIGN_FOREIGN,
        ModuleShowConfig.SINGLE_TILE_LOCAL,
        ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN,
        ModuleShowConfig.NUM_ENTRIES
    })
    @interface ModuleShowConfig {
        int SINGLE_TILE_FOREIGN = 0;
        int DOUBLE_TILE_FOREIGN_FOREIGN = 1;
        int SINGLE_TILE_LOCAL = 2;
        int DOUBLE_TILE_LOCAL_FOREIGN = 3;
        int NUM_ENTRIES = 4;
    }

    static final String HISTOGRAM_CLICK_INFO = "MagicStack.Clank.TabResumption.ClickInfo";
    static final String HISTOGRAM_MODULE_NOT_SHOWN_REASON =
            "MagicStack.Clank.TabResumption.ModuleNotShownReason";
    static final String HISTOGRAM_MODULE_SHOW_CONFIG =
            "MagicStack.Clank.TabResumption.ModuleShowConfig";
    static final String HISTOGRAM_STABILITY_DELAY = "MagicStack.Clank.TabResumption.StabilityDelay";

    static final String HISTOGRAM_IS_SALIENT_IMAGE_AVAILABLE =
            "MagicStack.Clank.TabResumption.IsSalientImageAvailable";

    static final String HISTOGRAM_SEE_MORE_LINK_CLICKED =
            "MagicStack.Clank.TabResumption.SeeMoreLinkClicked";

    static final String HISTOGRAM_TAB_RECENCY_SHOW =
            "MagicStack.Clank.TabResumption.TabRecency.Show";

    static final String HISTOGRAM_TAB_RECENCY_CLICK =
            "MagicStack.Clank.TabResumption.TabRecency.Click";

    /** Maps specification of a clicked tile to a ClickInfo for logging. */
    static @ClickInfo int computeClickInfo(@ModuleShowConfig int moduleShowConfig, int tileIndex) {
        switch (moduleShowConfig) {
            case ModuleShowConfig.SINGLE_TILE_FOREIGN:
                assert tileIndex == 0;
                return ClickInfo.FOREIGN_SINGLE_FIRST;

            case ModuleShowConfig.DOUBLE_TILE_FOREIGN_FOREIGN:
                assert tileIndex == 0 || tileIndex == 1;
                return tileIndex == 0
                        ? ClickInfo.FOREIGN_FOREIGN_DOUBLE_FIRST
                        : ClickInfo.FOREIGN_FOREIGN_DOUBLE_SECOND;

            case ModuleShowConfig.SINGLE_TILE_LOCAL:
                assert tileIndex == 0;
                return ClickInfo.LOCAL_SINGLE_FIRST;

            case ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN:
                assert tileIndex == 0 || tileIndex == 1;
                return tileIndex == 0
                        ? ClickInfo.LOCAL_FOREIGN_DOUBLE_FIRST
                        : ClickInfo.LOCAL_FOREIGN_DOUBLE_SECOND;
        }

        assert false;
        return ClickInfo.NUM_ENTRIES;
    }

    /** Maps SuggestionBundle to a ModuleShowConfig value, or null if there are no suggestions. */
    static @Nullable @ModuleShowConfig Integer computeModuleShowConfig(
            @Nullable SuggestionBundle bundle) {
        if (bundle == null || bundle.entries.size() == 0) return null;

        if (bundle.entries.size() == 1) {
            return bundle.entries.get(0).isLocalTab()
                    ? ModuleShowConfig.SINGLE_TILE_LOCAL
                    : ModuleShowConfig.SINGLE_TILE_FOREIGN;
        }

        // If Local Tab suggestion exists, it's always at index 0.
        return bundle.entries.get(0).isLocalTab()
                ? ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN
                : ModuleShowConfig.DOUBLE_TILE_FOREIGN_FOREIGN;
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

    /** Records whether a salient image fetch attempt was successful. */
    static void recordSalientImageAvailability(boolean isAvailable) {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_IS_SALIENT_IMAGE_AVAILABLE, isAvailable);
    }

    /**
     * Records the configuration of the tab resumption module when the "see more" link is clicked.
     */
    static void recordSeeMoreLinkClicked(@ModuleShowConfig int config) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_SEE_MORE_LINK_CLICKED, config, ModuleShowConfig.NUM_ENTRIES);
    }

    /**
     * Records the recency of a suggestion tile, i.e., the duration between the tile's tab's last
     * active time to when the tile gets shown.
     */
    static void recordTabRecencyShow(long recencyMs) {
        RecordHistogram.recordCustomTimesHistogram(
                HISTOGRAM_TAB_RECENCY_SHOW, recencyMs, 1, DateUtils.DAY_IN_MILLIS * 2, 50);
    }

    /**
     * Records the recency of a suggested tile on click, i.e., the duratoin of the tile's tab's last
     * active time to when the tile gets shown (NOT when click takes place).
     */
    static void recordTabRecencyClick(long recencyMs) {
        RecordHistogram.recordCustomTimesHistogram(
                HISTOGRAM_TAB_RECENCY_CLICK, recencyMs, 1, DateUtils.DAY_IN_MILLIS * 2, 50);
    }
}
