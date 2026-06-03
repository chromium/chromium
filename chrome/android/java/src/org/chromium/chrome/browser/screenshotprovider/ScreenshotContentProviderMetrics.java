// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics related to ScreenshotContentProvider. */
@NullMarked
public class ScreenshotContentProviderMetrics {

    @IntDef({
        ScreenshotContentProviderEvent.REQUEST_STARTED,
        ScreenshotContentProviderEvent.REQUEST_FAILED_CURRENT_TAB_CHANGED,
        ScreenshotContentProviderEvent.REQUEST_FAILED_INVALID_URI,
        ScreenshotContentProviderEvent.REQUEST_FAILED_INVALID_ID,
        ScreenshotContentProviderEvent.REQUEST_FAILED_TO_GET_CURRENT_TAB,
        ScreenshotContentProviderEvent.REQUEST_FAILED_INVALID_ACTIVITY,
        ScreenshotContentProviderEvent.REQUEST_SUCCEEDED_RETURNED_CAPTURED,
        ScreenshotContentProviderEvent.REQUEST_FAILED_INVALID_MODE,
        ScreenshotContentProviderEvent.REQUEST_FAILED_EMPTY_RESULT,
        ScreenshotContentProviderEvent.REQUEST_FAILED_CURRENT_TAB_NULL_URL,
        ScreenshotContentProviderEvent.REQUEST_FAILED_INVALID_WINDOW_ANDROID,
        ScreenshotContentProviderEvent.REQUEST_FAILED_INTERRUPTED,
        ScreenshotContentProviderEvent.REQUEST_FAILED_TIMED_OUT,
        ScreenshotContentProviderEvent.REQUEST_FAILED_EXCEPTION,
        ScreenshotContentProviderEvent.REQUEST_FAILED_INVALID_CONTEXT,
        ScreenshotContentProviderEvent.REQUEST_FAILED_EMPTY_BITMAP,
        ScreenshotContentProviderEvent.REQUEST_FAILED_IO_EXCEPTION_WHEN_CREATING_OUTPUT_FILE,
        ScreenshotContentProviderEvent.REQUEST_FAILED_EXECUTION_EXCEPTION,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenshotContentProviderEvent {
        int REQUEST_STARTED = 0;
        int REQUEST_FAILED_CURRENT_TAB_CHANGED = 1;
        int REQUEST_FAILED_INVALID_URI = 2;
        int REQUEST_FAILED_INVALID_ID = 3;
        int REQUEST_FAILED_TO_GET_CURRENT_TAB = 4;
        int REQUEST_FAILED_INVALID_ACTIVITY = 5;
        int REQUEST_SUCCEEDED_RETURNED_CAPTURED = 6;
        int REQUEST_FAILED_INVALID_MODE = 7;
        int REQUEST_FAILED_EMPTY_RESULT = 8;
        int REQUEST_FAILED_CURRENT_TAB_NULL_URL = 9;
        int REQUEST_FAILED_INVALID_WINDOW_ANDROID = 10;
        int REQUEST_FAILED_INTERRUPTED = 11;
        int REQUEST_FAILED_TIMED_OUT = 12;
        int REQUEST_FAILED_EXCEPTION = 13;
        int REQUEST_FAILED_INVALID_CONTEXT = 14;
        int REQUEST_FAILED_EMPTY_BITMAP = 15;
        int REQUEST_FAILED_IO_EXCEPTION_WHEN_CREATING_OUTPUT_FILE = 16;
        int REQUEST_FAILED_EXECUTION_EXCEPTION = 17;
        int NUM_ENTRIES = 18;
    }

    @IntDef({
        ScreenshotUriProviderEvent.GET_CONTENT_URI_FAILED,
        ScreenshotUriProviderEvent.GET_CONTENT_URI_SUCCESS,
        ScreenshotUriProviderEvent.GET_CONTENT_URI_REUSED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenshotUriProviderEvent {
        int GET_CONTENT_URI_FAILED = 0;
        int GET_CONTENT_URI_SUCCESS = 1;
        int GET_CONTENT_URI_REUSED = 2;
        int NUM_ENTRIES = 3;
    }

    public static void recordScreenshotProviderEvent(@ScreenshotContentProviderEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ScreenshotContentProvider.Events",
                event,
                ScreenshotContentProviderEvent.NUM_ENTRIES);
    }

    /**
     * Records an event to the screenshot URI provider events histogram.
     *
     * @param event The ScreenshotUriProviderEvent to record.
     */
    public static void recordScreenshotUriProviderEvent(@ScreenshotUriProviderEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ScreenshotUriProvider.Events",
                event,
                ScreenshotUriProviderEvent.NUM_ENTRIES);
    }

    /**
     * Records the latency from state creation to screenshot capture start.
     *
     * @param duration Latency in milliseconds.
     */
    public static void recordCreateToCaptureStartLatency(long duration) {
        RecordHistogram.recordMediumTimesHistogram(
                "Android.ScreenshotContentProvider.Latency.CreateToCaptureStart", duration);
    }

    /**
     * Records the latency of the screenshot capture process itself.
     *
     * @param duration Latency in milliseconds.
     */
    public static void recordCaptureStartToEndLatency(long duration) {
        RecordHistogram.recordMediumTimesHistogram(
                "Android.ScreenshotContentProvider.Latency.CaptureStartToEnd", duration);
    }

    /**
     * Records the total latency from URI generation to the end of screenshot capture.
     *
     * @param duration Latency in milliseconds.
     */
    public static void recordTotalLatency(long duration) {
        RecordHistogram.recordMediumTimesHistogram(
                "Android.ScreenshotContentProvider.Latency.TotalLatency", duration);
    }
}
