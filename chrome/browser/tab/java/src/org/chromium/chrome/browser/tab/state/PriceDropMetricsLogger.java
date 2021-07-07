// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

import java.util.Locale;

/**
 * Records metrics to better understand and enhance our price drops feature
 */
public class PriceDropMetricsLogger {
    private MetricsResult mMetrics;

    /**
     * Log metrics related to our price drops feature.
     * @param shoppingPersistedTabData {@link ShoppingPersistedTabData} associated with price drop
     *         data.
     */
    PriceDropMetricsLogger(ShoppingPersistedTabData shoppingPersistedTabData) {
        mMetrics = deriveMetrics(shoppingPersistedTabData);
    }

    /**
     * Log metrics related to the price drops feature
     * @param locationIdentifier to be placed in the metric name (these metrics are recorded in
     *         different places in the user experience)
     */
    public void logPriceDropMetrics(String locationIdentifier) {
        RecordHistogram.recordBooleanHistogram(
                String.format(Locale.US, "Commerce.PriceDrops.%s.IsProductDetailPage",
                        locationIdentifier),
                mMetrics.isProductDetailPage);
        RecordHistogram.recordBooleanHistogram(
                String.format(
                        Locale.US, "Commerce.PriceDrops.%s.ContainsPrice", locationIdentifier),
                mMetrics.containsPrice);
        RecordHistogram.recordBooleanHistogram(
                String.format(
                        Locale.US, "Commerce.PriceDrops.%s.ContainsPriceDrop", locationIdentifier),
                mMetrics.containsPriceDrop);
    }

    @VisibleForTesting
    protected MetricsResult getMetricsResultForTesting() {
        return mMetrics;
    }

    private static MetricsResult deriveMetrics(ShoppingPersistedTabData shoppingPersistedTabData) {
        return new MetricsResult(!TextUtils.isEmpty(shoppingPersistedTabData.getMainOfferId()),
                shoppingPersistedTabData.hasPriceMicros(),
                shoppingPersistedTabData.hasPriceMicros()
                        && shoppingPersistedTabData.hasPreviousPriceMicros());
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
}
