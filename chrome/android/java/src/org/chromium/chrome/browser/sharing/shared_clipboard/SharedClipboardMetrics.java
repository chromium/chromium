// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.shared_clipboard;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.sharing.SharingDialogType;

/**
 * Helper Class for Shared Clipboard metrics.
 */
public class SharedClipboardMetrics {
    public static void recordDeviceCount(int count) {
        RecordHistogram.recordLinearCountHistogram("Sharing.SharedClipboardDevicesToShow", count,
                /*min=*/1, /*max=*/20, /*num buckets=*/21);
    }

    public static void recordDeviceClick(int index) {
        RecordHistogram.recordLinearCountHistogram("Sharing.SharedClipboardSelectedDeviceIndex",
                index, /*min=*/1, /*max=*/20, /*num buckets=*/21);
    }

    public static void recordShowDeviceList() {
        RecordHistogram.recordEnumeratedHistogram("Sharing.SharedClipboardDialogShown",
                SharingDialogType.DIALOG_WITH_DEVICES_MAYBE_APPS, SharingDialogType.MAX_VALUE);
    }

    public static void recordShowEducationalDialog() {
        RecordHistogram.recordEnumeratedHistogram("Sharing.SharedClipboardDialogShown",
                SharingDialogType.EDUCATIONAL_DIALOG, SharingDialogType.MAX_VALUE);
    }

    /*
     * Records the size of the selected text in Shared Clipboard with the same number of buckets as
     * Desktop.
     */
    public static void recordTextSize(int textSize) {
        RecordHistogram.recordCount100000Histogram(
                "Sharing.SharedClipboardSelectedTextSize", textSize);
    }
}
