// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A helper class to log long screenshots metrics.
 */
public class LongScreenshotsMetrics {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
            LongScreenshotsEvent.DIALOG_OPEN,
            LongScreenshotsEvent.DIALOG_CANCEL,
            LongScreenshotsEvent.DIALOG_OK,
            LongScreenshotsEvent.GENERATOR_CAPTURE_GENERATION_ERROR,
            LongScreenshotsEvent.GENERATOR_CAPTURE_INSUFFICIENT_MEMORY,
            LongScreenshotsEvent.GENERATOR_COMPOSITOR_CAPTURE_COMPLETE,
            LongScreenshotsEvent.GENERATOR_COMPOSITOR_MEMORY_PRESSURE,
            LongScreenshotsEvent.GENERATOR_COMPOSITOR_GENERATION_ERROR,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LongScreenshotsEvent {
        int DIALOG_OPEN = 0;
        int DIALOG_CANCEL = 1;
        int DIALOG_OK = 2;
        int GENERATOR_CAPTURE_GENERATION_ERROR = 3;
        int GENERATOR_CAPTURE_INSUFFICIENT_MEMORY = 4;
        int GENERATOR_COMPOSITOR_CAPTURE_COMPLETE = 5;
        int GENERATOR_COMPOSITOR_MEMORY_PRESSURE = 6;
        int GENERATOR_COMPOSITOR_GENERATION_ERROR = 7;
        int COUNT = 8;
    };

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // These should correspond to the Status enum in
    // chrome/browser/long_screenshots/long_screenshots_tab_service.h
    @IntDef({
            BitmapGenerationStatus.UNKNOWN,
            BitmapGenerationStatus.OK,
            BitmapGenerationStatus.DIRECTORY_CREATION_FAILED,
            BitmapGenerationStatus.CAPTURE_FAILED,
            BitmapGenerationStatus.PROTO_SERIALIZATION_FAILED,
            BitmapGenerationStatus.WEB_CONTENTS_GONE,
            BitmapGenerationStatus.NATIVE_SERVICE_UNINITIALIZED,
            BitmapGenerationStatus.LOW_MEMORY_DETECTED,
            BitmapGenerationStatus.PROTO_DESERIALIZATION_FAILED,
            BitmapGenerationStatus.NATIVE_SERVICE_NOT_INITIALIZED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BitmapGenerationStatus {
        int UNKNOWN = 0;
        int OK = 1;
        int DIRECTORY_CREATION_FAILED = 2;
        int CAPTURE_FAILED = 3;
        int PROTO_SERIALIZATION_FAILED = 4;
        int WEB_CONTENTS_GONE = 5;
        int NATIVE_SERVICE_UNINITIALIZED = 6;
        int LOW_MEMORY_DETECTED = 7;
        int PROTO_DESERIALIZATION_FAILED = 8;
        int NATIVE_SERVICE_NOT_INITIALIZED = 9;
        int COUNT = 10;
    };

    /**
     * A helper function to log long screenshots UI events.
     * @param action the action to be logged.
     */
    public static void logLongScreenshotsEvent(@LongScreenshotsEvent int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Sharing.LongScreenshots.Event", action, LongScreenshotsEvent.COUNT);
    }

    /**
     * A helper function to log long screenshots bitmap generation statuses.
     * @param status the bitmap generation status to be logged.
     */
    public static void logBitmapGenerationStatus(@BitmapGenerationStatus int status) {
        RecordHistogram.recordEnumeratedHistogram("Sharing.LongScreenshots.BitmapGenerationStatus",
                status, BitmapGenerationStatus.COUNT);
    }

    /**
     * Records the selected screenshot's region's height.
     * @param sizePx the height of the saved bitmap, in pixels.
     */
    public static void logBitmapSelectedHeightPx(int sizePx) {
        RecordHistogram.recordCount100000Histogram(
                "Sharing.LongScreenshots.BitmapSelectedHeight", sizePx);
    }
}