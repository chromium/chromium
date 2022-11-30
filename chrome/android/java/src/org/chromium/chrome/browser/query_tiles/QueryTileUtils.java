// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.components.segmentation_platform.SegmentSelectionResult;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles various feature utility functions for query tiles.
 */
@JNINamespace("query_tiles")
public class QueryTileUtils {
    private static Boolean sShowQueryTilesOnNTP;
    private static Boolean sShowQueryTilesOnStartSurface;
    private static final String BEHAVIOURAL_TARGETING_KEY = "behavioural_targeting";
    private static final String NUM_DAYS_KEEP_SHOWING_QUERY_TILES_KEY =
            "num_days_keep_showing_query_tiles";
    private static final String NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD_KEY =
            "num_days_mv_clicks_below_threshold";
    private static final String MV_TILE_CLICKS_THRESHOLD_KEY = "mv_tile_click_threshold";
    private static final int DEFAULT_MV_TILE_CLICKS_THRESHOLD = 1;
    private static final long INVALID_DECISION_TIMESTAMP = -1L;
    private static final String QUERY_TILES_SEGMENTATION_PLATFORM_KEY = "query_tiles";
    private static int sSegmentationResultsForTesting = -1;

    @VisibleForTesting
    static final long MILLISECONDS_PER_DAY = TimeUtils.SECONDS_PER_DAY * 1000;
    @VisibleForTesting
    static final int DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES = 28;
    @VisibleForTesting
    static final int DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD = 7;

    // Constants with ShowQueryTilesSegmentationResult in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({ShowQueryTilesSegmentationResult.UNINITIALIZED,
            ShowQueryTilesSegmentationResult.DONT_SHOW, ShowQueryTilesSegmentationResult.SHOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShowQueryTilesSegmentationResult {
        int UNINITIALIZED = 0;
        int DONT_SHOW = 1;
        int SHOW = 2;
        int NUM_ENTRIES = 3;
    }

