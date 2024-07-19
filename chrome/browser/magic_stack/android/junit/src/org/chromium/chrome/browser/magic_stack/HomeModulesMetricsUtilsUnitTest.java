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
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

/** Unit tests for {@link HomeModulesMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesMetricsUtilsUnitTest {
    @Test
    @SmallTest
    public void testRecordModuleShown() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int modulePosition = 2;
        String histogramName = "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";
        String histogramNameWithPosition =
                "MagicStack.Clank.NewTabPage.Regular.Module.SingleTab.Impression";
        String histogramNameStartupWithPosition =
                "MagicStack.Clank.NewTabPage.Startup.Module.SingleTab.Impression";

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(histogramName, moduleType)
                        .expectIntRecord(histogramNameWithPosition, modulePosition)
                        .build();
        HomeModulesMetricsUtils.recordModuleShown(
                moduleType, modulePosition, /* isShownAtStartup= */ false);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(histogramName, moduleType)
                        .expectIntRecord(histogramNameStartupWithPosition, modulePosition)
                        .build();
        HomeModulesMetricsUtils.recordModuleShown(
                moduleType, modulePosition, /* isShownAtStartup= */ true);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuShown() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName = "MagicStack.Clank.NewTabPage.ContextMenu.ShownV2";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuShown(moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuRemoveModule() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName = "MagicStack.Clank.NewTabPage.ContextMenu.RemoveModuleV2";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuRemoveModule(moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordContextMenuCustomizeSettings() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName = "MagicStack.Clank.NewTabPage.ContextMenu.OpenCustomizeSettings";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordContextMenuCustomizeSettings(moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFetchDataDuration() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int duration = 100;

        String histogramName = "MagicStack.Clank.NewTabPage.Module.FetchDataDurationMs.SingleTab";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataDuration(moduleType, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFetchDataTimeoutDuration() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int duration = 100;

        String histogramName =
                "MagicStack.Clank.NewTabPage.Module.FetchDataTimeoutDurationMs.SingleTab";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataTimeOutDuration(moduleType, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFetchDataTimeoutType() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        String histogramName = "MagicStack.Clank.NewTabPage.Module.FetchDataTimeoutTypeV2";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        HomeModulesMetricsUtils.recordFetchDataTimeOutType(moduleType);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFetchDataFailedDuration() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int duration = 100;

        String histogramName =
                "MagicStack.Clank.NewTabPage.Module.FetchDataFailedDurationMs.SingleTab";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFetchDataFailedDuration(moduleType, duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordFirstModuleShowDuration() {
        int duration = 100;
        String histogramName = "MagicStack.Clank.NewTabPage.Module.FirstModuleShownDurationMs";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordFirstModuleShownDuration(duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordProfileReadyDelay() {
        int duration = 100;
        String histogramName = "MagicStack.Clank.NewTabPage.Module.ProfileReadyDelayMs";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordProfileReadyDelay(duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordSegmentationFetchRankingDuration() {
        int duration = 100;
        String histogramName =
                "MagicStack.Clank.NewTabPage.Segmentation.FetchRankingResultsDurationMs";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, duration).build();
        HomeModulesMetricsUtils.recordSegmentationFetchRankingDuration(duration);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordModuleClicked() {
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int modulePosition = 2;

        String histogramName = "MagicStack.Clank.NewTabPage.Module.Click";
        String histogramNameHomeSurface = "NewTabPage.Module.Click";
        String histogramNameWithPosition =
                "MagicStack.Clank.NewTabPage.Regular.Module.SingleTab.Click";
        String histogramNameStartupWithPosition =
                "MagicStack.Clank.NewTabPage.Startup.Module.SingleTab.Click";

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, moduleType)
                        .expectIntRecord(
                                histogramNameHomeSurface, ModuleTypeOnStartAndNtp.MAGIC_STACK)
                        .expectIntRecords(histogramNameWithPosition, modulePosition)
                        .build();
        HomeModulesMetricsUtils.recordModuleClicked(
                moduleType, modulePosition, /* isShownAtStartup= */ false);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, moduleType)
                        .expectIntRecord(
                                histogramNameHomeSurface, ModuleTypeOnStartAndNtp.MAGIC_STACK)
                        .expectIntRecords(histogramNameStartupWithPosition, modulePosition)
                        .build();
        HomeModulesMetricsUtils.recordModuleClicked(
                moduleType, modulePosition, /* isShownAtStartup= */ true);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordHomeModulesScrollState() {
        boolean isScrollable = true;
        boolean isScrolled = true;
        String histogramName = "MagicStack.Clank.NewTabPage.Scrollable.Scrolled";

        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, 1);
        HomeModulesMetricsUtils.recordHomeModulesScrollState(isScrollable, isScrolled);
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
        @ModuleType int moduleType = ModuleType.SINGLE_TAB;
        int modulePosition = 2;

        String histogramName = "MagicStack.Clank.NewTabPage.Regular.Module.SingleTab.Build";
        String histogramNameStartup = "MagicStack.Clank.NewTabPage.Startup.Module.SingleTab.Build";

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, modulePosition);
        HomeModulesMetricsUtils.recordModuleBuiltPosition(
                moduleType, modulePosition, /* isShownAtStartup= */ false);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramNameStartup, modulePosition);
        HomeModulesMetricsUtils.recordModuleBuiltPosition(
                moduleType, modulePosition, /* isShownAtStartup= */ true);
        histogramWatcher.assertExpected();
    }
}
