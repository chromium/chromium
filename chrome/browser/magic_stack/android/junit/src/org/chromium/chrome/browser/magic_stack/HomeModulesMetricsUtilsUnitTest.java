// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;

/** Unit tests for {@link HomeModulesMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesMetricsUtilsUnitTest {
    @Test
    @SmallTest
    public void testRecordModuleClick() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HomeModulesMetricsUtils.HISTOGRAM_MAGIC_STACK_MODULE_CLICK;

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordModuleClick(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordModuleShown() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HomeModulesMetricsUtils.HISTOGRAM_MAGIC_STACK_MODULE_IMPRESSION;

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordModuleShown(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuShown() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HomeModulesMetricsUtils.HISTOGRAM_CONTEXT_MENU_SHOWN;

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuShown(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuRemoveModule() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HomeModulesMetricsUtils.HISTOGRAM_CONTEXT_MENU_REMOVE_MODULE;

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuRemoveModule(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuCustomizeSettings() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HomeModulesMetricsUtils.HISTOGRAM_CONTEXT_MENU_OPEN_CUSTOMIZE_SETTINGS;

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuCustomizeSettings(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }
}
