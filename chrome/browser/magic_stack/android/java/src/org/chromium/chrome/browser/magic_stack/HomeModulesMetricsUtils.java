// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.AUXILIARY_SEARCH;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.DEFAULT_BROWSER_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.DEPRECATED_EDUCATIONAL_TIP;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.DEPRECATED_TAB_RESUMPTION;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.HISTORY_SYNC_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.QUICK_DELETE_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SAFETY_HUB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_GROUP_PROMO;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_GROUP_SYNC_PROMO;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

import java.util.HashSet;

/** The utility class for logging the magic stack's metrics. */
@NullMarked
public class HomeModulesMetricsUtils {
    @VisibleForTesting public static final String HISTOGRAM_PREFIX = "MagicStack.Clank.NewTabPage";
    @VisibleForTesting public static final String HISTOGRAM_MAGIC_STACK_MODULE = ".Module.";

    @VisibleForTesting
    public static final String HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION_WITH_POSITION =
            ".Impression";

    @VisibleForTesting static final String HISTOGRAM_OS_PREFIX = "MagicStack.Clank.";
    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_MODULE_CLICK = ".Module.Click";
    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_HOST_SURFACE_REGULAR = ".Regular";
    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_HOST_SURFACE_STARTUP = ".Startup";

    @VisibleForTesting
    static final String HISTOGRAM_MAGIC_STACK_MODULE_CLICK_WITH_POSITION = ".Click";

    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_MODULE_BUILD = ".Build";

    @VisibleForTesting
    static final String HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION = ".Module.TopImpressionV2";

    @VisibleForTesting static final String HISTOGRAM_CONTEXT_MENU_SHOWN = ".ContextMenu.ShownV2";

    @VisibleForTesting
    static final String HISTOGRAM_CONTEXT_MENU_REMOVE_MODULE = ".ContextMenu.RemoveModuleV2";

    @VisibleForTesting
    static final String HISTOGRAM_CONTEXT_MENU_OPEN_CUSTOMIZE_SETTINGS =
            ".ContextMenu.OpenCustomizeSettings";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS = ".Module.FetchDataDurationMs.";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS =
            ".Module.FetchDataTimeoutDurationMs.";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE = ".Module.FetchDataTimeoutTypeV2";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS =
            ".Module.FetchDataFailedDurationMs.";

    @VisibleForTesting
    static final String HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS =
            ".Module.FirstModuleShownDurationMs";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_SEGMENTATION_FETCH_RANKING_DURATION_MS =
            ".Segmentation.FetchRankingResultsDurationMs";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_PROFILE_READY_DELAY_MS = ".Module.ProfileReadyDelayMs";

    @VisibleForTesting
    static final String HISTOGRAM_MAGIC_STACK_SCROLLABLE_SCROLLED = ".Scrollable.Scrolled";

    @VisibleForTesting
    static final String HISTOGRAM_MAGIC_STACK_SCROLLABLE_NOTSCROLLED = ".Scrollable.NotScrolled";

    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_NOT_SCROLLABLE = ".NotScrollable";

    @VisibleForTesting
    static final String HISTOGRAM_CONFIGURATION_TURN_ON_MODULE = "Settings.TurnOnModule";

    @VisibleForTesting
    static final String HISTOGRAM_CONFIGURATION_TURN_OFF_MODULE = "Settings.TurnOffModule";

    @VisibleForTesting
    static final String HISTOGRAM_EDUCATIONAL_TIP_MODULE_IMPRESSION_COUNT_BEFORE_INTERACTION =
            ".ImpressionCountBeforeInteraction";

    private static final String TAG = "HomeModules";

    /**
     * Returns a string name of a module. Remember to update the variant ModuleType in
     * tools/metrics/histograms/metadata/magic_stack/histograms.xml when adding a new module type
     */
    public static String getModuleName(@ModuleType int moduleType) {
        switch (moduleType) {
            case SINGLE_TAB:
                return "SingleTab";
            case PRICE_CHANGE:
                return "PriceChange";
            case SAFETY_HUB:
                return "SafetyHub";
            case AUXILIARY_SEARCH:
                return "AuxiliarySearch";
            case DEFAULT_BROWSER_PROMO:
                return "DefaultBrowserPromo";
            case TAB_GROUP_PROMO:
                return "TabGroupPromo";
            case TAB_GROUP_SYNC_PROMO:
                return "TabGroupSyncPromo";
            case QUICK_DELETE_PROMO:
                return "QuickDeletePromo";
            case HISTORY_SYNC_PROMO:
                return "HistorySyncPromo";
            default:
                assert false : "Module type not supported!";
                return assumeNonNull(null);
        }
    }

