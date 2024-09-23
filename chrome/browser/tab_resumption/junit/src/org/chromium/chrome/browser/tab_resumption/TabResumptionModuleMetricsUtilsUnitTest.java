// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ClickInfo;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;

/** Unit tests for {@link TabResumptionModuleMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabResumptionModuleMetricsUtilsUnitTest extends TestSupportExtended {
    @Test
    @SmallTest
    public void testRecordSalientImageAvailability() {
        String histogramName = "MagicStack.Clank.TabResumption.IsSalientImageAvailable";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectBooleanRecord(histogramName, true).build();
        TabResumptionModuleMetricsUtils.recordSalientImageAvailability(true);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder().expectBooleanRecord(histogramName, false).build();
        TabResumptionModuleMetricsUtils.recordSalientImageAvailability(false);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecord() {
        String histogramName = "MagicStack.Clank.TabResumption.SeeMoreLinkClicked";
        @ModuleShowConfig int config = ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN;
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, config).build();
        TabResumptionModuleMetricsUtils.recordSeeMoreLinkClicked(config);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testComputeModuleShowConfig() {
        SuggestionBundle bundle = new SuggestionBundle(CURRENT_TIME_MS);
        int tabId = 10;
        SuggestionEntry localEntry = createLocalSuggestion(tabId);
        SuggestionEntry foreignEntry =
                SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB6);
        SuggestionEntry foreignEntry1 = SuggestionEntry.createFromForeignSessionTab("Maps", TAB5);
        SuggestionEntry historyEntry = createHistorySuggestion(/* needMatchLocalTab= */ false);

        bundle.entries.add(localEntry);
        assertEquals(
                (Integer) ModuleShowConfig.SINGLE_TILE_LOCAL,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(foreignEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(1, localEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_LOCAL_LOCAL,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(1, historyEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.clear();
        bundle.entries.add(foreignEntry);
        assertEquals(
                (Integer) ModuleShowConfig.SINGLE_TILE_FOREIGN,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(foreignEntry1);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_FOREIGN_FOREIGN,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(1, localEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(1, historyEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_FOREIGN_HISTORY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.clear();
        bundle.entries.add(historyEntry);
        assertEquals(
                (Integer) ModuleShowConfig.SINGLE_TILE_HISTORY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(foreignEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_FOREIGN_HISTORY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(1, localEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(1, historyEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_HISTORY_HISTORY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));
    }

    @Test
    @SmallTest
    public void testComputeModuleShowConfig_WithNotFinalizedSuggestion() {
        SuggestionBundle bundle = new SuggestionBundle(CURRENT_TIME_MS);
        int tabId = 10;
        SuggestionEntry localEntry = createLocalSuggestion(tabId);
        SuggestionEntry foreignEntry =
                SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB6);
        SuggestionEntry foreignEntryNotFinalized =
                createForeignSuggestion(/* needMatchLocalTab= */ true);
        SuggestionEntry historyEntryNotFinalized =
                createHistorySuggestion(/* needMatchLocalTab= */ true);

        bundle.entries.add(localEntry);
        assertEquals(
                (Integer) ModuleShowConfig.SINGLE_TILE_LOCAL,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.clear();
        bundle.entries.add(historyEntryNotFinalized);
        assertEquals(
                (Integer) ModuleShowConfig.SINGLE_TILE_ANY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(foreignEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_ANY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.clear();
        bundle.entries.add(foreignEntryNotFinalized);
        assertEquals(
                (Integer) ModuleShowConfig.SINGLE_TILE_ANY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));

        bundle.entries.add(localEntry);
        assertEquals(
                (Integer) ModuleShowConfig.DOUBLE_TILE_ANY,
                TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));
    }

    @Test
    @SmallTest
    public void testComputeClickInfo() {
        int tabId = 10;
        SuggestionEntry localEntry = createLocalSuggestion(tabId);
        SuggestionEntry foreignEntry =
                SuggestionEntry.createFromForeignSessionTab("My Tablet", TAB6);
        SuggestionEntry foreignEntryNotFinalized =
                createForeignSuggestion(/* needMatchLocalTab= */ true);
        SuggestionEntry historyEntry = createHistorySuggestion(/* needMatchLocalTab= */ false);
        SuggestionEntry historyEntryNotFinalized =
                createHistorySuggestion(/* needMatchLocalTab= */ true);

        // Cases of size = 1:
        assertEquals(
                ClickInfo.LOCAL_SINGLE_FIRST,
                TabResumptionModuleMetricsUtils.computeClickInfo(localEntry, 1));

        // Verifies that the same ClickInfo is returned for both finalized and not finalized tiles.
        assertEquals(
                ClickInfo.FOREIGN_SINGLE_FIRST,
                TabResumptionModuleMetricsUtils.computeClickInfo(foreignEntry, 1));
        assertEquals(
                ClickInfo.FOREIGN_SINGLE_FIRST,
                TabResumptionModuleMetricsUtils.computeClickInfo(foreignEntryNotFinalized, 1));

        assertEquals(
                ClickInfo.HISTORY_SINGLE_FIRST,
                TabResumptionModuleMetricsUtils.computeClickInfo(historyEntry, 1));
        assertEquals(
                ClickInfo.HISTORY_SINGLE_FIRST,
                TabResumptionModuleMetricsUtils.computeClickInfo(historyEntryNotFinalized, 1));

        // Cases of size = 2:
        assertEquals(
                ClickInfo.LOCAL_DOUBLE_ANY,
                TabResumptionModuleMetricsUtils.computeClickInfo(localEntry, 2));

        assertEquals(
                ClickInfo.FOREIGN_DOUBLE_ANY,
                TabResumptionModuleMetricsUtils.computeClickInfo(foreignEntry, 2));
        assertEquals(
                ClickInfo.FOREIGN_DOUBLE_ANY,
                TabResumptionModuleMetricsUtils.computeClickInfo(foreignEntryNotFinalized, 2));

        assertEquals(
                ClickInfo.HISTORY_DOUBLE_ANY,
                TabResumptionModuleMetricsUtils.computeClickInfo(historyEntry, 2));
        assertEquals(
                ClickInfo.HISTORY_DOUBLE_ANY,
                TabResumptionModuleMetricsUtils.computeClickInfo(historyEntryNotFinalized, 2));
    }
}
