// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import org.chromium.base.metrics.RecordHistogram;
/** A helper class for creating and saving histograms related to update success. */
class HistogramUtils {
    /**
     * Records a histogram for whether or an update is running at the time an update starts.
     * @param alreadyUpdating Whether or not an update is currently tracked as running.
     */
    public static void recordStartedUpdateHistogram(boolean alreadyUpdating) {
        RecordHistogram.recordBooleanHistogram("GoogleUpdate.StartingUpdateState", alreadyUpdating);
    }
}
