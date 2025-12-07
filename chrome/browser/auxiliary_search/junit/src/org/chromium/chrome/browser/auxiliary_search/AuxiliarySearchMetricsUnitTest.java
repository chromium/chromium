// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.CLICKED_ENTRY_POSITION;
import static org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.CLICKED_ENTRY_TYPE;
import static org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics.MAX_POSITION_INDEX;

import android.content.Intent;
import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link AuxiliarySearchMetrics} */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchMetricsUnitTest {

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE})
    public void testRecordDonationCount() {
        int[] counts = new int[AuxiliarySearchEntryType.MAX_VALUE + 1];
        counts[AuxiliarySearchEntryType.TAB] = 1;
        counts[AuxiliarySearchEntryType.CUSTOM_TAB] = 2;

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.DonationCount.Tabs", 1)
                        .expectIntRecord("Search.AuxiliarySearch.DonationCount.CustomTabs", 2)
                        .expectAnyRecordTimes("Search.AuxiliarySearch.DonationCount.TopSites", 0)
                        .build();

        AuxiliarySearchMetrics.recordDonationCount(counts);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testGetEntryTypeString() {
        assertTrue(
                TextUtils.equals(
                        ".Tabs",
                        AuxiliarySearchMetrics.getEntryTypeString(AuxiliarySearchEntryType.TAB)));
        assertTrue(
                TextUtils.equals(
                        ".CustomTabs",
                        AuxiliarySearchMetrics.getEntryTypeString(
                                AuxiliarySearchEntryType.CUSTOM_TAB)));
        assertTrue(
                TextUtils.equals(
                        ".TopSites",
                        AuxiliarySearchMetrics.getEntryTypeString(
                                AuxiliarySearchEntryType.TOP_SITE)));
    }

    @Test
    public void testMaybeRecordExternalAppClickInfo() {
        int position = 3;
        Intent intent = new Intent();
        intent.putExtra(CLICKED_ENTRY_TYPE, AuxiliarySearchEntryType.CUSTOM_TAB);
        intent.putExtra(CLICKED_ENTRY_POSITION, position);

        String externalAppName = "PixelLauncher";
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Search.AuxiliarySearch.LaunchedFromExternalApp.PixelLauncher.CustomTabs",
                                3)
                        .build();

        assertTrue(AuxiliarySearchMetrics.maybeRecordExternalAppClickInfo(externalAppName, intent));
        histogramWatcher.assertExpected();

        // Verifies the case with index is larger than MAX_INDEX.
        position = MAX_POSITION_INDEX + 5;
        intent.putExtra(CLICKED_ENTRY_POSITION, position);
        externalAppName = "ThirdPartyLauncher";
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Search.AuxiliarySearch.LaunchedFromExternalApp.ThirdPartyLauncher.CustomTabs",
                                9)
                        .build();

        assertTrue(AuxiliarySearchMetrics.maybeRecordExternalAppClickInfo(externalAppName, intent));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMaybeRecordExternalAppClickInfo_Failed() {
        int position = 3;
        Intent intent = new Intent();
        intent.putExtra(CLICKED_ENTRY_POSITION, position);

        // Verifies the case when no type is found in Intent's extras.
        String externalAppName = "PixelLauncher";
        assertFalse(
                AuxiliarySearchMetrics.maybeRecordExternalAppClickInfo(externalAppName, intent));

        // Verifies the case when a wrong type is found in Intent's extras.
        intent.putExtra(CLICKED_ENTRY_TYPE, AuxiliarySearchEntryType.MAX_VALUE + 5);
        assertFalse(
                AuxiliarySearchMetrics.maybeRecordExternalAppClickInfo(externalAppName, intent));

        // Verifies the case when no position is found in Intent's extras.
        intent.removeExtra(CLICKED_ENTRY_POSITION);
        intent.putExtra(CLICKED_ENTRY_TYPE, AuxiliarySearchEntryType.TOP_SITE);
        assertFalse(
                AuxiliarySearchMetrics.maybeRecordExternalAppClickInfo(externalAppName, intent));

        // Verifies a successful case.
        intent.putExtra(CLICKED_ENTRY_POSITION, position);
        assertTrue(AuxiliarySearchMetrics.maybeRecordExternalAppClickInfo(externalAppName, intent));
    }

    @Test
    public void testRecordTimeToCreateControllerInCustomTab() {
        long timeToCreateControllerMs = 1000L;
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Search.AuxiliarySearch.TimeToCreateControllerInCustomTab",
                                (int) timeToCreateControllerMs)
                        .build();

        AuxiliarySearchMetrics.recordTimeToCreateControllerInCustomTab(timeToCreateControllerMs);
        histogramWatcher.assertExpected();
    }
}
