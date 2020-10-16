// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Class that contains helper functions for download location download feature metrics recording.
 */
public final class DownloadLocationDialogMetrics {
    private DownloadLocationDialogMetrics() {}

    /**
     * Records the user choice on the locaction suggestion spinner.
     * @param isChosen The user choice, true if the user chooses the suggestion.
     */
    public static void recordDownloadLocationSuggestionChoice(boolean isSelected) {
        RecordHistogram.recordBooleanHistogram(
                "MobileDownload.Location.Dialog.SuggestionSelected", isSelected);
    }
}
