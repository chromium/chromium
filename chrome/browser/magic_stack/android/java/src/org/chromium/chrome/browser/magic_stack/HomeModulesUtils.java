// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.AUXILIARY_SEARCH;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.DEFAULT_BROWSER_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.HISTORY_SYNC_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.QUICK_DELETE_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SAFETY_HUB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_GROUP_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_GROUP_SYNC_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TIPS_NOTIFICATIONS_PROMO;

import android.content.Context;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Arrays;
import java.util.HashSet;

/** Utility class for the magic stack. */
@NullMarked
public class HomeModulesUtils {
    static final long INVALID_TIMESTAMP = -1;
    static final int INVALID_FRESHNESS_SCORE = -1;
    static final int INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION = 0;

    private static final String SINGLE_TAB_FRESHNESS_INPUT_CONTEXT = "single_tab_freshness";

    private static final String PRICE_CHANGE_FRESHNESS_INPUT_CONTEXT = "price_change_freshness";

    private static final String SAFETY_HUB_FRESHNESS_INPUT_CONTEXT = "safety_hub_freshness";

    private static final String AUXILIARY_SEARCH_FRESHNESS_INPUT_CONTEXT =
            "auxiliary_search_freshness";

    // List of all educational tip modules.
    private static final HashSet<Integer> sEducationalTipCardList =
            new HashSet<>(
                    Arrays.asList(
                            DEFAULT_BROWSER_PROMO,
                            TAB_GROUP_PROMO,
                            TAB_GROUP_SYNC_PROMO,
                            QUICK_DELETE_PROMO,
                            HISTORY_SYNC_PROMO,
                            TIPS_NOTIFICATIONS_PROMO));

    static boolean belongsToEducationalTipModule(@ModuleType int moduleType) {
        return sEducationalTipCardList.contains(moduleType);
    }

    static HashSet<Integer> getEducationalTipModuleList() {
        return sEducationalTipCardList;
    }

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
            case SAFETY_HUB:
                return SAFETY_HUB_FRESHNESS_INPUT_CONTEXT;
            case AUXILIARY_SEARCH:
                return AUXILIARY_SEARCH_FRESHNESS_INPUT_CONTEXT;
            default:
                assert false : "Module type not supported!";
                return assumeNonNull(null);
        }
    }

    /**
     * @param moduleType Type of the home module
     * @param context The application {@link Context} instance.
     * @return The string of switch title for the module type.
     */
    public static String getTitleForModuleType(@ModuleType int moduleType, Context context) {
        switch (moduleType) {
            case SINGLE_TAB:
                return context.getString(R.string.home_modules_single_tab_title);
            case PRICE_CHANGE:
                return context.getString(R.string.price_change_module_name);
            case SAFETY_HUB:
                return context.getString(R.string.safety_hub_magic_stack_module_name);
            case DEFAULT_BROWSER_PROMO:
            case TAB_GROUP_PROMO:
            case TAB_GROUP_SYNC_PROMO:
            case QUICK_DELETE_PROMO:
            case HISTORY_SYNC_PROMO:
            case TIPS_NOTIFICATIONS_PROMO:
                // All tips use the same name.
                return context.getString(R.string.educational_tip_module_name);
            case AUXILIARY_SEARCH:
                return context.getString(R.string.auxiliary_search_module_name);
            default:
                assert false : "Module type not supported!";
                return assumeNonNull(null);
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

    /** Returns the preference key of the module type for impression count before interaction. */
    private static String getImpressionCountBeforeInteractionPreferenceKey(
            @ModuleType int moduleType) {
        assert HomeModulesUtils.belongsToEducationalTipModule(moduleType);

        return ChromePreferenceKeys.HOME_MODULES_IMPRESSION_COUNT_BEFORE_INTERACTION.createKey(
                String.valueOf(moduleType));
    }

    /**
     * Called to increase the impression count before interaction for the module type provided by 1.
     * The count is increased from 0.
     */
    @VisibleForTesting
    public static void increaseImpressionCountBeforeInteraction(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String impressionCountBeforeInteractionKey =
                getImpressionCountBeforeInteractionPreferenceKey(moduleType);
        int totalCount =
                sharedPreferencesManager.readInt(
                                impressionCountBeforeInteractionKey,
                                INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION)
                        + 1;
        sharedPreferencesManager.writeInt(impressionCountBeforeInteractionKey, totalCount);
    }

    /** Gets the impression count before interaction of a module. */
    @VisibleForTesting
    static int getImpressionCountBeforeInteraction(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String impressionCountBeforeInteractionKey =
                getImpressionCountBeforeInteractionPreferenceKey(moduleType);
        return sharedPreferencesManager.readInt(
                impressionCountBeforeInteractionKey, INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION);
    }

    /** Called to remove the impression count before interaction shared preference key. */
    @VisibleForTesting
    public static void removeImpressionCountBeforeInteractionKey(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String impressionCountBeforeInteractionKey =
                getImpressionCountBeforeInteractionPreferenceKey(moduleType);
        sharedPreferencesManager.removeKey(impressionCountBeforeInteractionKey);
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

    /** Returns the preference key of the module type. */
    public static String getSettingsPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        // All the educational tip modules are controlled by the same preference key.
        if (HomeModulesUtils.belongsToEducationalTipModule(moduleType)) {
            return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                    String.valueOf(DEFAULT_BROWSER_PROMO));
        }

        return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(moduleType));
    }

    /**
     * Updates the C++ boolean user pref for profile {@param profile} with key {@param cKey}, to
     * have the same value as the Java SharedPreference with key {@param javaKey}.
     *
     * @param javaKey The key of the Java preference.
     * @param cKey The key of the C++ preference.
     * @param profile The profile that the preference is associated with.
     */
    public static void updateBooleanUserPrefs(String javaKey, String cKey, Profile profile) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        if (sharedPreferencesManager.contains(javaKey)) {
            // Default value should not be read since we already checked that the key was set.
            boolean value =
                    sharedPreferencesManager.readBoolean(javaKey, /* defaultValue= */ false);
            UserPrefs.get(profile).setBoolean(cKey, value);
        }
    }
}
