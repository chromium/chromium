// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

import java.util.Locale;
import java.util.concurrent.TimeUnit;

/** Records metrics to better understand and enhance our price drops feature */
public class PriceDropMetricsLogger {
    private static final long NINETY_DAYS_MS = TimeUnit.DAYS.toMillis(90);
    private static final long ONE_DAY_MS = TimeUnit.DAYS.toMillis(1);

    private ShoppingPersistedTabData mShoppingPersistedTabData;

    @VisibleForTesting
    protected enum TabUsageStatus {
        ABANDONED("AbandonedTab"),
        STALE("StaleTab"),
        ACTIVE("ActiveTab");

        private final String mTabUsageStatus;

        TabUsageStatus(String tabUsageStatus) {
            mTabUsageStatus = tabUsageStatus;
        }

        @Override
        public String toString() {
            return mTabUsageStatus;
        }
    }

    /**
     * Log metrics related to our price drops feature.
     * @param shoppingPersistedTabData {@link ShoppingPersistedTabData} associated with price drop
     *         data.
     */
    PriceDropMetricsLogger(ShoppingPersistedTabData shoppingPersistedTabData) {
        mShoppingPersistedTabData = shoppingPersistedTabData;
    }

    /**
     * Log metrics related to the price drops feature
     * @param locationIdentifier to be placed in the metric name (these metrics are recorded in
     *         different places in the user experience).
     * @param timeSinceTabLastOpenedMs time since the tab was last opened in milliseconds.
     */
    public void logPriceDropMetrics(String locationIdentifier, long timeSinceTabLastOpenedMs) {
        TabUsageStatus tabUsageStatus = getTabUsageStatus(timeSinceTabLastOpenedMs);
        // Tabs greater than 90 days old are not included in price drops, so the following shouldn't
        // happen but is included as a safeguard.
        if (tabUsageStatus == TabUsageStatus.ABANDONED) {
            return;
        }
        MetricsResult metrics = deriveMetrics();
        RecordHistogram.recordBooleanHistogram(
                String.format(
                        Locale.US,
                        "Commerce.PriceDrops.%s%s.IsProductDetailPage",
                        tabUsageStatus,
                        locationIdentifier),
                metrics.isProductDetailPage);
        RecordHistogram.recordBooleanHistogram(
                String.format(
                        Locale.US,
                        "Commerce.PriceDrops.%s%s.ContainsPrice",
                        tabUsageStatus,
                        locationIdentifier),
                metrics.containsPrice);
        RecordHistogram.recordBooleanHistogram(
                String.format(
                        Locale.US,
                        "Commerce.PriceDrops.%s%s.ContainsPriceDrop",
                        tabUsageStatus,
                        locationIdentifier),
                metrics.containsPriceDrop);
    }

    protected MetricsResult getMetricsResultForTesting() {
        return deriveMetrics();
    }

    private MetricsResult deriveMetrics() {
        return new MetricsResult(
                !TextUtils.isEmpty(mShoppingPersistedTabData.getMainOfferId()),
                mShoppingPersistedTabData.hasPriceMicros(),
                mShoppingPersistedTabData.hasPriceMicros()
                        && mShoppingPersistedTabData.hasPreviousPriceMicros());
    }

    @VisibleForTesting
    protected static TabUsageStatus getTabUsageStatus(long timeSinceTabLastOpenedMs) {
        if (timeSinceTabLastOpenedMs >= NINETY_DAYS_MS) {
            return TabUsageStatus.ABANDONED;
        }
        return timeSinceTabLastOpenedMs < ONE_DAY_MS ? TabUsageStatus.ACTIVE : TabUsageStatus.STALE;
    }

    @VisibleForTesting
    protected static class MetricsResult {
        public final boolean isProductDetailPage;
        public final boolean containsPrice;
        public final boolean containsPriceDrop;

        public MetricsResult(
                boolean isProductDetailPage, boolean containsPrice, boolean containsPriceDrop) {
            this.isProductDetailPage = isProductDetailPage;
            this.containsPrice = containsPrice;
            this.containsPriceDrop = containsPriceDrop;
        }
    }

    public void destroy() {
        mShoppingPersistedTabData = null;
    }
}
