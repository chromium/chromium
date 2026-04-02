// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper class for recording metrics related to the App Rating Prompt. */
@NullMarked
public class AppRatingPromoMetrics {
    // Entries should not be renumbered and numeric values should never be reused.
    @IntDef({
        ReviewStatus.SUCCESS,
        ReviewStatus.PLAY_STORE_NOT_FOUND,
        ReviewStatus.INVALID_REQUEST,
        ReviewStatus.INTERNAL_ERROR,
        ReviewStatus.UNKNOWN_ERROR,
        ReviewStatus.MAX_VALUE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ReviewStatus {
        int SUCCESS = 0;
        int PLAY_STORE_NOT_FOUND = 1;
        int INVALID_REQUEST = 2;
        int INTERNAL_ERROR = 3;
        int UNKNOWN_ERROR = 4;
        int MAX_VALUE = 5;
    }

    /**
     * Records that we attempted to show the App Rating Prompt. This acts as the denominator for
     * funnel analysis, ensuring we can track silent failures where the Play Store API never returns
     * a callback.
     */
    public static void recordShowAttempted() {
        RecordHistogram.recordBooleanHistogram("Android.AppRatingPrompt.ShowAttempted", true);
    }

    /**
     * Records the status of the Play Store In-App Review flow.
     *
     * @param playErrorCode The error code returned by the Play Store API (0 for success). See error
     *     codes at: <a
     *     href="https://developer.android.com/reference/com/google/android/play/core/review/model/ReviewErrorCode">
     *     ReviewErrorCode </a>
     */
    public static void recordReviewStatus(int playErrorCode) {
        @ReviewStatus int status;
        switch (playErrorCode) {
            case 0: // NO_ERROR
                status = ReviewStatus.SUCCESS;
                break;
            case -1: // PLAY_STORE_NOT_FOUND
                status = ReviewStatus.PLAY_STORE_NOT_FOUND;
                break;
            case -2: // INVALID_REQUEST
                status = ReviewStatus.INVALID_REQUEST;
                break;
            case -100: // INTERNAL_ERROR
                status = ReviewStatus.INTERNAL_ERROR;
                break;
            default:
                status = ReviewStatus.UNKNOWN_ERROR;
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AppRatingPrompt.Status", status, ReviewStatus.MAX_VALUE);
    }

    /**
     * Records the duration a user spent interacting with the App Rating prompt.
     *
     * @param durationMs The time in milliseconds.
     */
    public static void recordInteractionDuration(long durationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                "Android.AppRatingPrompt.InteractionDuration", durationMs);
    }
}
