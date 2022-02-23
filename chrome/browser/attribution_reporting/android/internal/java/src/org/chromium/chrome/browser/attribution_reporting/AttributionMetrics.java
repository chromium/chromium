// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.NativeMethods;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Manages Metrics for the attribution_reporting module.
 */
public class AttributionMetrics {
    public static final String ATTRIBUTION_EVENTS_NAME = "Conversions.AppToWeb.AttributionEvents";

    // It's safe to cache histogram pointers because they're never freed.
    private static long sNativeAttibutionEventsPtr;

    // These values are used for histograms, do not reorder.
    @IntDef({AttributionEvent.RECEIVED_WITH_NATIVE, AttributionEvent.CACHED_PRE_NATIVE,
            AttributionEvent.REPORTED_POST_NATIVE, AttributionEvent.DROPPED_STORAGE_FULL,
            AttributionEvent.DROPPED_READ_FAILED, AttributionEvent.DROPPED_WRITE_FAILED,
            AttributionEvent.FILE_CLOSE_FAILED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AttributionEvent {
        int RECEIVED_WITH_NATIVE = 0;
        int CACHED_PRE_NATIVE = 1;
        int REPORTED_POST_NATIVE = 2;
        int DROPPED_STORAGE_FULL = 3;
        int DROPPED_WRITE_FAILED = 4;
        int DROPPED_READ_FAILED = 5;
        int FILE_CLOSE_FAILED = 6;

        int NUM_ENTRIES = 7;
    }

    public static boolean isValidAttributionEventMetric(String metricName, int enumValue) {
        return AttributionMetrics.ATTRIBUTION_EVENTS_NAME.equals(metricName)
                && enumValue < AttributionEvent.NUM_ENTRIES;
    }

    public static void recordAttributionEvent(@AttributionEvent int event, int count) {
        sNativeAttibutionEventsPtr =
                AttributionMetricsJni.get().recordEnumMetrics(ATTRIBUTION_EVENTS_NAME,
                        sNativeAttibutionEventsPtr, event, AttributionEvent.NUM_ENTRIES, count);
    }

    @NativeMethods
    interface Natives {
        long recordEnumMetrics(String metricName, long nativeMetricPtr, int enumValue,
                int exclusiveMax, int count);
    }
}
