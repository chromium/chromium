// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Handles various feature utility functions for query tiles.
 */
public class QueryTileUtils {
    private static Boolean sShowQueryTilesOnNTP;
    private static final String NUM_DAYS_KEEP_SHOWING_QUERY_TILES_KEY =
            "num_days_keep_showing_query_tiles";
    private static final String NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD_KEY =
            "num_days_mv_clicks_below_threshold";
    private static final String MV_TILE_CLICKS_THRESHOLD_KEY = "mv_tile_click_threshold";
    private static final long INVALID_DECISION_TIMESTAMP = -1L;

    @VisibleForTesting
    static final long MILLISECONDS_PER_DAY = TimeUtils.SECONDS_PER_DAY * 1000;
    @VisibleForTesting
    static final int DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES = 28;
    @VisibleForTesting
    static final int DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD = 7;

    /**
     * Whether query tiles is enabled and should be shown on NTP.
     * @return Whether the query tile feature is enabled on NTP.
     */
    public static boolean isQueryTilesEnabledOnNTP() {
        // Cache the result so it will not change during the same browser session.
        if (sShowQueryTilesOnNTP != null) return sShowQueryTilesOnNTP;
        sShowQueryTilesOnNTP = ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_GEO_FILTER)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_IN_NTP)
                && shouldShowQueryTiles();
        return sShowQueryTilesOnNTP;
    }

    /**
     * This is one experimental variation where user will have a chance of editing the query text
     * before starting a search. When a query tile is clicked, the query text will be pasted in the
     * omnibox. No second level tiles will be displayed. This is meant to show only one level of
     * query tiles.
     * @return Whether the user should have a chance to edit the query text before starting a
     *         search.
     */
    public static boolean isQueryEditingEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.QUERY_TILES_ENABLE_QUERY_EDITING);
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

        long nextDecisionTimestamp = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.QUERY_TILES_NEXT_DISPLAY_DECISION_TIME_MS,
                INVALID_DECISION_TIMESTAMP);

        boolean noPreviousHistory = (nextDecisionTimestamp == INVALID_DECISION_TIMESTAMP);
        // If this is the first time we make a decision, don't show query tiles.
        if (noPreviousHistory) {
            updateDisplayStatusAndNextDecisionTime(false /*showQueryTiles*/);
            return false;
        }

        // Return the current decision before the next decision timestamp.
        if (System.currentTimeMillis() < nextDecisionTimestamp) {
            return SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.QUERY_TILES_SHOW_ON_NTP, false);
        }

        int recentMVClicks = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_MV_TILE_CLICKS, 0);
        int recentQueryTileClicks = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.QUERY_TILES_NUM_RECENT_QUERY_TILE_CLICKS, 0);

        int mvTileClickThreshold = getFieldTrialParamValue(MV_TILE_CLICKS_THRESHOLD_KEY, 0);

        // If MV tiles is clicked recently, hide query tiles for a while.
        // Otherwise, show it for a period of time.
        boolean showQueryTiles =
                (recentMVClicks <= mvTileClickThreshold || recentMVClicks <= recentQueryTileClicks);

        updateDisplayStatusAndNextDecisionTime(showQueryTiles);
        return showQueryTiles;
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
}
