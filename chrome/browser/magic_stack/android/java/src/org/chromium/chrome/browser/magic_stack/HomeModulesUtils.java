// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.AUXILIARY_SEARCH;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SAFETY_HUB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_RESUMPTION;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;

/** Utility class for the magic stack. */
public class HomeModulesUtils {
    static final long INVALID_TIMESTAMP = -1;
    static final int INVALID_FRESHNESS_SCORE = -1;

    private static final String SINGLE_TAB_FRESHNESS_INPUT_CONTEXT = "single_tab_freshness";

    private static final String PRICE_CHANGE_FRESHNESS_INPUT_CONTEXT = "price_change_freshness";

    private static final String TAB_RESUMPTION_FRESHNESS_INPUT_CONTEXT = "tab_resumption_freshness";

    private static final String SAFETY_HUB_FRESHNESS_INPUT_CONTEXT = "safety_hub_freshness";

    private static final String AUXILIARY_SEARCH_FRESHNESS_INPUT_CONTEXT =
            "auxiliary_search_freshness";

    /**
     * Returns the freshness score key used by InputContext for the given module. Remember to update
     * the variant ModuleType in tools/metrics/histograms/metadata/magic_stack/histograms.xml when
     * adding a new module type
     */
    public static String getFreshnessInputContextString(@ModuleType int moduleType) {
        switch (moduleType) {
            case SINGLE_TAB:
                return SINGLE_TAB_FRESHNESS_INPUT_CONTEXT;
            case PRICE_CHANGE:
                return PRICE_CHANGE_FRESHNESS_INPUT_CONTEXT;
            case TAB_RESUMPTION:
                return TAB_RESUMPTION_FRESHNESS_INPUT_CONTEXT;
            case SAFETY_HUB:
                return SAFETY_HUB_FRESHNESS_INPUT_CONTEXT;
            case AUXILIARY_SEARCH:
                return AUXILIARY_SEARCH_FRESHNESS_INPUT_CONTEXT;
            default:
                assert false : "Module type not supported!";
                return null;
        }
    }

    /** Returns the preference key of the module type. */
    private static String getFreshnessCountPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        return ChromePreferenceKeys.HOME_MODULES_FRESHNESS_COUNT.createKey(
                String.valueOf(moduleType));
    }

    /** Called to reset the freshness count when there is new information to show. */
    @VisibleForTesting
    public static void resetFreshnessCountAsFresh(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        sharedPreferencesManager.writeInt(freshnessScoreKey, 0);

        setFreshnessScoreTimeStamp(moduleType, TimeUtils.uptimeMillis());
    }

    /**
     * Called to increase the freshness score for the module. The count is increased from 0, not -1.
     */
    @VisibleForTesting
    public static void increaseFreshnessCount(@ModuleType int moduleType, int count) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        int score =
                Math.max(
                        0,
                        sharedPreferencesManager.readInt(
                                freshnessScoreKey, INVALID_FRESHNESS_SCORE));
        sharedPreferencesManager.writeInt(freshnessScoreKey, (score + count));

        setFreshnessScoreTimeStamp(moduleType, TimeUtils.uptimeMillis());
    }

    /** Gets the freshness count of a module. */
    @VisibleForTesting
    static int getFreshnessCount(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        return sharedPreferencesManager.readInt(freshnessScoreKey, INVALID_FRESHNESS_SCORE);
    }

    /** Returns the preference key of the module type. */
    private static String getFreshnessTimeStampPreferenceKey(@ModuleType int moduleType) {
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

    /**
     * Returns whether the feature flag
     * ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2 is enabled.
     */
    static boolean isHomeModuleRankerV2Enabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2);
    }

    /** Creates an InputContext for the given module type. */
    public static InputContext createInputContext(@ModuleType int moduleType) {
        InputContext inputContext = new InputContext();
        inputContext.addEntry(
                getFreshnessInputContextString(moduleType),
                ProcessedValue.fromFloat(
                        getFreshnessScore(isHomeModuleRankerV2Enabled(), moduleType)));
        return inputContext;
    }

    /** Returns the freshness score of a module if valid. */
    @VisibleForTesting
    static int getFreshnessScore(boolean useFreshnessScore, @ModuleType int moduleType) {
        if (!useFreshnessScore) return INVALID_FRESHNESS_SCORE;

        long timeStamp = getFreshnessScoreTimeStamp(moduleType);
        if (timeStamp == INVALID_TIMESTAMP
                || SystemClock.elapsedRealtime() - timeStamp
                        >= HomeModulesMediator.FRESHNESS_THRESHOLD_MS) {
            return INVALID_FRESHNESS_SCORE;
        }

        return getFreshnessCount(moduleType);
    }

    public static void setFreshnessCountForTesting(
            @ModuleType int moduleType, int count, long timeStamp) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        sharedPreferencesManager.writeInt(freshnessScoreKey, count);

        setFreshnessScoreTimeStamp(moduleType, timeStamp);
    }

    public static void resetFreshnessTimeStampForTesting(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessTimeStampPreferenceKey(moduleType);
        sharedPreferencesManager.removeKey(freshnessScoreKey);
    }
}
