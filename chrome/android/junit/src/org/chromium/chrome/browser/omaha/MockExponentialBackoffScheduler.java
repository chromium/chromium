// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;

/**
 * Allows changing the clock.
 */
public class MockExponentialBackoffScheduler extends ExponentialBackoffScheduler {
    private long mCurrentTimestamp;

    public MockExponentialBackoffScheduler(
            String packageName, Context context, long baseMilliseconds, long maxMilliseconds) {
        super(packageName, context, baseMilliseconds, maxMilliseconds);
    }

    @Override
    public long getCurrentTime() {
        return mCurrentTimestamp;
    }

    public void setCurrentTime(long timestamp) {
        mCurrentTimestamp = timestamp;
    }
}
