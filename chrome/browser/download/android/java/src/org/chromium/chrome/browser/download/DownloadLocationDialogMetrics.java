// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Class that contains helper functions for download location download feature metrics recording.
 */
public final class DownloadLocationDialogMetrics {
    /**
     * Defines events for download location suggestion feature. Used in histograms, don't reuse or
     * remove items. Keep in sync with DownloadLocationSuggestionEvent in enums.xml.
     */
    @IntDef({
        DownloadLocationSuggestionEvent.LOCATION_SUGGESTION_SHOWN,
        DownloadLocationSuggestionEvent.NOT_ENOUGH_SPACE_SHOWN,
    })
    public @interface DownloadLocationSuggestionEvent {
        int LOCATION_SUGGESTION_SHOWN = 0;
        int NOT_ENOUGH_SPACE_SHOWN = 1;

        int COUNT = 2;
    }

    private DownloadLocationDialogMetrics() {}

    /**
     * Records the user choice on the locaction suggestion spinner.
     * @param isChosen The user choice, true if the user chooses the suggestion.
     */
    public static void recordDownloadLocationSuggestionChoice(boolean isSelected) {
        RecordHistogram.recordBooleanHistogram(
                "MobileDownload.Location.Dialog.SuggestionSelected", isSelected);
    }

    /**
     * Collects download location suggestion event metrics.
     * @param event The event to collect.
     */
    public static void recordDownloadLocationSuggestionEvent(
            @DownloadLocationSuggestionEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.Location.Dialog.Suggestion.Events",
                event,
                DownloadLocationSuggestionEvent.COUNT);
    }
}
