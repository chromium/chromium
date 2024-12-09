// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Utility class for the magic stack. */
public class HomeModulesUtils {
    static final long INVALID_TIMESTAMP = -1;
    static final int INVALID_FRESHNESS_SCORE = -1;

    /** Returns the preference key of the module type. */
    private static String getFreshnessCountPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        return ChromePreferenceKeys.HOME_MODULES_FRESHNESS_COUNT.createKey(
                String.valueOf(moduleType));
    }

    /** Gets the freshness count of a module. */
    @VisibleForTesting
    static int getFreshnessCount(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        return sharedPreferencesManager.readInt(freshnessScoreKey, INVALID_FRESHNESS_SCORE);
    }

    /** Called to reset the freshness count when there is new information to show. */
    @VisibleForTesting
    static void resetFreshnessCount(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        sharedPreferencesManager.writeInt(freshnessScoreKey, INVALID_FRESHNESS_SCORE);
    }

    /**
     * Called to increase the freshness score for the module. The count is increased from 0, not -1.
     */
    @VisibleForTesting
    static void increaseFreshnessCount(@ModuleType int moduleType, int count) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        int score =
                Math.max(
                        0,
                        sharedPreferencesManager.readInt(
                                freshnessScoreKey, INVALID_FRESHNESS_SCORE));
        sharedPreferencesManager.writeInt(freshnessScoreKey, (score + count));
    }

    /** Returns the preference key of the module type. */
    @VisibleForTesting
    static String getFreshnessTimeStampPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        return ChromePreferenceKeys.HOME_MODULES_FRESHNESS_TIMESTAMP_MS.createKey(
                String.valueOf(moduleType));
    }

    /** Sets the timestamp of last time a freshness score is logged for a module. */
    @VisibleForTesting
    static void setFreshnessScoreTimeStamp(@ModuleType int moduleType, long timeStampMs) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreTimeStampKey = getFreshnessTimeStampPreferenceKey(moduleType);
        sharedPreferencesManager.writeLong(freshnessScoreTimeStampKey, timeStampMs);
    }

    /** Gets the timestamp of last time a freshness score is logged for a module. */
    @VisibleForTesting
    static long getFreshnessScoreTimeStamp(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreTimeStampKey = getFreshnessTimeStampPreferenceKey(moduleType);
        return sharedPreferencesManager.readLong(freshnessScoreTimeStampKey, INVALID_TIMESTAMP);
    }

    public static void setFreshnessCountForTesting(@ModuleType int moduleType, int count) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        sharedPreferencesManager.writeInt(freshnessScoreKey, count);
    }

    public static void resetFreshnessTimeStampForTesting(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessTimeStampPreferenceKey(moduleType);
        sharedPreferencesManager.removeKey(freshnessScoreKey);
    }
}
