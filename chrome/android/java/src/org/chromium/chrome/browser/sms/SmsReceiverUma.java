// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper Class for Sms Receiver UMA Collection.
 */
final class SmsReceiverUma {
    // Note: these values must match the SMSReceiverInfobar enum in enums.xml.
    // Only add new values at the end, right before NUM_ENTRIES.
    @IntDef({InfobarAction.SHOWN, InfobarAction.KEYBOARD_DISMISSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface InfobarAction {
        int SHOWN = 0;
        int KEYBOARD_DISMISSED = 1;
        int NUM_ENTRIES = 2;
    }

    static void recordInfobarAction(int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Blink.Sms.Receive.Infobar", action, InfobarAction.NUM_ENTRIES);
    }

    static void recordCancelTimeAfterKeyboardDismissal(long durationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                "Blink.Sms.Receive.TimeCancelOnKeyboardDismissal", durationMs);
    }
}
