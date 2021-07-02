// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.proto.PriceTracking.PriceTrackingData;

import java.util.Locale;

/**
 * Records metrics to better understand and enhance our price drops feature
 */
public class PriceDropMetricsLogger {
    private MetricsResult mMetrics;

    /**
     * Log metrics related to our price drops feature
     * @param priceTrackingDataProto price tracking data proto acquired from OptimizationGuide to
     *         power the price drops experimence
     */
    PriceDropMetricsLogger(PriceTrackingData priceTrackingDataProto) {
        mMetrics = deriveMetrics(priceTrackingDataProto);
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

    private static MetricsResult deriveMetrics(PriceTrackingData priceTrackingDataProto) {
        return new MetricsResult(priceTrackingDataProto.hasBuyableProduct()
                        && priceTrackingDataProto.getBuyableProduct().hasOfferId(),
                priceTrackingDataProto.hasBuyableProduct()
                        && priceTrackingDataProto.getBuyableProduct().hasCurrentPrice(),
                priceTrackingDataProto.hasProductUpdate()
                        && priceTrackingDataProto.getProductUpdate().hasOldPrice()
                        && priceTrackingDataProto.getProductUpdate().hasNewPrice());
    }

    private static class MetricsResult {
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
