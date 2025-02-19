// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/** Metrics utils for use in power bookmarks. */
public class PowerBookmarkMetrics {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. Keep up-to-date with the PriceTrackingState enum in
    // tools/metrics/histograms/enums.xml.
    @IntDef({
        PriceTrackingState.PRICE_TRACKING_SHOWN,
        PriceTrackingState.PRICE_TRACKING_ENABLED,
        PriceTrackingState.PRICE_TRACKING_DISABLED,
        PriceTrackingState.COUNT
    })
    public @interface PriceTrackingState {
        int PRICE_TRACKING_SHOWN = 0;
        int PRICE_TRACKING_ENABLED = 1;
        int PRICE_TRACKING_DISABLED = 2;
        int COUNT = 3;
    }

    /** Report the price tracking state for the bookmark save flow surface. */
    public static void reportBookmarkSaveFlowPriceTrackingState(@PriceTrackingState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "PowerBookmarks.BookmarkSaveFlow.PriceTrackingEnabled",
                state,
                PriceTrackingState.COUNT);
    }

    /** Report the price tracking state for the bookmark shopping item row. */
    public static void reportBookmarkShoppingItemRowPriceTrackingState(
            @PriceTrackingState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "PowerBookmarks.BookmarkManager.PriceTrackingEnabled",
                state,
                PriceTrackingState.COUNT);
    }
}
