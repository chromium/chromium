// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.SINGLE_TAB;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;

/** The utility class for magic stack. */
public class HomeModulesMetricsUtils {
    @VisibleForTesting static final String HISTOGRAM_OS_PREFIX = "MagicStack.Clank.";
    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_MODULE_CLICK = ".Module.Click.";

    @VisibleForTesting
    static final String HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION = ".Module.TopImpression.";

    @VisibleForTesting static final String HISTOGRAM_CONTEXT_MENU_SHOWN = ".ContextMenu.Shown.";

    @VisibleForTesting
    static final String HISTOGRAM_CONTEXT_MENU_REMOVE_MODULE = ".ContextMenu.RemoveModule.";

    @VisibleForTesting
    static final String HISTOGRAM_CONTEXT_MENU_OPEN_CUSTOMIZE_SETTINGS =
            ".ContextMenu.OpenCustomizeSettings";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS = ".Module.FetchDataDurationMs.";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS =
            ".Module.FetchDataTimeoutDurationMs.";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE = ".Module.FetchDataTimeoutType.";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS =
            ".Module.FetchDataFailedDurationMs.";

    @VisibleForTesting
    static final String HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS =
            ".Module.FirstModuleShownDurationMs";

    @VisibleForTesting
    static final String HISTOGRAM_MODULE_SEGMENTATION_FETCH_RANKING_DURATION_MS =
            ".Segmentation.FetchRankingResultsDurationMs";

    /** Returns a string name of a module. */
    public static String getModuleName(@ModuleType int moduleType) {
        switch (moduleType) {
            case SINGLE_TAB:
                return "SingleTab";
            case (PRICE_CHANGE):
                return "PriceChange";
            default:
                assert false : "Module type not supported!";
                return null;
        }
    }

    /**
     * Records a module is shown.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     */
    public static void recordModuleShown(@HostSurface int hostSurface, @ModuleType int moduleType) {
        recordUma(hostSurface, moduleType, HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION);
    }

    /**
     * Records a module is clicked.
     *
     * @param hostSurface The type of the host surface of the magic stack.
     * @param moduleType The type of module.
     */
    public static void recordModuleClick(@HostSurface int hostSurface, @ModuleType int moduleType) {
        recordUma(hostSurface, moduleType, HISTOGRAM_MAGIC_STACK_MODULE_CLICK);
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

    private static void recordUma(
            @HostSurface int hostSurface, @ModuleType int moduleType, String umaName) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_OS_PREFIX + BrowserUiUtils.getHostName(hostSurface) + umaName,
                moduleType,
                ModuleType.NUM_ENTRIES);
    }

    private static void recordUma(@HostSurface int hostSurface, String umaName, long timeMs) {
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_OS_PREFIX + BrowserUiUtils.getHostName(hostSurface) + umaName, timeMs);
    }

    private static void recordUma(
            @HostSurface int hostSurface, @ModuleType int moduleType, String umaName, long timeMs) {
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_OS_PREFIX
                        + BrowserUiUtils.getHostName(hostSurface)
                        + umaName
                        + getModuleName(moduleType),
                timeMs);
    }
}
