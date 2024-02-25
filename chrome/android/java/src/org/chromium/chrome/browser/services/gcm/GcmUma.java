// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper Class for GCM UMA Collection. */
public class GcmUma {
    // Keep in sync with the WebPushDeviceState enum in enums.xml.
    @IntDef({
        WebPushDeviceState.NOT_IDLE_NOT_HIGH_PRIORITY,
        WebPushDeviceState.NOT_IDLE_HIGH_PRIORITY,
        WebPushDeviceState.IDLE_NOT_HIGH_PRIORITY,
        WebPushDeviceState.IDLE_HIGH_PRIORITY
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WebPushDeviceState {
        int NOT_IDLE_NOT_HIGH_PRIORITY = 0;
        int NOT_IDLE_HIGH_PRIORITY = 1;
        int IDLE_NOT_HIGH_PRIORITY = 2;
        int IDLE_HIGH_PRIORITY = 3;
        int NUM_ENTRIES = 4;
    }

    public static void recordWebPushReceivedDeviceState(@WebPushDeviceState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "GCM.WebPushReceived.DeviceState", state, WebPushDeviceState.NUM_ENTRIES);
    }
}
