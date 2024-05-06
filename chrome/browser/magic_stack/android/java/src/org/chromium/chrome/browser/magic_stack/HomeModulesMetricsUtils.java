// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.TAB_RESUMPTION;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;

/** The utility class for magic stack. */
public class HomeModulesMetricsUtils {
    @VisibleForTesting static final String HISTOGRAM_OS_PREFIX = "MagicStack.Clank.";
    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_MODULE_CLICK = ".Module.Click";
    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_MODULE = ".Module.";

    @VisibleForTesting
    static final String HISTOGRAM_MAGIC_STACK_MODULE_CLICK_WITH_POSITION = ".Click";

    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_MODULE_BUILD = ".Build";
    static final String HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION_WITH_POSITION = ".Impression";

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

    private static final String FRESHNESS_INPUT_CONTEXT_SUFFIX = "_freshness";

    private static final String SINGLE_TAB_FRESHNESS_INPUT_CONTEXT = "single_tab_freshness";

    private static final String PRICE_CHANGE_FRESHNESS_INPUT_CONTEXT = "price_change_freshness";

    private static final String TAB_RESUMPTION_FRESHNESS_INPUT_CONTEXT = "tab_resumption_freshness";

    private static final String HOME_MODULES_SHOW_ALL_MODULES_PARAM = "show_all_modules";
    public static final BooleanCachedFieldTrialParameter HOME_MODULES_SHOW_ALL_MODULES =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.MAGIC_STACK_ANDROID,
                    HOME_MODULES_SHOW_ALL_MODULES_PARAM,
                    false);

    private static final String HOME_MODULES_COMBINE_TABS_PARAM = "show_tabs_in_one_module";
    public static final BooleanCachedFieldTrialParameter HOME_MODULES_COMBINE_TABS =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.MAGIC_STACK_ANDROID, HOME_MODULES_COMBINE_TABS_PARAM, false);

    /**
     * Returns a string name of a module. Remember to update the variant ModuleType in
     * tools/metrics/histograms/metadata/magic_stack/histograms.xml when adding a new module type
     */
    public static String getModuleName(@ModuleType int moduleType) {
        switch (moduleType) {
            case SINGLE_TAB:
                return "SingleTab";
            case (PRICE_CHANGE):
                return "PriceChange";
            case (TAB_RESUMPTION):
                return "TabResumption";
            default:
                assert false : "Module type not supported!";
                return null;
        }
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
            case (PRICE_CHANGE):
                return PRICE_CHANGE_FRESHNESS_INPUT_CONTEXT;
            case (TAB_RESUMPTION):
                return TAB_RESUMPTION_FRESHNESS_INPUT_CONTEXT;
            default:
                assert false : "Module type not supported!";
                return null;
        }
    }

    public static Integer convertLabelToModuleType(String label) {
        switch (label) {
            case "SingleTab":
                return ModuleType.SINGLE_TAB;
            case "PriceChange":
                return ModuleType.PRICE_CHANGE;
            case "TabResumption":
                return ModuleType.TAB_RESUMPTION;
            default:
                assert false : "Module type not supported!";
                return ModuleType.NUM_ENTRIES;
        }
    }

    /**
     * Records a module is shown.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     * @param modulePosition The position of the module on the recyclerview.
     */
    public static void recordModuleShown(
            @HostSurface int hostSurface, @ModuleType int moduleType, int modulePosition) {
        recordUma(hostSurface, moduleType, HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION);
        recordUmaWithPosition(
                hostSurface,
                HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION_WITH_POSITION,
                moduleType,
                modulePosition);
    }

    /**
     * Records the context menu is shown.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     */
    public static void recordContextMenuShown(
            @HostSurface int hostSurface, @ModuleType int moduleType) {
        recordUma(hostSurface, moduleType, HISTOGRAM_CONTEXT_MENU_SHOWN);
    }

    /**
     * Records the context menu "remove module" item is clicked.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     */
    public static void recordContextMenuRemoveModule(
            @HostSurface int hostSurface, @ModuleType int moduleType) {
        recordUma(hostSurface, moduleType, HISTOGRAM_CONTEXT_MENU_REMOVE_MODULE);
    }

    /**
     * Records the context menu item "customize" is clicked.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     */
    public static void recordContextMenuCustomizeSettings(
            @HostSurface int hostSurface, @ModuleType int moduleType) {
        recordUma(hostSurface, moduleType, HISTOGRAM_CONTEXT_MENU_OPEN_CUSTOMIZE_SETTINGS);
    }

    /**
     * Records the duration from building a module to the time when it returns a successful fetching
     * data response.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     * @param durationMs The time duration.
     */
    public static void recordFetchDataDuration(
            @HostSurface int hostSurface, @ModuleType int moduleType, long durationMs) {
        recordUma(hostSurface, moduleType, HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS, durationMs);
    }

    /**
     * Records the duration from building a module to the time when it returns a response of no data
     * to show.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     * @param durationMs The time duration.
     */
    public static void recordFetchDataFailedDuration(
            @HostSurface int hostSurface, @ModuleType int moduleType, long durationMs) {
        recordUma(
                hostSurface,
                moduleType,
                HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS,
                durationMs);
    }

    /**
     * Records the duration from building a module to the time when it returns a response after
     * timeout.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     * @param durationMs The time duration.
     */
    public static void recordFetchDataTimeOutDuration(
            @HostSurface int hostSurface, @ModuleType int moduleType, long durationMs) {
        recordUma(
                hostSurface,
                moduleType,
                HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS,
                durationMs);
    }

    /**
     * Records the types of modules which didn't respond before the timer timeout.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     */
    public static void recordFetchDataTimeOutType(
            @HostSurface int hostSurface, @ModuleType int moduleType) {
        recordUma(hostSurface, moduleType, HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE);
    }

    /**
     * Records the duration from building the first module to the time when the recyclerview becomes
     * visible, i.e., the first highest ranking module returns valid data.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param durationMs The time duration.
     */
    public static void recordFirstModuleShownDuration(
            @HostSurface int hostSurface, long durationMs) {
        recordUma(hostSurface, HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS, durationMs);
    }

    /**
     * Records the duration from calling the segmentation API to fetch a ranking to the time when a
     * response returns.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param durationMs The time duration.
     */
    public static void recordSegmentationFetchRankingDuration(
            @HostSurface int hostSurface, long durationMs) {
        recordUma(hostSurface, HISTOGRAM_MODULE_SEGMENTATION_FETCH_RANKING_DURATION_MS, durationMs);
    }

    /**
     * Records the time spent between triggering to show modules and beginning to fetch the module
     * list when the profile is ready.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param durationMs The time duration.
     */
    public static void recordProfileReadyDelay(@HostSurface int hostSurface, long durationMs) {
        recordUma(hostSurface, HISTOGRAM_MODULE_PROFILE_READY_DELAY_MS, durationMs);
    }

    /**
     * Records the total count of times that magic stack being scrollable or not, and, when it is
     * scrollable, the number of times it has been scrolled.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param isScrollable True if the home modules are scrollable.
     * @param hasScrolled True if home modules has been scrolled.
     */
    public static void recordHomeModulesScrollState(
            @HostSurface int hostSurface, boolean isScrollable, boolean hasScrolled) {
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
        builder.append(HISTOGRAM_OS_PREFIX);
        builder.append(BrowserUiUtils.getHostName(hostSurface));
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordCount1MHistogram(name, 1);
    }

    /**
     * Records the type and position of the module when the home modules are clicked.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     * @param modulePosition The position of the module which got clicked.
     */
    public static void recordModuleClicked(
            @HostSurface int hostSurface, @ModuleType int moduleType, int modulePosition) {
        recordUma(hostSurface, moduleType, HISTOGRAM_MAGIC_STACK_MODULE_CLICK);
        recordUmaWithPosition(
                hostSurface,
                HISTOGRAM_MAGIC_STACK_MODULE_CLICK_WITH_POSITION,
                moduleType,
                modulePosition);
    }

    /**
     * Records the type and position of the module when the home modules are added to the magic
     * stack.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     * @param modulePosition The position of the module when it is built in home modules.
     */
    public static void recordModuleBuiltPosition(
            @HostSurface int hostSurface, @ModuleType int moduleType, int modulePosition) {
        recordUmaWithPosition(
                hostSurface, HISTOGRAM_MAGIC_STACK_MODULE_BUILD, moduleType, modulePosition);
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

    private static void recordUmaWithPosition(
            @HostSurface int hostSurface,
            String umaName,
            @ModuleType int moduleType,
            int modulePosition) {
        assert 0 <= modulePosition && modulePosition < ModuleType.NUM_ENTRIES;
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_OS_PREFIX);
        builder.append(BrowserUiUtils.getHostName(hostSurface));
        builder.append(HISTOGRAM_MAGIC_STACK_MODULE);
        builder.append(getModuleName(moduleType));
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordEnumeratedHistogram(
                name, modulePosition, HomeModulesCoordinator.MAXIMUM_MODULE_SIZE);
    }

    private static void recordUma(
            @HostSurface int hostSurface, @ModuleType int moduleType, String umaName) {
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_OS_PREFIX);
        builder.append(BrowserUiUtils.getHostName(hostSurface));
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordEnumeratedHistogram(name, moduleType, ModuleType.NUM_ENTRIES);
    }

    private static void recordUma(@HostSurface int hostSurface, String umaName, long timeMs) {
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_OS_PREFIX);
        builder.append(BrowserUiUtils.getHostName(hostSurface));
        builder.append(umaName);
        String name = builder.toString();
        RecordHistogram.recordTimesHistogram(name, timeMs);
    }

    private static void recordUma(
            @HostSurface int hostSurface, @ModuleType int moduleType, String umaName, long timeMs) {
        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_OS_PREFIX);
        builder.append(BrowserUiUtils.getHostName(hostSurface));
        builder.append(umaName);
        builder.append(getModuleName(moduleType));
        String name = builder.toString();
        RecordHistogram.recordTimesHistogram(name, timeMs);
    }
}