    // Constants with ShowQueryTilesSegmentationResultComparison in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({ShowQueryTilesSegmentationResultComparison.UNINITIALIZED,
            ShowQueryTilesSegmentationResultComparison.SEGMENTATION_ENABLED_LOGIC_ENABLED,
            ShowQueryTilesSegmentationResultComparison.SEGMENTATION_ENABLED_LOGIC_DISABLED,
            ShowQueryTilesSegmentationResultComparison.SEGMENTATION_DISABLED_LOGIC_ENABLED,
            ShowQueryTilesSegmentationResultComparison.SEGMENTATION_DISABLED_LOGIC_DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface ShowQueryTilesSegmentationResultComparison {
        int UNINITIALIZED = 0;
        int SEGMENTATION_ENABLED_LOGIC_ENABLED = 1;
        int SEGMENTATION_ENABLED_LOGIC_DISABLED = 2;
        int SEGMENTATION_DISABLED_LOGIC_ENABLED = 3;
        int SEGMENTATION_DISABLED_LOGIC_DISABLED = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Whether query tiles is enabled and should be shown on NTP.
     * @return Whether the query tile feature is enabled on NTP.
     */
    public static boolean isQueryTilesEnabledOnNTP() {
        // Cache the result so it will not change during the same browser session.
        if (sShowQueryTilesOnNTP != null) return sShowQueryTilesOnNTP;
        boolean queryTileEnabled =
                (ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES)
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_IN_NTP))
                || QueryTileUtilsJni.get().isQueryTilesEnabled();
        sShowQueryTilesOnNTP = queryTileEnabled && shouldShowQueryTiles();
        return sShowQueryTilesOnNTP;
    }

    /**
     * Whether query tiles is enabled and should be shown on start surface.
     * @return Whether the query tile feature is enabled on start surface.
     */
    public static boolean isQueryTilesEnabledOnStartSurface() {
        // Cache the result so it will not change during the same browser session.
        if (sShowQueryTilesOnStartSurface != null) return sShowQueryTilesOnStartSurface;
        boolean queryTileEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES)
                || QueryTileUtilsJni.get().isQueryTilesEnabled();
        sShowQueryTilesOnStartSurface = queryTileEnabled && shouldShowQueryTiles();
        return sShowQueryTilesOnStartSurface;
    }

    /**
     * Called to Check whether query tiles should be displayed. Here are the rules for showing query
     * tile: If user hasn't clicked on MV tiles for a while, query tiles will be shown for a period
     * of time. And the decision is reevaluated when it expires.
     * @return Whether query tiles should be displayed.
     */
    @VisibleForTesting
    static boolean shouldShowQueryTiles() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_SEGMENTATION)) {
            return true;
        }

        // Check if segmentation model should be used.
        // When behavioural targeting key is "model", the segmentation model result will be used.
        // When behavioural targeting key is "model_comparison", the segmentation model result will
        // be recorded for comparison in histogram.
        final String behavioralTarget = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.QUERY_TILES_SEGMENTATION, BEHAVIOURAL_TARGETING_KEY);

        boolean shouldUseModelResult =
                !TextUtils.isEmpty(behavioralTarget) && TextUtils.equals(behavioralTarget, "model");
        boolean shouldCompareModelResult = !TextUtils.isEmpty(behavioralTarget)
                && TextUtils.equals(behavioralTarget, "model_comparison");

        long nextDecisionTimestamp = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS,
                INVALID_DECISION_TIMESTAMP);

        boolean noPreviousHistory = (nextDecisionTimestamp == INVALID_DECISION_TIMESTAMP);

        boolean nextDecisionTimestampReached = System.currentTimeMillis() >= nextDecisionTimestamp;

        // Use segmentation model result only if finch enabled and next decision is expired or
        // unavailable. If nextDecisionTimestamp is available and hasn't been reached, continue
        // using code algorithm.
        boolean lastDecisionExpired = noPreviousHistory || nextDecisionTimestampReached;
        if (shouldUseModelResult && lastDecisionExpired) {
            SharedPreferencesManager.getInstance().removeKey(
                    ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS);
            return getBehaviourResultFromSegmentation(getSegmentationResult(), false);
        }

        boolean resultFromCode;
        if (noPreviousHistory) {
            // If this is the first time we make a decision, don't show query tiles.
            resultFromCode = false;
            updateDisplayStatusAndNextDecisionTime(resultFromCode);
        } else if (nextDecisionTimestampReached) {
            int recentMVClicks = SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_MV_TILE_CLICKS, 0);
            int recentQueryTileClicks = SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_QUERY_TILE_CLICKS, 0);

            int mvTileClickThreshold = getFieldTrialParamValue(
                    MV_TILE_CLICKS_THRESHOLD_KEY, DEFAULT_MV_TILE_CLICKS_THRESHOLD);

            // If MV tiles is clicked recently, hide query tiles for a while.
            // Otherwise, show it for a period of time.
            resultFromCode = (recentMVClicks <= mvTileClickThreshold
                    || recentMVClicks <= recentQueryTileClicks);
            updateDisplayStatusAndNextDecisionTime(resultFromCode);
        } else {
            resultFromCode = SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.QUERY_TILES_SHOW_ON_NTP, false);
        }

        // Used for measuring consistency of segmentation model result. This is recorded for
        // every request.
        if (shouldCompareModelResult) {
            recordSegmentationResultComparison(getSegmentationResult(), resultFromCode);
        }

        return resultFromCode;
    }

    /**
     * Records UMA to compare the result of segmentation platform with hard coded logics.
     * @param segmentationResult The result from segmentation model.
     * @param existingResult The result from code logics.
     */
    private static void recordSegmentationResultComparison(
            @ShowQueryTilesSegmentationResult int segmentationResult, boolean existingResult) {
        @ShowQueryTilesSegmentationResultComparison
        int comparison = ShowQueryTilesSegmentationResultComparison.UNINITIALIZED;
        switch (segmentationResult) {
            case ShowQueryTilesSegmentationResult.UNINITIALIZED:
                comparison = ShowQueryTilesSegmentationResultComparison.UNINITIALIZED;
                break;
            case ShowQueryTilesSegmentationResult.SHOW:
                comparison = existingResult ? ShowQueryTilesSegmentationResultComparison
                                                      .SEGMENTATION_ENABLED_LOGIC_ENABLED
                                            : ShowQueryTilesSegmentationResultComparison
                                                      .SEGMENTATION_ENABLED_LOGIC_DISABLED;
                break;
            case ShowQueryTilesSegmentationResult.DONT_SHOW:
                comparison = existingResult ? ShowQueryTilesSegmentationResultComparison
                                                      .SEGMENTATION_DISABLED_LOGIC_ENABLED
                                            : ShowQueryTilesSegmentationResultComparison
                                                      .SEGMENTATION_DISABLED_LOGIC_DISABLED;
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Search.QueryTiles.ShowQueryTilesSegmentationResultComparison", comparison,
                ShowQueryTilesSegmentationResultComparison.NUM_ENTRIES);
    }

    /**
     * Called to check whether query tiles should be displayed based on segmentation model result.
     * @param segmentationResult The result from segmentation model.
     * @param defaultResult The default result.
     * @return Whether to show query tiles based on segmentation result. When unavailable, returns
     *         the default value given.
     */
    private static boolean getBehaviourResultFromSegmentation(
            @ShowQueryTilesSegmentationResult int segmentationResult, boolean defaultResult) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.QueryTiles.ShowQueryTilesSegmentationResult", segmentationResult,
                ShowQueryTilesSegmentationResult.NUM_ENTRIES);
        switch (segmentationResult) {
            case ShowQueryTilesSegmentationResult.DONT_SHOW:
                return false;
            case ShowQueryTilesSegmentationResult.SHOW:
                return true;

            case ShowQueryTilesSegmentationResult.UNINITIALIZED:
            default:
                return defaultResult;
        }
    }

    /**
     * Called to get segment selection result from segmentation platform service.
     * @return The segmentation result.
     */
    private static @ShowQueryTilesSegmentationResult int getSegmentationResult() {
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_SEGMENTATION);
        @ShowQueryTilesSegmentationResult
        int segmentationResult;
        if (sSegmentationResultsForTesting == -1) {
            SegmentationPlatformService segmentationPlatformService =
                    SegmentationPlatformServiceFactory.getForProfile(
                            Profile.getLastUsedRegularProfile());
            SegmentSelectionResult result = segmentationPlatformService.getCachedSegmentResult(
                    QUERY_TILES_SEGMENTATION_PLATFORM_KEY);
            if (!result.isReady) {
                segmentationResult = ShowQueryTilesSegmentationResult.UNINITIALIZED;
            } else if (result.selectedSegment
                    == SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES) {
                segmentationResult = ShowQueryTilesSegmentationResult.SHOW;
            } else {
                segmentationResult = ShowQueryTilesSegmentationResult.DONT_SHOW;
            }
        } else {
            segmentationResult = sSegmentationResultsForTesting;
        }

        return segmentationResult;
    }

    /**
     * Updates the display status for query tiles and set the next decision time.
     * @param showQueryTiles Whether query tiles should be displayed.
     */
    private static void updateDisplayStatusAndNextDecisionTime(boolean showQueryTiles) {
        long nextDecisionTime = System.currentTimeMillis();

        if (showQueryTiles) {
            nextDecisionTime += MILLISECONDS_PER_DAY
                    * getFieldTrialParamValue(NUM_DAYS_KEEP_SHOWING_QUERY_TILES_KEY,
                            DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES);
        } else {
            nextDecisionTime += MILLISECONDS_PER_DAY
                    * getFieldTrialParamValue(NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD_KEY,
                            DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD);
        }
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.QUERY_TILES_SHOW_ON_NTP, showQueryTiles);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS, nextDecisionTime);
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_MV_TILE_CLICKS);
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_QUERY_TILE_CLICKS);
    }

    /**
     * Called when most visited tile is clicked.
     */
    public static void onMostVisitedTileClicked() {
        incrementClickCountIfCloseToNextDecisionTime(
                ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_MV_TILE_CLICKS);
    }

    /**
     * Called when query tile is clicked.
     */
    public static void onQueryTileClicked() {
        incrementClickCountIfCloseToNextDecisionTime(
                ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_QUERY_TILE_CLICKS);
    }

    /**
     * Helper method to increment the click count if the click is close to the next decision date.
     * @param key The shared preference key to increment.
     */
    private static void incrementClickCountIfCloseToNextDecisionTime(String key) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_SEGMENTATION)) return;

        long now = System.currentTimeMillis();
        long nextDecisionTimestamp = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS,
                INVALID_DECISION_TIMESTAMP);
        if (nextDecisionTimestamp < now
                        + MILLISECONDS_PER_DAY
                                * getFieldTrialParamValue(NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD_KEY,
                                        DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD)) {
            SharedPreferencesManager.getInstance().incrementInt(key);
        }
    }

    /**
     * Getting the value for a field trial param.
     * @param key Key of the field trial param.
     * @param defaultValue Default value to use, if the param is missing.
     * @return The value for the field trial param, or default value if not specified.
     */
    private static int getFieldTrialParamValue(String key, int defaultValue) {
        String val = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.QUERY_TILES_SEGMENTATION, key);
        if (TextUtils.isEmpty(val)) return defaultValue;
        try {
            return Integer.parseInt(val);
        } catch (NumberFormatException e) {
            return defaultValue;
        }
    }

    /** For testing only. */
    @VisibleForTesting
    public static void setSegmentationResultsForTesting(int result) {
        sSegmentationResultsForTesting = result;
    }

    @NativeMethods
    interface Natives {
        boolean isQueryTilesEnabled();
    }
}
