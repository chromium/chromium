// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper Class for GCM UMA Collection.
 */
public class GcmUma {
    // Keep in sync with the WebPushDeviceState enum in enums.xml.
    @IntDef({WebPushDeviceState.NOT_IDLE_NOT_HIGH_PRIORITY,
            WebPushDeviceState.NOT_IDLE_HIGH_PRIORITY, WebPushDeviceState.IDLE_NOT_HIGH_PRIORITY,
            WebPushDeviceState.IDLE_HIGH_PRIORITY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface WebPushDeviceState {
        int NOT_IDLE_NOT_HIGH_PRIORITY = 0;
        int NOT_IDLE_HIGH_PRIORITY = 1;
        int IDLE_NOT_HIGH_PRIORITY = 2;
        int IDLE_HIGH_PRIORITY = 3;
        int NUM_ENTRIES = 4;
    }

    public static void recordDataMessageReceived(Context context, final boolean hasCollapseKey) {
        // There is no equivalent of the GCM Store on Android in which we can fail to find a
        // registered app. It's not clear whether Google Play Services doesn't check for
        // registrations, or only gives us messages that have one, but in either case we
        // should log true here.
        RecordHistogram.recordBooleanHistogram("GCM.DataMessageReceivedHasRegisteredApp", true);
        RecordHistogram.recordCount1MHistogram("GCM.DataMessageReceived", 1);
        RecordHistogram.recordBooleanHistogram(
                "GCM.DataMessageReceivedHasCollapseKey", hasCollapseKey);
    }

    public static void recordDeletedMessages(Context context) {
        RecordHistogram.recordCount1000Histogram(
                "GCM.DeletedMessagesReceived", 0 /* unknown deleted count */);
    }

    public static void recordWebPushReceivedDeviceState(@WebPushDeviceState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "GCM.WebPushReceived.DeviceState", state, WebPushDeviceState.NUM_ENTRIES);
    }
}