    public static Integer convertLabelToModuleType(String label) {
        switch (label) {
            case "SingleTab":
                return SINGLE_TAB;
            case "PriceChange":
                return PRICE_CHANGE;
            case "SafetyHub":
                return SAFETY_HUB;
            case "AuxiliarySearch":
                return AUXILIARY_SEARCH;
            case "DefaultBrowserPromo":
                return DEFAULT_BROWSER_PROMO;
            case "TabGroupPromo":
                return TAB_GROUP_PROMO;
            case "TabGroupSyncPromo":
                return TAB_GROUP_SYNC_PROMO;
            case "QuickDeletePromo":
                return QUICK_DELETE_PROMO;
            case "HistorySyncPromo":
                return HISTORY_SYNC_PROMO;
            default:
                Log.i(TAG, "Module type %s not supported!", label);
                return ModuleType.NUM_ENTRIES;
        }
    }

    /** Returns a list of all modules that are not deprecated. */
    static HashSet<Integer> getAllActiveModulesForTesting() {
        HashSet<Integer> set = new HashSet<>();
        for (@ModuleType int moduleType = 0; moduleType < ModuleType.NUM_ENTRIES; moduleType++) {
            if (moduleType == DEPRECATED_EDUCATIONAL_TIP
                    || moduleType == DEPRECATED_TAB_RESUMPTION) {
                continue;
            }
            set.add(moduleType);
        }
        return set;
    }

    /**
     * Records a module is shown.
     *
     * @param moduleType The type of module.
     * @param modulePosition The position of the module on the recyclerview.
     * @param isShownAtStartup Whether the host surface is a home surface which is shown at startup.
     */
    public static void recordModuleShown(
            @ModuleType int moduleType, int modulePosition, boolean isShownAtStartup) {
        recordUma(moduleType, HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION);
        recordUmaWithPosition(
                HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION_WITH_POSITION,
                moduleType,
                modulePosition,
                isShownAtStartup);
    }

    /**
     * Records the context menu is shown.
     *
     * @param moduleType The type of module.
     */
    public static void recordContextMenuShown(@ModuleType int moduleType) {
        recordUma(moduleType, HISTOGRAM_CONTEXT_MENU_SHOWN);
    }

    /**
     * Records the context menu "remove module" item is clicked.
     *
     * @param moduleType The type of module.
     */
    public static void recordContextMenuRemoveModule(@ModuleType int moduleType) {
        recordUma(moduleType, HISTOGRAM_CONTEXT_MENU_REMOVE_MODULE);
    }

    /**
     * Records the context menu item "customize" is clicked.
     *
     * @param moduleType The type of module.
     */
    public static void recordContextMenuCustomizeSettings(@ModuleType int moduleType) {
        recordUma(moduleType, HISTOGRAM_CONTEXT_MENU_OPEN_CUSTOMIZE_SETTINGS);
    }

    /**
     * Records the duration from building a module to the time when it returns a successful fetching
     * data response.
     *
     * @param moduleType The type of module.
     * @param durationMs The time duration.
     */
    public static void recordFetchDataDuration(@ModuleType int moduleType, long durationMs) {
        recordUma(moduleType, HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS, durationMs);
    }

    /**
     * Records the duration from building a module to the time when it returns a response of no data
     * to show.
     *
     * @param moduleType The type of module.
     * @param durationMs The time duration.
     */
    public static void recordFetchDataFailedDuration(@ModuleType int moduleType, long durationMs) {
        recordUma(moduleType, HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS, durationMs);
    }

    /**
     * Records the duration from building a module to the time when it returns a response after
     * timeout.
     *
     * @param moduleType The type of module.
     * @param durationMs The time duration.
     */
    public static void recordFetchDataTimeOutDuration(@ModuleType int moduleType, long durationMs) {
        recordUma(moduleType, HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS, durationMs);
    }

    /**
     * Records the types of modules which didn't respond before the timer timeout.
     *
     * @param moduleType The type of module.
     */
    public static void recordFetchDataTimeOutType(@ModuleType int moduleType) {
        recordUma(moduleType, HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE);
    }

    /**
     * Records the duration from building the first module to the time when the recyclerview becomes
     * visible, i.e., the first highest ranking module returns valid data.
     *
     * @param durationMs The time duration.
     */
    public static void recordFirstModuleShownDuration(long durationMs) {
        recordUma(HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS, durationMs);
    }

    /**
     * Records the duration from calling the segmentation API to fetch a ranking to the time when a
     * response returns.
     *
     * @param durationMs The time duration.
     */
    public static void recordSegmentationFetchRankingDuration(long durationMs) {
        recordUma(HISTOGRAM_MODULE_SEGMENTATION_FETCH_RANKING_DURATION_MS, durationMs);
    }

    /**
     * Records the time spent between triggering to show modules and beginning to fetch the module
     * list when the profile is ready.
     *
     * @param durationMs The time duration.
     */
    public static void recordProfileReadyDelay(long durationMs) {
        recordUma(HISTOGRAM_MODULE_PROFILE_READY_DELAY_MS, durationMs);
    }

