// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

public class ReadAloudMetrics {
    @VisibleForTesting public static String READABILITY_SUCCESS = "ReadAloud.IsPageReadable";

    public static void recordIsPageReadable(boolean successful) {
        RecordHistogram.recordBooleanHistogram(READABILITY_SUCCESS, successful);
    }
}
