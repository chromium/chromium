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
        ClickInfo.LOCAL_SINGLE_FIRST,
        ClickInfo.HISTORY_SINGLE_FIRST,
        ClickInfo.FOREIGN_DOUBLE_ANY,
        ClickInfo.LOCAL_DOUBLE_ANY,
        ClickInfo.HISTORY_DOUBLE_ANY,
        ClickInfo.NUM_ENTRIES
    })
    @interface ClickInfo {
        int FOREIGN_SINGLE_FIRST = 0;
        // int FOREIGN_FOREIGN_DOUBLE_FIRST = 1;
        // int FOREIGN_FOREIGN_DOUBLE_SECOND = 2;
        int LOCAL_SINGLE_FIRST = 3;
        // int LOCAL_FOREIGN_DOUBLE_FIRST = 4;
        // int LOCAL_FOREIGN_DOUBLE_SECOND = 5;
        int HISTORY_SINGLE_FIRST = 6;
        int FOREIGN_DOUBLE_ANY = 7;
        int LOCAL_DOUBLE_ANY = 8;
        int HISTORY_DOUBLE_ANY = 9;
        int NUM_ENTRIES = 10;
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
        ModuleShowConfig.SINGLE_TILE_HISTORY,
        ModuleShowConfig.DOUBLE_TILE_FOREIGN_HISTORY,
        ModuleShowConfig.DOUBLE_TILE_LOCAL_LOCAL,
        ModuleShowConfig.DOUBLE_TILE_HISTORY_HISTORY,
        ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY,
        ModuleShowConfig.SINGLE_TILE_ANY,
        ModuleShowConfig.DOUBLE_TILE_ANY,
        ModuleShowConfig.NUM_ENTRIES
    })
    @interface ModuleShowConfig {
        int SINGLE_TILE_FOREIGN = 0;
        int DOUBLE_TILE_FOREIGN_FOREIGN = 1;
        int SINGLE_TILE_LOCAL = 2;
        int DOUBLE_TILE_LOCAL_FOREIGN = 3;
        int DOUBLE_TILE_FOREIGN_HISTORY = 4;
        int DOUBLE_TILE_LOCAL_LOCAL = 5;
        int DOUBLE_TILE_LOCAL_HISTORY = 6;
        int SINGLE_TILE_HISTORY = 7;
        int DOUBLE_TILE_HISTORY_HISTORY = 8;
        int SINGLE_TILE_ANY = 9;
        int DOUBLE_TILE_ANY = 10;
        int NUM_ENTRIES = 11;
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
    static @ClickInfo int computeClickInfo(SuggestionEntry entry, int size) {
        boolean isSingle = size == 1;
        if (isSingle) {
            if (entry.isLocalTab()) return ClickInfo.LOCAL_SINGLE_FIRST;
            return entry.type == SuggestionEntryType.FOREIGN_TAB
                    ? ClickInfo.FOREIGN_SINGLE_FIRST
                    : ClickInfo.HISTORY_SINGLE_FIRST;
        }

        if (entry.isLocalTab()) {
            return isSingle ? ClickInfo.LOCAL_SINGLE_FIRST : ClickInfo.LOCAL_DOUBLE_ANY;
        } else if (entry.type == SuggestionEntryType.FOREIGN_TAB) {
            return isSingle ? ClickInfo.FOREIGN_SINGLE_FIRST : ClickInfo.FOREIGN_DOUBLE_ANY;
        } else {
            return isSingle ? ClickInfo.HISTORY_SINGLE_FIRST : ClickInfo.HISTORY_DOUBLE_ANY;
        }
    }

    /** Maps SuggestionBundle to a ModuleShowConfig value, or null if there are no suggestions. */
    static @Nullable @ModuleShowConfig Integer computeModuleShowConfig(
            @Nullable SuggestionBundle bundle) {
        if (bundle == null || bundle.entries.size() == 0) return null;

        boolean isSingle = bundle.entries.size() == 1;
        SuggestionEntry entry = bundle.entries.get(0);
        if (isSingle) {
            if (entry.isLocalTab()) {
                return ModuleShowConfig.SINGLE_TILE_LOCAL;
            } else if (entry.getNeedMatchLocalTab()) {
                return ModuleShowConfig.SINGLE_TILE_ANY;
            } else {
                return entry.type == SuggestionEntryType.FOREIGN_TAB
                        ? ModuleShowConfig.SINGLE_TILE_FOREIGN
                        : ModuleShowConfig.SINGLE_TILE_HISTORY;
            }
        }

        SuggestionEntry entry1 = bundle.entries.get(1);
        if (entry.getNeedMatchLocalTab() || entry1.getNeedMatchLocalTab()) {
            return ModuleShowConfig.DOUBLE_TILE_ANY;
        }

        if (entry.isLocalTab()) {
            if (entry1.isLocalTab()) {
                return ModuleShowConfig.DOUBLE_TILE_LOCAL_LOCAL;
            } else {
                return entry1.type == SuggestionEntryType.FOREIGN_TAB
                        ? ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN
                        : ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY;
            }
        } else if (entry.type == SuggestionEntryType.FOREIGN_TAB) {
            if (entry1.isLocalTab()) {
                return ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN;
            }
            return entry1.type == SuggestionEntryType.FOREIGN_TAB
                    ? ModuleShowConfig.DOUBLE_TILE_FOREIGN_FOREIGN
                    : ModuleShowConfig.DOUBLE_TILE_FOREIGN_HISTORY;
        } else {
            if (entry1.isLocalTab()) {
                return ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY;
            }
            return entry1.type == SuggestionEntryType.FOREIGN_TAB
                    ? ModuleShowConfig.DOUBLE_TILE_FOREIGN_HISTORY
                    : ModuleShowConfig.DOUBLE_TILE_HISTORY_HISTORY;
        }
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
