// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;

/** The utility class for magic stack. */
public class HomeModulesMetricsUtils {
    private static final String HISTOGRAM_OS_PREFIX = "MagicStack.Clank.";
    @VisibleForTesting static final String HISTOGRAM_MAGIC_STACK_MODULE_CLICK = ".Module.Click.";

    @VisibleForTesting
    static final String HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION = ".Module.TopImpression.";

    @VisibleForTesting static final String HISTOGRAM_CONTEXT_MENU_SHOWN = ".ContextMenu.Shown.";

    @VisibleForTesting
    static final String HISTOGRAM_CONTEXT_MENU_REMOVE_MODULE = ".ContextMenu.RemoveModule.";

    @VisibleForTesting
    static final String HISTOGRAM_CONTEXT_MENU_OPEN_CUSTOMIZE_SETTINGS =
            ".ContextMenu.OpenCustomizeSettings";

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

    private static void recordUma(
            @HostSurface int hostSurface, @ModuleType int moduleType, String umaName) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_OS_PREFIX + BrowserUiUtils.getHostName(hostSurface) + umaName,
                moduleType,
                ModuleType.NUM_ENTRIES);
    }
}