    /**
     * Records the total count of times that magic stack being scrollable or not, and, when it is
     * scrollable, the number of times it has been scrolled.
     *
     * @param isScrollable True if the home modules are scrollable.
     * @param hasScrolled True if home modules has been scrolled.
     */
    public static void recordHomeModulesScrollState(boolean isScrollable, boolean hasScrolled) {
        String umaName;
        if (isScrollable) {
            if (hasScrolled) {
                umaName = HISTOGRAM_MAGIC_STACK_SCROLLABLE_SCROLLED;
            } else {
                umaName = HISTOGRAM_MAGIC_STACK_SCROLLABLE_NOTSCROLLED;
            }
        } else {
            umaName = HISTOGRAM_MAGIC_STACK_NOT_SCROLLABLE;
        }
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_PREFIX);
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordCount1MHistogram(name, 1);
    }

    /**
     * Records the type and position of the module when the home modules are clicked.
     *
     * @param moduleType The type of module.
     * @param modulePosition The position of the module which got clicked.
     * @param isShownAtStartup Whether the host surface is a home surface which is shown at startup.
     */
    public static void recordModuleClicked(
            @ModuleType int moduleType, int modulePosition, boolean isShownAtStartup) {
        BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.MAGIC_STACK);
        recordUma(moduleType, HISTOGRAM_MAGIC_STACK_MODULE_CLICK);
        recordUmaWithPosition(
                HISTOGRAM_MAGIC_STACK_MODULE_CLICK_WITH_POSITION,
                moduleType,
                modulePosition,
                isShownAtStartup);
    }

    /**
     * Records the type and position of the module when the home modules are added to the magic
     * stack.
     *
     * @param moduleType The type of module.
     * @param modulePosition The position of the module when it is built in home modules.
     * @param isShownAtStartup Whether the host surface is a home surface which is shown at startup.
     */
    public static void recordModuleBuiltPosition(
            @ModuleType int moduleType, int modulePosition, boolean isShownAtStartup) {
        recordUmaWithPosition(
                HISTOGRAM_MAGIC_STACK_MODULE_BUILD, moduleType, modulePosition, isShownAtStartup);
    }

    /**
     * Records when a module is activated or deactivated in the configuration page of the magic
     * stack.
     *
     * @param moduleType The type of module.
     * @param isEnabled True if the module is turned on.
     */
    public static void recordModuleToggledInConfiguration(
            @ModuleType int moduleType, boolean isEnabled) {
        String umaName =
                isEnabled
                        ? HISTOGRAM_CONFIGURATION_TURN_ON_MODULE
                        : HISTOGRAM_CONFIGURATION_TURN_OFF_MODULE;
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_OS_PREFIX + umaName, moduleType, ModuleType.NUM_ENTRIES);
    }

    /** Returns whether a magic stack is enabled on Start surface. */
    public static boolean useMagicStack() {
        return ChromeFeatureList.sMagicStackAndroid.isEnabled();
    }

    /**
     * Records how many times an educational tip module appears to the user before it is clicked.
     *
     * @param moduleType The type of module.
     * @param isShownAtStartup Whether the host surface is a home surface which is shown at startup.
     * @param count The number of times the module has appeared to the user.
     */
    public static void recordEducationalTipModuleImpressionCountBeforeInteraction(
            @ModuleType int moduleType, boolean isShownAtStartup, int count) {
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_PREFIX);
        if (isShownAtStartup) {
            builder.append(HISTOGRAM_MAGIC_STACK_HOST_SURFACE_STARTUP);
        } else {
            builder.append(HISTOGRAM_MAGIC_STACK_HOST_SURFACE_REGULAR);
        }
        builder.append(HISTOGRAM_MAGIC_STACK_MODULE);
        builder.append(getModuleName(moduleType));
        builder.append(HISTOGRAM_EDUCATIONAL_TIP_MODULE_IMPRESSION_COUNT_BEFORE_INTERACTION);
        String name = builder.toString();
        RecordHistogram.recordCount100Histogram(name, count);
    }

    private static void recordUmaWithPosition(
            String umaName,
            @ModuleType int moduleType,
            int modulePosition,
            boolean isShownAtStartup) {
        assert 0 <= modulePosition && modulePosition < ModuleType.NUM_ENTRIES;
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_PREFIX);
        if (isShownAtStartup) {
            builder.append(HISTOGRAM_MAGIC_STACK_HOST_SURFACE_STARTUP);
        } else {
            builder.append(HISTOGRAM_MAGIC_STACK_HOST_SURFACE_REGULAR);
        }
        builder.append(HISTOGRAM_MAGIC_STACK_MODULE);
        builder.append(getModuleName(moduleType));
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordEnumeratedHistogram(
                name, modulePosition, HomeModulesCoordinator.MAXIMUM_MODULE_SIZE);
    }

    private static void recordUma(@ModuleType int moduleType, String umaName) {
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_PREFIX);
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordEnumeratedHistogram(name, moduleType, ModuleType.NUM_ENTRIES);
    }

    private static void recordUma(String umaName, long timeMs) {
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_PREFIX);
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordTimesHistogram(name, timeMs);
    }

    private static void recordUma(@ModuleType int moduleType, String umaName, long timeMs) {
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_PREFIX);
        builder.append(umaName);
        builder.append(getModuleName(moduleType));
        String name = builder.toString();
        RecordHistogram.recordTimesHistogram(name, timeMs);
    }
}
