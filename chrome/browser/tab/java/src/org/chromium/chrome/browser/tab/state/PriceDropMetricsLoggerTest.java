// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

import java.util.concurrent.TimeUnit;

/** Test relating to {@link PriceDropMetricsLogger} */
@RunWith(BaseRobolectricTestRunner.class)
public class PriceDropMetricsLoggerTest {
    @Mock private ShoppingPersistedTabData mShoppingPersistedTabData;

    private PriceDropMetricsLogger mPriceDropMetricsLogger;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn("offer-id").when(mShoppingPersistedTabData).getMainOfferId();
        doReturn(true).when(mShoppingPersistedTabData).hasPriceMicros();
        doReturn(true).when(mShoppingPersistedTabData).hasPreviousPriceMicros();
        mPriceDropMetricsLogger = new PriceDropMetricsLogger(mShoppingPersistedTabData);
    }

    @SmallTest
    @Test
    public void testTabUsageStatus() {
        Assert.assertEquals(
                PriceDropMetricsLogger.TabUsageStatus.ABANDONED,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(100)));
        Assert.assertEquals(
                PriceDropMetricsLogger.TabUsageStatus.ABANDONED,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(90)));
        Assert.assertEquals(
                PriceDropMetricsLogger.TabUsageStatus.STALE,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(45)));
        Assert.assertEquals(
                PriceDropMetricsLogger.TabUsageStatus.STALE,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(1)));
        Assert.assertEquals(
                PriceDropMetricsLogger.TabUsageStatus.ACTIVE,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.HOURS.toMillis(12)));
    }

    @SmallTest
    @Test
    public void testMetricsStaleTabNavigation() {
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "Commerce.PriceDrops.StaleTabNavigationComplete.IsProductDetailPage")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.StaleTabNavigationComplete.ContainsPrice")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.StaleTabNavigationComplete.ContainsPriceDrop")
                        .build();
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "NavigationComplete", TimeUnit.DAYS.toMillis(45));
        histograms.assertExpected();
    }

    @SmallTest
    @Test
    public void testMetrics2StaleTabEnterTabSwitcher() {
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "Commerce.PriceDrops.StaleTabEnterTabSwitcher.IsProductDetailPage")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.StaleTabEnterTabSwitcher.ContainsPrice")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.StaleTabEnterTabSwitcher.ContainsPriceDrop")
                        .build();
        mPriceDropMetricsLogger.logPriceDropMetrics("EnterTabSwitcher", TimeUnit.DAYS.toMillis(45));
        histograms.assertExpected();
    }

    @SmallTest
    @Test
    public void testMetricsActiveTabNavigationComplete() {
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabNavigationComplete.IsProductDetailPage")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabNavigationComplete.ContainsPrice")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabNavigationComplete.ContainsPriceDrop")
                        .build();
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "NavigationComplete", TimeUnit.HOURS.toMillis(12));
        histograms.assertExpected();
    }

    @SmallTest
    @Test
    public void testMetricsActiveTabEnterTabSwitcher() {
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop")
                        .build();
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "EnterTabSwitcher", TimeUnit.HOURS.toMillis(12));
        histograms.assertExpected();
    }

    @SmallTest
    @Test
    public void testMetricsPriceNoPriceDrop() {
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice")
                        .expectAnyRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop")
                        .build();
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "EnterTabSwitcher", TimeUnit.HOURS.toMillis(12));
        histograms.assertExpected();
    }

    @SmallTest
    @Test
    public void testEmptyPriceDropResponse() {
        doReturn(null).when(mShoppingPersistedTabData).getMainOfferId();
        doReturn(false).when(mShoppingPersistedTabData).hasPriceMicros();
        doReturn(false).when(mShoppingPersistedTabData).hasPreviousPriceMicros();
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage",
                                false)
                        .expectBooleanRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice",
                                false)
                        .expectBooleanRecord(
                                "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop",
                                false)
                        .build();
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "EnterTabSwitcher", TimeUnit.HOURS.toMillis(12));
        histograms.assertExpected();
    }
}
