// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE;
import static org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils.HISTOGRAM_MODULE_SEGMENTATION_FETCH_RANKING_DURATION_MS;

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

    @Test
    @SmallTest
    public void testRecordFetchDataDuration() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int duration = 100;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HISTOGRAM_MODULE_FETCH_DATA_DURATION_MS
                        + HomeModulesMetricsUtils.getModuleName(moduleType);

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataDuration(hostSurface, moduleType, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFetchDataTimeoutDuration() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int duration = 100;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_DURATION_MS
                        + HomeModulesMetricsUtils.getModuleName(moduleType);

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataTimeOutDuration(hostSurface, moduleType, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFetchDataTimeoutType() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName =
                "MagicStack.Clank.StartSurface" + HISTOGRAM_MODULE_FETCH_DATA_TIMEOUT_TYPE;

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordFetchDataTimeOutType(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFetchDataFailedDuration() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int duration = 100;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HISTOGRAM_MODULE_FETCH_DATA_FAILED_DURATION_MS
                        + HomeModulesMetricsUtils.getModuleName(moduleType);

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataFailedDuration(hostSurface, moduleType, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFirstModuleShowDuration() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        int duration = 100;
        String histogramName =
                "MagicStack.Clank.StartSurface" + HISTOGRAM_FIRST_MODULE_SHOWN_DURATION_MS;

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFirstModuleShownDuration(hostSurface, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordSegmentationFetchRankingDuration() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        int duration = 100;
        String histogramName =
                "MagicStack.Clank.StartSurface"
                        + HISTOGRAM_MODULE_SEGMENTATION_FETCH_RANKING_DURATION_MS;

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordSegmentationFetchRankingDuration(hostSurface, duration);
        histogramWatcher.assertExpected();
    }
}
