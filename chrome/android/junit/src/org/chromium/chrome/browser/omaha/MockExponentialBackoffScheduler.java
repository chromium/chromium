// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import static junit.framework.Assert.assertNotNull;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;

/**
 * Overrides the setAlarm function and allows changing the clock.
 */
public class MockExponentialBackoffScheduler extends ExponentialBackoffScheduler {
    private boolean mAlarmWasSet;
    private long mAlarmTimestamp;
    private long mCurrentTimestamp;

    public MockExponentialBackoffScheduler(
            String packageName, Context context, long baseMilliseconds, long maxMilliseconds) {
        super(packageName, context, baseMilliseconds, maxMilliseconds);
    }

    @Override
    protected void setAlarm(AlarmManager am, long timestamp, PendingIntent retryPIntent) {
        // Getting the Intent from the PendingIntent is not straightforward.
        // The delay is checked by another unit test.
        assertNotNull(am);
        assertNotNull(retryPIntent);
        mAlarmWasSet = true;
        mAlarmTimestamp = timestamp;
    }

    @Override
    public long getCurrentTime() {
        return mCurrentTimestamp;
    }

    public void setCurrentTime(long timestamp) {
        mCurrentTimestamp = timestamp;
    }

    public boolean getAlarmWasSet() {
        return mAlarmWasSet;
    }

    public long getAlarmTimestamp() {
        return mAlarmTimestamp;
    }
}