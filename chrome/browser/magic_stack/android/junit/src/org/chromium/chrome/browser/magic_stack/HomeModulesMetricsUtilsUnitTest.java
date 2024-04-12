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
    public void testRecordModuleShown() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int modulePosition = 2;
        String histogramName = "MagicStack.Clank.StartSurface.Module.TopImpressionV2";
        String histogramNameWithPosition =
                "MagicStack.Clank.StartSurface.Module.SingleTab.Impression";

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(histogramName, moduleType)
                        .expectIntRecord(histogramNameWithPosition, modulePosition)
                        .build();
        HomeModulesMetricsUtils.recordModuleShown(hostSurface, moduleType, modulePosition);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuShown() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName = "MagicStack.Clank.StartSurface.ContextMenu.ShownV2";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuShown(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuRemoveModule() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName = "MagicStack.Clank.StartSurface.ContextMenu.RemoveModuleV2";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuRemoveModule(hostSurface, moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuCustomizeSettings() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName = "MagicStack.Clank.StartSurface.ContextMenu.OpenCustomizeSettings";

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

        String histogramName = "MagicStack.Clank.StartSurface.Module.FetchDataDurationMs.SingleTab";

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
                "MagicStack.Clank.StartSurface.Module.FetchDataTimeoutDurationMs.SingleTab";

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
        String histogramName = "MagicStack.Clank.StartSurface.Module.FetchDataTimeoutTypeV2";

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
                "MagicStack.Clank.StartSurface.Module.FetchDataFailedDurationMs.SingleTab";

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
        String histogramName = "MagicStack.Clank.StartSurface.Module.FirstModuleShownDurationMs";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFirstModuleShownDuration(hostSurface, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordProfileReadyDelay() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        int duration = 100;
        String histogramName = "MagicStack.Clank.StartSurface.Module.ProfileReadyDelayMs";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordProfileReadyDelay(hostSurface, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordSegmentationFetchRankingDuration() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        int duration = 100;
        String histogramName =
                "MagicStack.Clank.StartSurface.Segmentation.FetchRankingResultsDurationMs";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordSegmentationFetchRankingDuration(hostSurface, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordModuleClicked() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int modulePosition = 2;

        String histogramName = "MagicStack.Clank.StartSurface.Module.Click";
        String histogramNameWithPosition = "MagicStack.Clank.StartSurface.Module.SingleTab.Click";

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, moduleType)
                        .expectIntRecords(histogramNameWithPosition, modulePosition)
                        .build();
        HomeModulesMetricsUtils.recordModuleClicked(hostSurface, moduleType, modulePosition);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordHomeModulesScrollState() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        boolean isScrollable = true;
        boolean isScrolled = true;
        String histogramName = "MagicStack.Clank.StartSurface.Scrollable.Scrolled";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, 1);
        HomeModulesMetricsUtils.recordHomeModulesScrollState(hostSurface, isScrollable, isScrolled);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordModuleToggledInConfiguration() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;
        boolean isEnabled = true;
        String histogramName = "MagicStack.Clank.Settings.TurnOnModule";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordModuleToggledInConfiguration(moduleType, isEnabled);
        histogramWatcher.assertExpected();

        isEnabled = false;
        histogramName = "MagicStack.Clank.Settings.TurnOffModule";

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordModuleToggledInConfiguration(moduleType, isEnabled);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordModuleBuiltPosition() {
        @HostSurface int hostSurface = HostSurface.START_SURFACE;
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int modulePosition = 2;

        String histogramName = "MagicStack.Clank.StartSurface.Module.SingleTab.Build";

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, modulePosition);
        HomeModulesMetricsUtils.recordModuleBuiltPosition(hostSurface, moduleType, modulePosition);
        histogramWatcher.assertExpected();
    }
}
