// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that captures all the metrics needed for Send Tab To Self on Android.
 */
class SendTabToSelfMetrics {
    /**
     * Metrics captured when a user initiates and completes the sending flow.
     */
    static class SendTabToSelfShareClickResult {
        @Retention(RetentionPolicy.SOURCE)
        @IntDef({ClickType.SHOW_ITEM, ClickType.CLICK_ITEM, ClickType.SHOW_DEVICE_LIST})
        @interface ClickType {
            // These values are used for UMA. Don't reuse or reorder values.
            // If you add something, update NUM_ENTRIES.This must be kept in sync with
            // send_tab_to_self_desktop_util.h
            int SHOW_ITEM = 0;
            int CLICK_ITEM = 1;
            int SHOW_DEVICE_LIST = 2;
            int NUM_ENTRIES = 3;
        }

        static void recordClickResult(@ClickType int result) {
            RecordHistogram.recordEnumeratedHistogram(
                    "SendTabToSelf.AndroidShareSheet.ClickResult", result, ClickType.NUM_ENTRIES);
        }
    }
}
