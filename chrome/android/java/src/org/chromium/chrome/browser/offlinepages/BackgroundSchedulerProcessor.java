// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.device.DeviceConditions;

/** Class allowing for mocking out calls to BackgroundSchedulerBridge.  */
public class BackgroundSchedulerProcessor {
    private static BackgroundSchedulerProcessor sInstance;

    /** Returns a singleton instance. */
    public static BackgroundSchedulerProcessor getInstance() {
        if (sInstance == null) {
            sInstance = new BackgroundSchedulerProcessor();
        }
        return sInstance;
    }

    static void setInstanceForTesting(BackgroundSchedulerProcessor instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    /**
     * Starts processing of one or more queued background requests.  Returns whether processing was
     * started and that caller should expect a callback (once processing has completed or
     * terminated).  If processing was already active or not able to process for some other reason,
     * returns false and this calling instance will not receive a callback.
     */
    public boolean startScheduledProcessing(
            DeviceConditions deviceConditions, Callback<Boolean> callback) {
        return BackgroundSchedulerBridge.startScheduledProcessing(deviceConditions, callback);
    }

    /**
     * Stops processing background requests.
     * @return Whether processing should be scheduled again at a later time, because there is more
     * work.
     */
    public boolean stopScheduledProcessing() {
        return BackgroundSchedulerBridge.stopScheduledProcessing();
    }
}
